#include "binance/feed_handler.hpp"
#include "binance/endpoints.hpp"
#include "binance/message_parser.hpp"
#include <spdlog/spdlog.h>

namespace titan::binance {

FeedHandler::FeedHandler(
    boost::asio::io_context& ioc,
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx,
    const Config& config,
    MessageCallback on_message
)
    : ioc_(ioc)
    , ssl_ctx_(std::move(ssl_ctx))
    , config_(config)
    , on_message_(std::move(on_message))
    , reconnect_timer_(ioc)
    , reconnect_strategy_(
        config.network.reconnect_delay_initial,
        config.network.reconnect_delay_max,
        config.network.reconnect_backoff_multiplier,
        config.network.reconnect_jitter_factor
    )
{}

FeedHandler::~FeedHandler() {
    stop();
}

void FeedHandler::start() {
    spdlog::info("FeedHandler starting for {}", config_.network.symbol);
    connect();
}

void FeedHandler::stop() {
    spdlog::info("FeedHandler stopping");
    set_state(FeedState::Disconnected);

    reconnect_timer_.cancel();

    if (ws_client_) {
        ws_client_->close();
        ws_client_.reset();
    }
}

void FeedHandler::connect() {
    set_state(FeedState::Connecting);

    ws_client_ = std::make_shared<network::WebSocketClient>(
        ioc_,
        ssl_ctx_,
        [self = shared_from_this()](std::string_view msg) {
            self->on_ws_message(msg);
        },
        [self = shared_from_this()](auto ec, auto what) {
            self->on_ws_error(ec, what);
        },
        [self = shared_from_this()]() {
            self->on_ws_connected();
        },
        [self = shared_from_this()]() {
            self->on_ws_disconnect();
        }
    );

    auto path = endpoints::ws_combined_path(config_.network.symbol);
    ws_client_->connect(
        config_.network.ws_host,
        config_.network.ws_port,
        path
    );
}

void FeedHandler::on_ws_connected() {
    spdlog::info("WebSocket connected, requesting snapshot");

    reconnect_strategy_.reset();
    set_state(FeedState::WaitingSnapshot);

    emit_message(ConnectionRestored{std::chrono::steady_clock::now()});

    // Start buffering updates and fetch snapshot
    buffered_updates_.clear();
    fetch_snapshot();
}

void FeedHandler::on_ws_message(std::string_view message) {
    // Parse combined stream wrapper
    auto stream_result = MessageParser::parse_combined_stream(message);
    if (stream_result.is_err()) {
        spdlog::warn("Failed to parse combined stream: {}", stream_result.error());
        return;
    }

    const auto& stream_msg = stream_result.value();

    if (MessageParser::is_depth_stream(stream_msg.stream)) {
        auto update_result = MessageParser::parse_depth_update(stream_msg.data);
        if (update_result.is_ok()) {
            process_depth_update(update_result.value());
        } else {
            spdlog::warn("Failed to parse depth update: {}", update_result.error());
        }
    } else if (MessageParser::is_agg_trade_stream(stream_msg.stream)) {
        auto trade_result = MessageParser::parse_agg_trade(stream_msg.data);
        if (trade_result.is_ok()) {
            process_agg_trade(trade_result.value());
        } else {
            spdlog::warn("Failed to parse aggTrade: {}", trade_result.error());
        }
    }
}

void FeedHandler::process_depth_update(const DepthUpdate& update) {
    auto current_state = state_.load();

    if (current_state == FeedState::WaitingSnapshot) {
        // Buffer updates until snapshot arrives
        buffered_updates_.push_back(update);
        spdlog::trace("Buffered depth update u={}", update.final_update_id);
    } else if (current_state == FeedState::Live) {
        // Forward directly to engine
        emit_message(DepthUpdateMsg{update, std::chrono::steady_clock::now()});
    }
}

void FeedHandler::process_agg_trade(const AggTrade& trade) {
    // Trades are always forwarded immediately
    emit_message(AggTradeMsg{trade, std::chrono::steady_clock::now()});
}

void FeedHandler::on_ws_error(boost::system::error_code ec, std::string_view what) {
    spdlog::error("WebSocket error in {}: {}", what, ec.message());

    emit_message(ConnectionLost{
        std::string(what) + ": " + ec.message(),
        std::chrono::steady_clock::now()
    });

    schedule_reconnect();
}

void FeedHandler::on_ws_disconnect() {
    if (state_.load() == FeedState::Disconnected) {
        return;  // Intentional shutdown
    }

    spdlog::warn("WebSocket disconnected unexpectedly");

    emit_message(ConnectionLost{
        "Connection closed",
        std::chrono::steady_clock::now()
    });

    schedule_reconnect();
}

void FeedHandler::request_snapshot() {
    if (snapshot_requested_) {
        spdlog::debug("Snapshot already requested, ignoring");
        return;
    }

    spdlog::info("Snapshot requested (gap detected)");

    set_state(FeedState::WaitingSnapshot);
    buffered_updates_.clear();
    fetch_snapshot();
}

void FeedHandler::fetch_snapshot() {
    snapshot_requested_ = true;

    auto rest_client = std::make_shared<network::RestClient>(ioc_, ssl_ctx_);

    auto symbol_upper = endpoints::to_uppercase(config_.network.symbol);
    auto path = endpoints::rest_depth_path(symbol_upper, static_cast<int>(config_.engine.depth_limit));

    rest_client->get(
        config_.network.rest_host,
        config_.network.rest_port,
        path,
        [self = shared_from_this(), rest_client](auto result) {
            self->on_snapshot_response(std::move(result));
        }
    );
}

void FeedHandler::on_snapshot_response(Result<std::string, std::string> result) {
    snapshot_requested_ = false;

    if (result.is_err()) {
        spdlog::error("Failed to fetch snapshot: {}", result.error());
        // Retry after delay
        schedule_reconnect();
        return;
    }

    auto symbol_upper = endpoints::to_uppercase(config_.network.symbol);
    auto snapshot_result = MessageParser::parse_depth_snapshot(result.value(), symbol_upper);

    if (snapshot_result.is_err()) {
        spdlog::error("Failed to parse snapshot: {}", snapshot_result.error());
        schedule_reconnect();
        return;
    }

    apply_snapshot(snapshot_result.value());
}

void FeedHandler::apply_snapshot(const DepthSnapshot& snapshot) {
    spdlog::info("Applying snapshot lastUpdateId={}, buffered {} updates",
                 snapshot.last_update_id, buffered_updates_.size());

    set_state(FeedState::Syncing);

    // Send snapshot to engine
    emit_message(SnapshotMsg{snapshot, std::chrono::steady_clock::now()});

    // Replay buffered updates that came after snapshot
    // According to Binance: first update after snapshot must have
    // U <= lastUpdateId+1 AND u >= lastUpdateId+1
    bool found_first_valid = false;

    for (const auto& update : buffered_updates_) {
        if (!found_first_valid) {
            // Looking for the bridging update
            if (update.first_update_id <= snapshot.last_update_id + 1 &&
                update.final_update_id >= snapshot.last_update_id + 1) {
                found_first_valid = true;
                spdlog::debug("Found bridging update U={} u={}",
                             update.first_update_id, update.final_update_id);
            } else if (update.final_update_id <= snapshot.last_update_id) {
                // Update is older than snapshot, skip
                continue;
            } else {
                // Gap detected - update is newer than expected
                spdlog::warn("Sync gap: snapshot={} but first update U={}",
                            snapshot.last_update_id, update.first_update_id);
                // Request new snapshot
                request_snapshot();
                return;
            }
        }

        if (found_first_valid) {
            emit_message(DepthUpdateMsg{update, std::chrono::steady_clock::now()});
        }
    }

    buffered_updates_.clear();

    if (!found_first_valid && !buffered_updates_.empty()) {
        spdlog::warn("No bridging update found, requesting new snapshot");
        request_snapshot();
        return;
    }

    set_state(FeedState::Live);
    spdlog::info("Feed handler is now Live");
}

void FeedHandler::schedule_reconnect() {
    set_state(FeedState::Reconnecting);

    if (ws_client_) {
        ws_client_.reset();
    }

    auto delay = reconnect_strategy_.next_delay();
    spdlog::info("Reconnecting in {}ms (attempt {})",
                 delay.count(), reconnect_strategy_.attempt_count());

    reconnect_timer_.expires_after(delay);
    reconnect_timer_.async_wait([self = shared_from_this()](auto ec) {
        if (!ec) {
            self->on_reconnect_timer();
        }
    });
}

void FeedHandler::on_reconnect_timer() {
    if (state_.load() == FeedState::Disconnected) {
        return;  // Stopped while waiting
    }

    connect();
}

void FeedHandler::set_state(FeedState new_state) {
    auto old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        spdlog::debug("FeedState: {} -> {}", to_string(old_state), to_string(new_state));
    }
}

void FeedHandler::emit_message(EngineMessage msg) {
    if (on_message_) {
        on_message_(std::move(msg));
    }
}

FeedState FeedHandler::state() const noexcept {
    return state_.load();
}

}  // namespace titan::binance

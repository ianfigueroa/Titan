#include "binance/feed_handler.hpp"
#include "binance/endpoints.hpp"
#include "binance/message_parser.hpp"
#include <spdlog/spdlog.h>
#include <utility>

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
    close_streams();
}

void FeedHandler::connect() {
    set_state(FeedState::Connecting);

    depth_ws_ = make_stream([self = shared_from_this()]() {
        self->on_ws_connected();
    });
    trade_ws_ = make_stream([]() {
        spdlog::info("Trade stream connected");
    });

    depth_ws_->connect(config_.network.ws_host, config_.network.ws_port, config_.ws_depth_path());
    trade_ws_->connect(config_.network.ws_host, config_.network.ws_port, config_.ws_trade_path());
}

std::shared_ptr<network::WebSocketClient> FeedHandler::make_stream(std::function<void()> on_connected) {
    return std::make_shared<network::WebSocketClient>(
        ioc_,
        ssl_ctx_,
        [self = shared_from_this()](std::string_view msg) {
            self->on_ws_message(msg);
        },
        [self = shared_from_this()](auto ec, auto what) {
            self->on_ws_error(ec, what);
        },
        std::move(on_connected),
        [self = shared_from_this()]() {
            self->on_ws_disconnect();
        }
    );
}

void FeedHandler::close_streams() {
    for (auto* ws : {&depth_ws_, &trade_ws_}) {
        if (*ws) {
            (*ws)->close();
            ws->reset();
        }
    }
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
    const auto received_at = std::chrono::steady_clock::now();
    auto result = MessageParser::parse_stream_event(message);
    if (result.is_err()) {
        spdlog::warn("Failed to parse stream message: {}", result.error());
        return;
    }

    auto event = std::move(result).take_value();
    if (auto* update = std::get_if<DepthUpdate>(&event)) {
        process_depth_update(std::move(*update), received_at);
    } else if (auto* trade = std::get_if<AggTrade>(&event)) {
        process_agg_trade(std::move(*trade), received_at);
    }
}

void FeedHandler::process_depth_update(DepthUpdate update, Timestamp received_at) {
    auto current_state = state_.load();

    if (current_state == FeedState::WaitingSnapshot) {
        // Buffer updates until snapshot arrives
        spdlog::trace("Buffered depth update u={}", update.final_update_id);
        buffered_updates_.push_back(std::move(update));
    } else if (current_state == FeedState::Live) {
        // Forward directly to engine
        emit_message(DepthUpdateMsg{std::move(update), received_at});
    }
}

void FeedHandler::process_agg_trade(AggTrade trade, Timestamp received_at) {
    // Trades are always forwarded immediately
    emit_message(AggTradeMsg{std::move(trade), received_at});
}

void FeedHandler::on_ws_error(boost::system::error_code ec, std::string_view what) {
    auto state = state_.load();
    if (state == FeedState::Reconnecting || state == FeedState::Disconnected) {
        return;
    }

    spdlog::error("WebSocket error in {}: {}", what, ec.message());

    emit_message(ConnectionLost{
        std::string(what) + ": " + ec.message(),
        std::chrono::steady_clock::now()
    });

    schedule_reconnect();
}

void FeedHandler::on_ws_disconnect() {
    auto state = state_.load();
    if (state == FeedState::Disconnected || state == FeedState::Reconnecting) {
        return;
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

    emit_message(SnapshotMsg{snapshot, std::chrono::steady_clock::now()});
    for (auto& update : buffered_updates_) {
        emit_message(DepthUpdateMsg{std::move(update), std::chrono::steady_clock::now(), true});
    }
    buffered_updates_.clear();

    set_state(FeedState::Live);
    spdlog::info("Feed handler is now Live");
}

void FeedHandler::schedule_reconnect() {
    if (state_.load() == FeedState::Reconnecting) {
        return;
    }
    set_state(FeedState::Reconnecting);
    close_streams();

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

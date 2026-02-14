#include "engine/market_data_engine.hpp"
#include "network/ssl_context.hpp"
#include "output/json_formatter.hpp"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace titan {

MarketDataEngine::MarketDataEngine(const Config& config)
    : config_(config)
    , ssl_ctx_(network::create_ssl_context())
    , order_book_(config.output.imbalance_levels)
    , trade_flow_(config.engine)
    , last_metrics_output_(std::chrono::steady_clock::now())
{
    setup_logging();

    console_ = std::make_unique<output::ConsoleLogger>(config.output.console_interval);
    ws_server_ = std::make_unique<output::WebSocketServer>(config.output.ws_server_port);
}

MarketDataEngine::~MarketDataEngine() {
    request_shutdown();

    if (network_thread_.joinable()) {
        network_thread_.join();
    }
    if (engine_thread_.joinable()) {
        engine_thread_.join();
    }
}

void MarketDataEngine::setup_logging() {
    // Initialize async logging to avoid blocking
    spdlog::init_thread_pool(8192, 1);

    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "titan",
        stdout_sink,
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest
    );

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
}

void MarketDataEngine::run() {
    spdlog::info("Starting titan market data engine");
    spdlog::info("Symbol: {}", config_.network.symbol);

    // Start WebSocket server first (runs in its own thread)
    ws_server_->start();

    // Start engine thread
    engine_thread_ = std::thread([this]() {
        engine_thread_func();
    });

    // Start network thread (this is the main thread)
    network_thread_func();

    // Wait for engine thread to finish
    if (engine_thread_.joinable()) {
        engine_thread_.join();
    }

    // Stop WebSocket server
    ws_server_->stop();

    spdlog::info("Engine shutdown complete");
}

void MarketDataEngine::network_thread_func() {
    spdlog::debug("Network thread started");

    // Create feed handler
    feed_handler_ = std::make_shared<binance::FeedHandler>(
        network_ioc_,
        ssl_ctx_,
        config_,
        [this](EngineMessage msg) {
            // Push message to queue for engine thread
            if (!queue_.try_push(std::move(msg))) {
                spdlog::warn("Queue full, dropping message");
            }
        }
    );

    feed_handler_->start();

    // Run the io_context
    while (!shutdown_requested_.load()) {
        try {
            network_ioc_.run_for(std::chrono::milliseconds(100));
            network_ioc_.restart();
        } catch (const std::exception& e) {
            spdlog::error("Network thread exception: {}", e.what());
        }
    }

    // Cleanup
    feed_handler_->stop();
    feed_handler_.reset();

    // Signal engine thread to stop
    queue_.try_push(Shutdown{});

    spdlog::debug("Network thread stopped");
}

void MarketDataEngine::engine_thread_func() {
    spdlog::debug("Engine thread started");

    while (!shutdown_requested_.load()) {
        // Poll for messages
        if (auto msg = queue_.try_pop()) {
            process_message(*msg);

            // Check for shutdown message
            if (std::holds_alternative<Shutdown>(*msg)) {
                break;
            }
        } else {
            // No message, do periodic tasks
            output_metrics();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    spdlog::debug("Engine thread stopped");
}

void MarketDataEngine::process_message(const EngineMessage& message) {
    std::visit([this](const auto& msg) {
        using T = std::decay_t<decltype(msg)>;

        if constexpr (std::is_same_v<T, DepthUpdateMsg>) {
            handle_depth_update(msg);
        } else if constexpr (std::is_same_v<T, AggTradeMsg>) {
            handle_agg_trade(msg);
        } else if constexpr (std::is_same_v<T, SnapshotMsg>) {
            handle_snapshot(msg);
        } else if constexpr (std::is_same_v<T, ConnectionLost>) {
            handle_connection_lost(msg);
        } else if constexpr (std::is_same_v<T, ConnectionRestored>) {
            handle_connection_restored(msg);
        } else if constexpr (std::is_same_v<T, SequenceGap>) {
            handle_sequence_gap(msg);
        } else if constexpr (std::is_same_v<T, Shutdown>) {
            spdlog::info("Shutdown message received");
        }
    }, message);
}

void MarketDataEngine::handle_depth_update(const DepthUpdateMsg& msg) {
    if (sync_state_ != SyncState::Synced) {
        return;  // Drop updates until synced
    }

    const auto& update = msg.data;

    // Check for sequence gap
    if (last_processed_id_ > 0 &&
        order_book_.has_sequence_gap(update.first_update_id, update.prev_final_update_id)) {

        spdlog::warn("Sequence gap detected: expected {}, got prev={}",
                    last_processed_id_, update.prev_final_update_id);

        sync_state_ = SyncState::WaitingSnapshot;
        order_book_.clear();
        feed_handler_->request_snapshot();
        return;
    }

    order_book_.apply_update(update);
    last_processed_id_ = update.final_update_id;
}

void MarketDataEngine::handle_agg_trade(const AggTradeMsg& msg) {
    auto metrics = trade_flow_.process_trade(msg.data);

    // Check for new alert
    if (metrics.last_alert.has_value()) {
        pending_alert_ = metrics.last_alert;
        console_->log_alert(*metrics.last_alert);

        // Broadcast alert to WebSocket clients
        ws_server_->broadcast(output::JsonFormatter::format_alert(*metrics.last_alert));
    }
}

void MarketDataEngine::handle_snapshot(const SnapshotMsg& msg) {
    spdlog::info("Applying snapshot, lastUpdateId={}", msg.data.last_update_id);

    order_book_.apply_snapshot(msg.data);
    last_processed_id_ = msg.data.last_update_id;
    sync_state_ = SyncState::Synced;

    console_->log_sync_status("Synchronized");
    console_->force_next();
}

void MarketDataEngine::handle_connection_lost(const ConnectionLost& msg) {
    console_->log_connection_status(false, msg.reason);
    sync_state_ = SyncState::WaitingSnapshot;

    // Broadcast status to WebSocket clients
    ws_server_->broadcast(output::JsonFormatter::format_status(false, "disconnected"));
}

void MarketDataEngine::handle_connection_restored(const ConnectionRestored& /*msg*/) {
    console_->log_connection_status(true);
    sync_state_ = SyncState::WaitingSnapshot;

    // Broadcast status to WebSocket clients
    ws_server_->broadcast(output::JsonFormatter::format_status(true, "connected"));
}

void MarketDataEngine::handle_sequence_gap(const SequenceGap& msg) {
    spdlog::warn("Sequence gap: expected {}, got {}",
                msg.expected, msg.received);

    sync_state_ = SyncState::WaitingSnapshot;
    order_book_.clear();
    feed_handler_->request_snapshot();
}

void MarketDataEngine::output_metrics() {
    if (sync_state_ != SyncState::Synced) {
        return;
    }

    auto book = order_book_.snapshot();
    auto flow = trade_flow_.current_metrics();

    // Console output (rate limited internally)
    console_->log_metrics(book, flow);

    // WebSocket broadcast
    broadcast_metrics();
}

void MarketDataEngine::broadcast_metrics() {
    auto now = std::chrono::steady_clock::now();

    // Rate limit WebSocket broadcasts to match console interval
    if ((now - last_metrics_output_) >= config_.output.console_interval) {
        last_metrics_output_ = now;

        auto book = order_book_.snapshot();
        auto flow = trade_flow_.current_metrics();

        ws_server_->broadcast(output::JsonFormatter::format_metrics(book, flow));
    }
}

void MarketDataEngine::request_shutdown() {
    shutdown_requested_.store(true);
    network_ioc_.stop();
}

bool MarketDataEngine::shutdown_requested() const noexcept {
    return shutdown_requested_.load();
}

}  // namespace titan

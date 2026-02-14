#pragma once

#include "binance/feed_handler.hpp"
#include "core/config.hpp"
#include "core/messages.hpp"
#include "orderbook/order_book.hpp"
#include "output/console_logger.hpp"
#include "output/websocket_server.hpp"
#include "queue/spsc_queue.hpp"
#include "trade/trade_flow.hpp"
#include <atomic>
#include <memory>
#include <thread>

namespace titan {

/// Main market data engine
/// Coordinates all components and manages threading model
class MarketDataEngine {
public:
    /// Create a market data engine
    /// @param config Application configuration
    explicit MarketDataEngine(const Config& config);

    ~MarketDataEngine();

    // Non-copyable, non-movable
    MarketDataEngine(const MarketDataEngine&) = delete;
    MarketDataEngine& operator=(const MarketDataEngine&) = delete;

    /// Start the engine (blocks until shutdown)
    void run();

    /// Request graceful shutdown (thread-safe)
    void request_shutdown();

    /// Check if shutdown was requested
    [[nodiscard]] bool shutdown_requested() const noexcept;

private:
    void setup_logging();
    void network_thread_func();
    void engine_thread_func();

    void process_message(const EngineMessage& message);
    void handle_depth_update(const DepthUpdateMsg& msg);
    void handle_agg_trade(const AggTradeMsg& msg);
    void handle_snapshot(const SnapshotMsg& msg);
    void handle_connection_lost(const ConnectionLost& msg);
    void handle_connection_restored(const ConnectionRestored& msg);
    void handle_sequence_gap(const SequenceGap& msg);

    void output_metrics();
    void broadcast_metrics();

    const Config& config_;

    // Network components
    boost::asio::io_context network_ioc_;
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx_;
    std::shared_ptr<binance::FeedHandler> feed_handler_;

    // Queue between network and engine threads
    SpscQueue<EngineMessage, 65536> queue_;

    // Engine components
    OrderBook order_book_;
    TradeFlow trade_flow_;

    // Output components
    std::unique_ptr<output::ConsoleLogger> console_;
    std::unique_ptr<output::WebSocketServer> ws_server_;

    // Sync state
    enum class SyncState {
        Initializing,
        WaitingSnapshot,
        Synced
    };
    SyncState sync_state_{SyncState::Initializing};
    SequenceId last_processed_id_{0};

    // Threading
    std::thread network_thread_;
    std::thread engine_thread_;
    std::atomic<bool> shutdown_requested_{false};

    // Metrics timing
    std::chrono::steady_clock::time_point last_metrics_output_;
    std::optional<TradeAlert> pending_alert_;
};

}  // namespace titan

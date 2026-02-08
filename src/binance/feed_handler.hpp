#pragma once

#include "binance/feed_state.hpp"
#include "binance/types.hpp"
#include "core/config.hpp"
#include "core/messages.hpp"
#include "engine/reconnect_strategy.hpp"
#include "network/rest_client.hpp"
#include "network/websocket_client.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace titan::binance {

/// Binance Futures feed handler
/// Manages WebSocket connection, REST snapshots, and message dispatch
class FeedHandler : public std::enable_shared_from_this<FeedHandler> {
public:
    /// Callback for processed messages (routed to engine queue)
    using MessageCallback = std::function<void(EngineMessage)>;

    /// Create a new feed handler
    /// @param ioc IO context for async operations
    /// @param ssl_ctx Shared SSL context
    /// @param config Application configuration
    /// @param on_message Callback for engine messages
    FeedHandler(
        boost::asio::io_context& ioc,
        std::shared_ptr<boost::asio::ssl::context> ssl_ctx,
        const Config& config,
        MessageCallback on_message
    );

    ~FeedHandler();

    // Non-copyable, non-movable
    FeedHandler(const FeedHandler&) = delete;
    FeedHandler& operator=(const FeedHandler&) = delete;

    /// Start the feed handler (connect and subscribe)
    void start();

    /// Stop the feed handler
    void stop();

    /// Request a fresh snapshot (e.g., after gap detection)
    void request_snapshot();

    /// Get current feed state
    [[nodiscard]] FeedState state() const noexcept;

private:
    void connect();
    void on_ws_connected();
    void on_ws_message(std::string_view message);
    void on_ws_error(boost::system::error_code ec, std::string_view what);
    void on_ws_disconnect();

    void fetch_snapshot();
    void on_snapshot_response(Result<std::string, std::string> result);

    void process_depth_update(const DepthUpdate& update);
    void process_agg_trade(const AggTrade& trade);
    void apply_snapshot(const DepthSnapshot& snapshot);

    void schedule_reconnect();
    void on_reconnect_timer();

    void set_state(FeedState new_state);
    void emit_message(EngineMessage msg);

    boost::asio::io_context& ioc_;
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx_;
    const Config& config_;
    MessageCallback on_message_;

    std::shared_ptr<network::WebSocketClient> ws_client_;
    boost::asio::steady_timer reconnect_timer_;
    ReconnectStrategy reconnect_strategy_;

    std::atomic<FeedState> state_{FeedState::Disconnected};

    // Buffering during snapshot wait
    std::vector<DepthUpdate> buffered_updates_;
    bool snapshot_requested_{false};
};

}  // namespace titan::binance

#pragma once

#include "network/connection_state.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace titan::network {

/// Async WebSocket client over SSL
class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
public:
    using tcp = boost::asio::ip::tcp;
    using tcp_stream = boost::beast::tcp_stream;
    using ssl_stream = boost::asio::ssl::stream<tcp_stream>;
    using ws_stream = boost::beast::websocket::stream<ssl_stream>;

    /// Callback types
    using MessageHandler = std::function<void(std::string_view)>;
    using ErrorHandler = std::function<void(boost::system::error_code, std::string_view)>;
    using ConnectHandler = std::function<void()>;
    using DisconnectHandler = std::function<void()>;

    /// Create a new WebSocket client
    /// @param ioc IO context for async operations
    /// @param ssl_ctx Shared SSL context
    /// @param on_message Called when a message is received
    /// @param on_error Called on errors
    /// @param on_connect Called when connection is established
    /// @param on_disconnect Called when connection is lost
    WebSocketClient(
        boost::asio::io_context& ioc,
        std::shared_ptr<boost::asio::ssl::context> ssl_ctx,
        MessageHandler on_message,
        ErrorHandler on_error,
        ConnectHandler on_connect,
        DisconnectHandler on_disconnect
    );

    ~WebSocketClient();

    // Non-copyable, non-movable
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    /// Connect to a WebSocket server
    /// @param host Hostname (e.g., "fstream.binance.com")
    /// @param port Port (e.g., "443")
    /// @param path Path with query (e.g., "/stream?streams=...")
    void connect(std::string_view host, std::string_view port, std::string_view path);

    /// Send a message (thread-safe)
    void send(std::string message);

    /// Close the connection gracefully
    void close();

    /// Get current connection state
    [[nodiscard]] ConnectionState state() const noexcept;

    /// Check if connected
    [[nodiscard]] bool is_connected() const noexcept;

private:
    void do_resolve();
    void on_resolve(boost::system::error_code ec, tcp::resolver::results_type results);
    void do_connect(tcp::resolver::results_type::endpoint_type ep);
    void on_connect(boost::system::error_code ec);
    void do_ssl_handshake();
    void on_ssl_handshake(boost::system::error_code ec);
    void do_ws_handshake();
    void on_ws_handshake(boost::system::error_code ec);
    void do_read();
    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
    void do_close();
    void on_close(boost::system::error_code ec);
    void fail(boost::system::error_code ec, std::string_view what);
    void set_state(ConnectionState new_state);

    boost::asio::io_context& ioc_;
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx_;
    tcp::resolver resolver_;
    std::unique_ptr<ws_stream> ws_;
    boost::beast::flat_buffer buffer_;

    std::string host_;
    std::string port_;
    std::string path_;

    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};

    MessageHandler on_message_;
    ErrorHandler on_error_;
    ConnectHandler on_connect_;
    DisconnectHandler on_disconnect_;
};

}  // namespace titan::network

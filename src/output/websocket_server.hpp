#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>

namespace titan::output {

class WebSocketSession;

/// WebSocket server for streaming market data to clients
/// Runs on its own io_context in a separate thread to ensure
/// it cannot block the main feed handling
class WebSocketServer {
public:
    using tcp = boost::asio::ip::tcp;

    /// Create a WebSocket server
    /// @param port Port to listen on
    explicit WebSocketServer(std::uint16_t port);

    ~WebSocketServer();

    // Non-copyable, non-movable
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    /// Start the server (launches background thread)
    void start();

    /// Stop the server
    void stop();

    /// Broadcast a JSON message to all connected clients
    /// Thread-safe, can be called from any thread
    void broadcast(const nlohmann::json& message);

    /// Get number of connected clients
    [[nodiscard]] std::size_t client_count() const;

    /// Check if server is running
    [[nodiscard]] bool is_running() const noexcept;

private:
    friend class WebSocketSession;

    void do_accept();
    void on_accept(boost::system::error_code ec, tcp::socket socket);
    void add_session(std::shared_ptr<WebSocketSession> session);
    void remove_session(std::shared_ptr<WebSocketSession> session);

    std::uint16_t port_;
    boost::asio::io_context ioc_;
    tcp::acceptor acceptor_;
    std::thread io_thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex sessions_mutex_;
    std::set<std::shared_ptr<WebSocketSession>> sessions_;
};

/// Individual WebSocket client session
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    using tcp = boost::asio::ip::tcp;
    using ws_stream = boost::beast::websocket::stream<tcp::socket>;

    WebSocketSession(tcp::socket socket, WebSocketServer& server);

    /// Start the session (perform WebSocket handshake)
    void start();

    /// Send a message to this client
    void send(const std::string& message);

    /// Close the session
    void close();

private:
    void on_accept(boost::system::error_code ec);
    void do_read();
    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
    void do_write();
    void on_write(boost::system::error_code ec, std::size_t bytes_transferred);

    ws_stream ws_;
    WebSocketServer& server_;
    boost::beast::flat_buffer buffer_;

    std::mutex write_mutex_;
    std::vector<std::string> write_queue_;
    bool writing_{false};
};

}  // namespace titan::output

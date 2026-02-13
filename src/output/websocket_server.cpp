#include "output/websocket_server.hpp"
#include <spdlog/spdlog.h>

namespace titan::output {

// ============================================================================
// WebSocketServer
// ============================================================================

WebSocketServer::WebSocketServer(std::uint16_t port)
    : port_(port)
    , acceptor_(ioc_)
{}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    try {
        tcp::endpoint endpoint(tcp::v4(), port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        spdlog::info("WebSocket server listening on port {}", port_);

        do_accept();

        // Run io_context in background thread
        io_thread_ = std::thread([this]() {
            ioc_.run();
        });

    } catch (const std::exception& e) {
        spdlog::error("Failed to start WebSocket server: {}", e.what());
        running_ = false;
        throw;
    }
}

void WebSocketServer::stop() {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    spdlog::info("WebSocket server stopping");

    // Close all sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_) {
            session->close();
        }
        sessions_.clear();
    }

    // Stop accepting
    boost::system::error_code ec;
    acceptor_.close(ec);

    // Stop io_context
    ioc_.stop();

    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            on_accept(ec, std::move(socket));
        }
    );
}

void WebSocketServer::on_accept(boost::system::error_code ec, tcp::socket socket) {
    if (ec) {
        if (running_) {
            spdlog::warn("Accept error: {}", ec.message());
        }
        return;
    }

    spdlog::debug("New WebSocket client connected");

    auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
    add_session(session);
    session->start();

    // Continue accepting
    if (running_) {
        do_accept();
    }
}

void WebSocketServer::broadcast(const nlohmann::json& message) {
    std::string msg = message.dump();

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : sessions_) {
        session->send(msg);
    }
}

std::size_t WebSocketServer::client_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

bool WebSocketServer::is_running() const noexcept {
    return running_.load();
}

void WebSocketServer::add_session(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.insert(std::move(session));
}

void WebSocketServer::remove_session(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
}

// ============================================================================
// WebSocketSession
// ============================================================================

WebSocketSession::WebSocketSession(tcp::socket socket, WebSocketServer& server)
    : ws_(std::move(socket))
    , server_(server)
{}

void WebSocketSession::start() {
    // Set WebSocket options
    ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
        boost::beast::role_type::server
    ));

    ws_.set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type& res) {
            res.set(boost::beast::http::field::server, "titan/1.0");
        }
    ));

    // Accept the WebSocket handshake
    ws_.async_accept(
        [self = shared_from_this()](boost::system::error_code ec) {
            self->on_accept(ec);
        }
    );
}

void WebSocketSession::on_accept(boost::system::error_code ec) {
    if (ec) {
        spdlog::debug("WebSocket accept error: {}", ec.message());
        server_.remove_session(shared_from_this());
        return;
    }

    // Start reading (to detect disconnects)
    do_read();
}

void WebSocketSession::do_read() {
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](auto ec, auto bytes) {
            self->on_read(ec, bytes);
        }
    );
}

void WebSocketSession::on_read(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        if (ec != boost::beast::websocket::error::closed) {
            spdlog::debug("WebSocket read error: {}", ec.message());
        }
        server_.remove_session(shared_from_this());
        return;
    }

    // Discard any messages from client (we only broadcast)
    buffer_.consume(buffer_.size());

    // Continue reading
    do_read();
}

void WebSocketSession::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.push_back(message);

    if (!writing_) {
        writing_ = true;
        do_write();
    }
}

void WebSocketSession::do_write() {
    std::string msg;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty()) {
            writing_ = false;
            return;
        }
        msg = std::move(write_queue_.front());
        write_queue_.erase(write_queue_.begin());
    }

    ws_.text(true);
    ws_.async_write(
        boost::asio::buffer(msg),
        [self = shared_from_this()](auto ec, auto bytes) {
            self->on_write(ec, bytes);
        }
    );
}

void WebSocketSession::on_write(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        spdlog::debug("WebSocket write error: {}", ec.message());
        server_.remove_session(shared_from_this());
        return;
    }

    // Continue with next message in queue
    do_write();
}

void WebSocketSession::close() {
    boost::system::error_code ec;
    ws_.close(boost::beast::websocket::close_code::normal, ec);
}

}  // namespace titan::output

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

    // First read the HTTP upgrade request from the browser
    // This is required for RFC 6455 compliant WebSocket handshake
    boost::beast::http::async_read(
        ws_.next_layer(),
        http_buffer_,
        upgrade_request_,
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
            self->on_http_read(ec, bytes);
        }
    );
}

void WebSocketSession::on_http_read(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        spdlog::debug("HTTP upgrade read error: {}", ec.message());
        server_.remove_session(shared_from_this());
        return;
    }

    // Validate this is a WebSocket upgrade request
    if (!boost::beast::websocket::is_upgrade(upgrade_request_)) {
        spdlog::debug("Received non-WebSocket HTTP request");
        server_.remove_session(shared_from_this());
        return;
    }

    // Accept the WebSocket handshake with the parsed HTTP request
    ws_.async_accept(
        upgrade_request_,
        [self = shared_from_this()](boost::system::error_code accept_ec) {
            self->on_accept(accept_ec);
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
    // Post to the io_context to ensure thread safety
    // async operations must be called from the io_context's thread
    boost::asio::post(
        ws_.get_executor(),
        [self = shared_from_this(), msg = message]() {
            bool should_write = false;
            {
                std::lock_guard<std::mutex> lock(self->write_mutex_);
                self->write_queue_.push_back(msg);

                if (!self->writing_) {
                    self->writing_ = true;
                    should_write = true;
                }
            }
            // Call do_write outside the lock to avoid deadlock
            if (should_write) {
                self->do_write();
            }
        }
    );
}

void WebSocketSession::do_write() {
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty()) {
            writing_ = false;
            return;
        }
        current_message_ = std::move(write_queue_.front());
        write_queue_.pop_front();
    }

    ws_.text(true);
    ws_.async_write(
        boost::asio::buffer(current_message_),
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

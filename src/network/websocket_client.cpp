#include "network/websocket_client.hpp"
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <spdlog/spdlog.h>

namespace titan::network {

WebSocketClient::WebSocketClient(
    boost::asio::io_context& ioc,
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx,
    MessageHandler on_message,
    ErrorHandler on_error,
    ConnectHandler on_connect,
    DisconnectHandler on_disconnect
)
    : ioc_(ioc)
    , ssl_ctx_(std::move(ssl_ctx))
    , resolver_(ioc)
    , on_message_(std::move(on_message))
    , on_error_(std::move(on_error))
    , on_connect_(std::move(on_connect))
    , on_disconnect_(std::move(on_disconnect))
{}

WebSocketClient::~WebSocketClient() {
    if (ws_ && ws_->is_open()) {
        boost::system::error_code ec;
        ws_->close(boost::beast::websocket::close_code::normal, ec);
    }
}

void WebSocketClient::connect(std::string_view host, std::string_view port, std::string_view path) {
    host_ = std::string(host);
    port_ = std::string(port);
    path_ = std::string(path);

    spdlog::info("WebSocket connecting to {}:{}{}", host_, port_, path_);
    set_state(ConnectionState::Resolving);
    do_resolve();
}

void WebSocketClient::do_resolve() {
    resolver_.async_resolve(
        host_,
        port_,
        [self = shared_from_this()](auto ec, auto results) {
            self->on_resolve(ec, results);
        }
    );
}

void WebSocketClient::on_resolve(boost::system::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        return fail(ec, "resolve");
    }

    spdlog::debug("Resolved {} endpoints", results.size());

    // Create a new WebSocket stream
    ws_ = std::make_unique<ws_stream>(ioc_, *ssl_ctx_);

    // Set SNI hostname for SSL
    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
        boost::system::error_code ssl_ec{
            static_cast<int>(::ERR_get_error()),
            boost::asio::error::get_ssl_category()
        };
        return fail(ssl_ec, "ssl_sni");
    }

    set_state(ConnectionState::Connecting);
    do_connect(results.begin()->endpoint());
}

void WebSocketClient::do_connect(tcp::resolver::results_type::endpoint_type ep) {
    // Set TCP connect timeout
    boost::beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));

    boost::beast::get_lowest_layer(*ws_).async_connect(
        ep,
        [self = shared_from_this()](auto ec) {
            self->on_connect(ec);
        }
    );
}

void WebSocketClient::on_connect(boost::system::error_code ec) {
    if (ec) {
        return fail(ec, "connect");
    }

    spdlog::debug("TCP connected to {}:{}", host_, port_);

    set_state(ConnectionState::SslHandshake);
    do_ssl_handshake();
}

void WebSocketClient::do_ssl_handshake() {
    boost::beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));

    ws_->next_layer().async_handshake(
        boost::asio::ssl::stream_base::client,
        [self = shared_from_this()](auto ec) {
            self->on_ssl_handshake(ec);
        }
    );
}

void WebSocketClient::on_ssl_handshake(boost::system::error_code ec) {
    if (ec) {
        return fail(ec, "ssl_handshake");
    }

    spdlog::debug("SSL handshake complete");

    // Disable timeout for WebSocket (it has its own ping/pong)
    boost::beast::get_lowest_layer(*ws_).expires_never();

    // Set WebSocket options
    ws_->set_option(boost::beast::websocket::stream_base::timeout::suggested(
        boost::beast::role_type::client
    ));

    // Set a user agent
    ws_->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::request_type& req) {
            req.set(boost::beast::http::field::user_agent, "titan/1.0");
        }
    ));

    set_state(ConnectionState::WsHandshake);
    do_ws_handshake();
}

void WebSocketClient::do_ws_handshake() {
    ws_->async_handshake(
        host_,
        path_,
        [self = shared_from_this()](auto ec) {
            self->on_ws_handshake(ec);
        }
    );
}

void WebSocketClient::on_ws_handshake(boost::system::error_code ec) {
    if (ec) {
        return fail(ec, "ws_handshake");
    }

    spdlog::info("WebSocket connected to {}:{}{}", host_, port_, path_);

    set_state(ConnectionState::Connected);

    if (on_connect_) {
        on_connect_();
    }

    do_read();
}

void WebSocketClient::do_read() {
    ws_->async_read(
        buffer_,
        [self = shared_from_this()](auto ec, auto bytes) {
            self->on_read(ec, bytes);
        }
    );
}

void WebSocketClient::on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == boost::beast::websocket::error::closed) {
            spdlog::info("WebSocket closed by server");
            set_state(ConnectionState::Disconnected);
            if (on_disconnect_) {
                on_disconnect_();
            }
            return;
        }
        return fail(ec, "read");
    }

    // Process the message
    if (on_message_) {
        auto data = boost::beast::buffers_to_string(buffer_.data());
        on_message_(data);
    }

    buffer_.consume(bytes_transferred);

    // Continue reading
    do_read();
}

void WebSocketClient::send(std::string message) {
    if (!is_connected()) {
        spdlog::warn("Cannot send: not connected");
        return;
    }

    // Post to strand to ensure thread safety
    boost::asio::post(ioc_, [self = shared_from_this(), msg = std::move(message)]() mutable {
        boost::system::error_code ec;
        self->ws_->write(boost::asio::buffer(msg), ec);
        if (ec) {
            spdlog::error("WebSocket send error: {}", ec.message());
        }
    });
}

void WebSocketClient::close() {
    if (state_.load() == ConnectionState::Disconnected) {
        return;
    }

    set_state(ConnectionState::Closing);
    do_close();
}

void WebSocketClient::do_close() {
    ws_->async_close(
        boost::beast::websocket::close_code::normal,
        [self = shared_from_this()](auto ec) {
            self->on_close(ec);
        }
    );
}

void WebSocketClient::on_close(boost::system::error_code ec) {
    if (ec) {
        spdlog::warn("WebSocket close error: {}", ec.message());
    }

    spdlog::info("WebSocket connection closed");
    set_state(ConnectionState::Disconnected);

    if (on_disconnect_) {
        on_disconnect_();
    }
}

void WebSocketClient::fail(boost::system::error_code ec, std::string_view what) {
    spdlog::error("WebSocket {} error: {}", what, ec.message());

    set_state(ConnectionState::Failed);

    if (on_error_) {
        on_error_(ec, what);
    }

    if (on_disconnect_) {
        on_disconnect_();
    }
}

void WebSocketClient::set_state(ConnectionState new_state) {
    state_.store(new_state, std::memory_order_release);
}

ConnectionState WebSocketClient::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

bool WebSocketClient::is_connected() const noexcept {
    return state() == ConnectionState::Connected;
}

}  // namespace titan::network

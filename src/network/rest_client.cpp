#include "network/rest_client.hpp"
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

// Helper to set SNI hostname without old-style cast warning
namespace {
inline bool set_sni_hostname(SSL* ssl, const char* hostname) {
    // SSL_set_tlsext_host_name is a macro with old-style cast
    // Use SSL_ctrl directly to avoid warning
    return SSL_ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME,
                    TLSEXT_NAMETYPE_host_name,
                    const_cast<char*>(hostname)) != 0;
}
}  // namespace

namespace titan::network {

RestClient::RestClient(
    boost::asio::io_context& ioc,
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx
)
    : ioc_(ioc)
    , ssl_ctx_(std::move(ssl_ctx))
    , resolver_(ioc)
{}

RestClient::~RestClient() {
    if (stream_) {
        boost::system::error_code ec;
        stream_->shutdown(ec);
    }
}

void RestClient::get(
    std::string_view host,
    std::string_view port,
    std::string_view path,
    ResponseHandler handler
) {
    host_ = std::string(host);
    port_ = std::string(port);
    path_ = std::string(path);
    handler_ = std::move(handler);

    spdlog::debug("REST GET https://{}:{}{}", host_, port_, path_);

    do_resolve();
}

void RestClient::do_resolve() {
    resolver_.async_resolve(
        host_,
        port_,
        [self = shared_from_this()](auto ec, auto results) {
            self->on_resolve(ec, results);
        }
    );
}

void RestClient::on_resolve(boost::system::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        return fail("resolve", ec);
    }

    // Create a new SSL stream
    stream_ = std::make_unique<ssl_stream>(ioc_, *ssl_ctx_);

    // Set SNI hostname (using helper to avoid old-style cast)
    if (!set_sni_hostname(stream_->native_handle(), host_.c_str())) {
        boost::system::error_code ssl_ec{
            static_cast<int>(::ERR_get_error()),
            boost::asio::error::get_ssl_category()
        };
        return fail("ssl_sni", ssl_ec);
    }

    do_connect(results.begin()->endpoint());
}

void RestClient::do_connect(tcp::resolver::results_type::endpoint_type ep) {
    stream_->next_layer().async_connect(
        ep,
        [self = shared_from_this()](auto ec) {
            self->on_connect(ec);
        }
    );
}

void RestClient::on_connect(boost::system::error_code ec) {
    if (ec) {
        return fail("connect", ec);
    }

    do_ssl_handshake();
}

void RestClient::do_ssl_handshake() {
    stream_->async_handshake(
        boost::asio::ssl::stream_base::client,
        [self = shared_from_this()](auto ec) {
            self->on_ssl_handshake(ec);
        }
    );
}

void RestClient::on_ssl_handshake(boost::system::error_code ec) {
    if (ec) {
        return fail("ssl_handshake", ec);
    }

    // Prepare the HTTP request
    req_.method(boost::beast::http::verb::get);
    req_.target(path_);
    req_.version(11);
    req_.set(boost::beast::http::field::host, host_);
    req_.set(boost::beast::http::field::user_agent, "titan/1.0");
    req_.set(boost::beast::http::field::accept, "application/json");

    do_write();
}

void RestClient::do_write() {
    boost::beast::http::async_write(
        *stream_,
        req_,
        [self = shared_from_this()](auto ec, auto bytes) {
            self->on_write(ec, bytes);
        }
    );
}

void RestClient::on_write(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        return fail("write", ec);
    }

    do_read();
}

void RestClient::do_read() {
    boost::beast::http::async_read(
        *stream_,
        buffer_,
        res_,
        [self = shared_from_this()](auto ec, auto bytes) {
            self->on_read(ec, bytes);
        }
    );
}

void RestClient::on_read(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        return fail("read", ec);
    }

    // Check HTTP status
    auto status = res_.result();
    if (status != boost::beast::http::status::ok) {
        std::string error = "HTTP " + std::to_string(static_cast<int>(status)) +
                           ": " + std::string(res_.reason());
        spdlog::warn("REST request failed: {}", error);
        if (handler_) {
            handler_(Result<std::string, std::string>::Err(std::move(error)));
        }
        do_shutdown();
        return;
    }

    // Success - return the body
    spdlog::debug("REST response: {} bytes", res_.body().size());
    if (handler_) {
        handler_(Result<std::string, std::string>::Ok(std::move(res_.body())));
    }

    do_shutdown();
}

void RestClient::do_shutdown() {
    stream_->async_shutdown(
        [self = shared_from_this()](auto ec) {
            self->on_shutdown(ec);
        }
    );
}

void RestClient::on_shutdown(boost::system::error_code ec) {
    // SSL shutdown errors are common and can be ignored
    if (ec && ec != boost::asio::error::eof &&
        ec != boost::asio::ssl::error::stream_truncated) {
        spdlog::debug("SSL shutdown: {}", ec.message());
    }

    // Connection is now closed
}

void RestClient::fail(const std::string& what, boost::system::error_code ec) {
    std::string error = what + ": " + ec.message();
    spdlog::error("REST {} error: {}", what, ec.message());

    if (handler_) {
        handler_(Result<std::string, std::string>::Err(std::move(error)));
    }
}

}  // namespace titan::network

#pragma once

#include "core/status.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace titan::network {

/// Async HTTPS client for REST API calls
class RestClient : public std::enable_shared_from_this<RestClient> {
public:
    using tcp = boost::asio::ip::tcp;
    using ssl_stream = boost::asio::ssl::stream<tcp::socket>;

    /// Response handler callback
    using ResponseHandler = std::function<void(Result<std::string, std::string>)>;

    /// Create a new REST client
    /// @param ioc IO context for async operations
    /// @param ssl_ctx Shared SSL context
    RestClient(
        boost::asio::io_context& ioc,
        std::shared_ptr<boost::asio::ssl::context> ssl_ctx
    );

    ~RestClient();

    // Non-copyable, non-movable
    RestClient(const RestClient&) = delete;
    RestClient& operator=(const RestClient&) = delete;

    /// Perform an async GET request
    /// @param host Hostname (e.g., "fapi.binance.com")
    /// @param port Port (e.g., "443")
    /// @param path Path with query (e.g., "/fapi/v1/depth?symbol=BTCUSDT&limit=1000")
    /// @param handler Callback with response body or error
    void get(
        std::string_view host,
        std::string_view port,
        std::string_view path,
        ResponseHandler handler
    );

private:
    void do_resolve();
    void on_resolve(boost::system::error_code ec, tcp::resolver::results_type results);
    void do_connect(tcp::resolver::results_type::endpoint_type ep);
    void on_connect(boost::system::error_code ec);
    void do_ssl_handshake();
    void on_ssl_handshake(boost::system::error_code ec);
    void do_write();
    void on_write(boost::system::error_code ec, std::size_t bytes_transferred);
    void do_read();
    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
    void do_shutdown();
    void on_shutdown(boost::system::error_code ec);
    void fail(const std::string& what, boost::system::error_code ec);

    boost::asio::io_context& ioc_;
    std::shared_ptr<boost::asio::ssl::context> ssl_ctx_;
    tcp::resolver resolver_;
    std::unique_ptr<ssl_stream> stream_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> req_;
    boost::beast::http::response<boost::beast::http::string_body> res_;

    std::string host_;
    std::string port_;
    std::string path_;
    ResponseHandler handler_;
};

}  // namespace titan::network

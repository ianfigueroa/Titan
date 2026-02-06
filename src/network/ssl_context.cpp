#include "network/ssl_context.hpp"
#include <boost/asio/ssl/context.hpp>

namespace titan::network {

std::shared_ptr<boost::asio::ssl::context> create_ssl_context() {
    auto ctx = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::tlsv12_client
    );

    // Use system certificate store
    ctx->set_default_verify_paths();

    // Enable certificate verification
    ctx->set_verify_mode(boost::asio::ssl::verify_peer);

    // Set reasonable options
    ctx->set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3 |
        boost::asio::ssl::context::single_dh_use
    );

    return ctx;
}

}  // namespace titan::network

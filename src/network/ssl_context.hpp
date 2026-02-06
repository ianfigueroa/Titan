#pragma once

#include <boost/asio/ssl/context.hpp>
#include <memory>

namespace titan::network {

/// Create a shared SSL context configured for TLS client connections
/// This context should be shared between REST and WebSocket clients
[[nodiscard]] std::shared_ptr<boost::asio::ssl::context> create_ssl_context();

}  // namespace titan::network

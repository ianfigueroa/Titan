#pragma once

#include <string_view>

namespace titan::network {

/// Connection state for network clients
enum class ConnectionState {
    Disconnected,    // Not connected
    Resolving,       // DNS resolution in progress
    Connecting,      // TCP connection in progress
    SslHandshake,    // SSL/TLS handshake in progress
    WsHandshake,     // WebSocket upgrade in progress
    Connected,       // Fully connected and ready
    Closing,         // Graceful close in progress
    Failed           // Connection failed
};

/// Convert ConnectionState to string for logging
[[nodiscard]] constexpr std::string_view to_string(ConnectionState state) noexcept {
    switch (state) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Resolving:    return "Resolving";
        case ConnectionState::Connecting:   return "Connecting";
        case ConnectionState::SslHandshake: return "SslHandshake";
        case ConnectionState::WsHandshake:  return "WsHandshake";
        case ConnectionState::Connected:    return "Connected";
        case ConnectionState::Closing:      return "Closing";
        case ConnectionState::Failed:       return "Failed";
    }
    return "Unknown";
}

}  // namespace titan::network

#pragma once

#include <string_view>

namespace titan::binance {

/// Feed handler state machine states
enum class FeedState {
    Disconnected,      // Initial state, not connected
    Connecting,        // TCP/SSL/WS handshake in progress
    WaitingSnapshot,   // Connected, buffering updates, waiting for REST snapshot
    Syncing,           // Applying snapshot and buffered updates
    Live,              // Fully synchronized, processing updates in real-time
    Reconnecting       // Connection lost, backing off before retry
};

/// Convert FeedState to string for logging
[[nodiscard]] constexpr std::string_view to_string(FeedState state) noexcept {
    switch (state) {
        case FeedState::Disconnected:    return "Disconnected";
        case FeedState::Connecting:      return "Connecting";
        case FeedState::WaitingSnapshot: return "WaitingSnapshot";
        case FeedState::Syncing:         return "Syncing";
        case FeedState::Live:            return "Live";
        case FeedState::Reconnecting:    return "Reconnecting";
    }
    return "Unknown";
}

}  // namespace titan::binance

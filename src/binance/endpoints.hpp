#pragma once

#include <string>
#include <string_view>

namespace titan::binance {

/// Binance Futures API endpoints
namespace endpoints {

/// WebSocket base host
constexpr std::string_view WS_HOST = "fstream.binance.com";

/// REST API base host
constexpr std::string_view REST_HOST = "fapi.binance.com";

/// Default port for HTTPS/WSS
constexpr std::string_view PORT = "443";

/// Build REST path for depth snapshot
/// @param symbol Uppercase symbol (e.g., "BTCUSDT")
/// @param limit Number of price levels (default 1000, max 1000)
/// @return Path like "/fapi/v1/depth?symbol=BTCUSDT&limit=1000"
[[nodiscard]] inline std::string rest_depth_path(std::string_view symbol, int limit = 1000) {
    std::string path = "/fapi/v1/depth?symbol=";
    path += symbol;
    path += "&limit=";
    path += std::to_string(limit);
    return path;
}

/// Convert symbol to uppercase for REST API
[[nodiscard]] inline std::string to_uppercase(std::string_view symbol) {
    std::string result;
    result.reserve(symbol.size());
    for (char c : symbol) {
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

}  // namespace endpoints

}  // namespace titan::binance

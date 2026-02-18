#pragma once

#include "core/status.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace titan {

/// Immutable configuration for titan
struct Config {
    /// Network configuration
    struct Network {
        std::string ws_host = "fstream.binance.com";
        std::string ws_port = "443";
        std::string rest_host = "fapi.binance.com";
        std::string rest_port = "443";
        std::string symbol = "btcusdt";

        // Reconnection settings
        std::chrono::milliseconds reconnect_delay_initial{1000};
        std::chrono::milliseconds reconnect_delay_max{30000};
        double reconnect_backoff_multiplier = 2.0;
        double reconnect_jitter_factor = 0.3;  // +/- 30%
    };

    /// Engine configuration
    struct Engine {
        std::size_t queue_capacity = 65536;
        std::size_t vwap_window = 100;
        double large_trade_std_devs = 2.0;
        std::size_t depth_limit = 1000;  // REST snapshot depth
    };

    /// Output configuration
    struct Output {
        std::chrono::milliseconds console_interval{500};
        std::uint16_t ws_server_port = 9001;
        std::size_t imbalance_levels = 5;
    };

    Network network;
    Engine engine;
    Output output;

    /// Create default configuration
    [[nodiscard]] static Config defaults() {
        return Config{};
    }

    /// Load configuration from JSON file
    /// Falls back to defaults for any missing fields
    /// @param path Path to the JSON configuration file
    /// @return Config on success, error message on failure
    [[nodiscard]] static Result<Config, std::string> load_from_file(const std::string& path);

    /// Load configuration with optional file path and environment variable overrides
    /// Priority (highest to lowest): environment variables > config file > defaults
    /// @param config_path Optional path to JSON config file
    /// @return Loaded configuration
    [[nodiscard]] static Config load(const std::optional<std::string>& config_path = std::nullopt);

    /// Build WebSocket path for combined streams
    [[nodiscard]] std::string ws_stream_path() const {
        return "/stream?streams=" + network.symbol + "@depth@100ms/" +
               network.symbol + "@aggTrade";
    }

    /// Build REST depth snapshot path
    [[nodiscard]] std::string rest_depth_path() const {
        // Convert symbol to uppercase for REST API
        std::string upper_symbol = network.symbol;
        for (char& c : upper_symbol) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return "/fapi/v1/depth?symbol=" + upper_symbol +
               "&limit=" + std::to_string(engine.depth_limit);
    }
};

}  // namespace titan

#include "core/config.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace titan {

using json = nlohmann::json;

namespace {

/// Get environment variable value, or nullopt if not set
std::optional<std::string> get_env(const char* name) {
    const char* value = std::getenv(name);
    if (value != nullptr) {
        return std::string(value);
    }
    return std::nullopt;
}

/// Get environment variable as integer (with optional range validation)
std::optional<int> get_env_int(const char* name, int min_val = std::numeric_limits<int>::min(),
                               int max_val = std::numeric_limits<int>::max()) {
    auto value = get_env(name);
    if (value) {
        try {
            int result = std::stoi(*value);
            if (result < min_val || result > max_val) {
                std::cerr << "Warning: " << name << " value " << result
                          << " out of range [" << min_val << ", " << max_val
                          << "], ignoring" << std::endl;
                return std::nullopt;
            }
            return result;
        } catch (...) {
            std::cerr << "Warning: Invalid integer value for " << name
                      << ": " << *value << ", ignoring" << std::endl;
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// Get environment variable as positive size_t
std::optional<std::size_t> get_env_size(const char* name, std::size_t max_val = std::numeric_limits<std::size_t>::max()) {
    auto value = get_env(name);
    if (value) {
        try {
            long long result = std::stoll(*value);
            if (result < 0) {
                std::cerr << "Warning: " << name << " must be non-negative, ignoring"
                          << std::endl;
                return std::nullopt;
            }
            if (static_cast<unsigned long long>(result) > max_val) {
                std::cerr << "Warning: " << name << " value too large, ignoring"
                          << std::endl;
                return std::nullopt;
            }
            return static_cast<std::size_t>(result);
        } catch (...) {
            std::cerr << "Warning: Invalid size value for " << name
                      << ": " << *value << ", ignoring" << std::endl;
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// Get environment variable as double
std::optional<double> get_env_double(const char* name) {
    auto value = get_env(name);
    if (value) {
        try {
            return std::stod(*value);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// Apply environment variable overrides to config
void apply_env_overrides(Config& config) {
    // Network overrides
    if (auto v = get_env("TITAN_WS_HOST")) {
        config.network.ws_host = *v;
    }
    if (auto v = get_env("TITAN_WS_PORT")) {
        config.network.ws_port = *v;
    }
    if (auto v = get_env("TITAN_REST_HOST")) {
        config.network.rest_host = *v;
    }
    if (auto v = get_env("TITAN_REST_PORT")) {
        config.network.rest_port = *v;
    }
    if (auto v = get_env("TITAN_SYMBOL")) {
        config.network.symbol = *v;
    }
    // Reconnect delay: 100ms to 5 minutes
    if (auto v = get_env_int("TITAN_RECONNECT_DELAY_INITIAL_MS", 100, 300000)) {
        config.network.reconnect_delay_initial = std::chrono::milliseconds(*v);
    }
    if (auto v = get_env_int("TITAN_RECONNECT_DELAY_MAX_MS", 1000, 600000)) {
        config.network.reconnect_delay_max = std::chrono::milliseconds(*v);
    }
    if (auto v = get_env_double("TITAN_RECONNECT_BACKOFF_MULTIPLIER")) {
        if (*v > 0.0 && *v <= 10.0) {
            config.network.reconnect_backoff_multiplier = *v;
        }
    }
    if (auto v = get_env_double("TITAN_RECONNECT_JITTER_FACTOR")) {
        if (*v >= 0.0 && *v <= 1.0) {
            config.network.reconnect_jitter_factor = *v;
        }
    }

    // Engine overrides with validation
    if (auto v = get_env_size("TITAN_QUEUE_CAPACITY", 1000000)) {
        config.engine.queue_capacity = *v;
    }
    if (auto v = get_env_size("TITAN_VWAP_WINDOW", 10000)) {
        config.engine.vwap_window = *v;
    }
    if (auto v = get_env_double("TITAN_LARGE_TRADE_STD_DEVS")) {
        if (*v > 0.0) {
            config.engine.large_trade_std_devs = *v;
        }
    }
    if (auto v = get_env_size("TITAN_DEPTH_LIMIT", 5000)) {
        config.engine.depth_limit = *v;
    }

    // Output overrides with validation
    if (auto v = get_env_int("TITAN_CONSOLE_INTERVAL_MS", 100, 60000)) {
        config.output.console_interval = std::chrono::milliseconds(*v);
    }
    // Port range: 1024-65535 (non-privileged ports)
    if (auto v = get_env_int("TITAN_WS_SERVER_PORT", 1024, 65535)) {
        config.output.ws_server_port = static_cast<std::uint16_t>(*v);
    }
    if (auto v = get_env_size("TITAN_IMBALANCE_LEVELS", 100)) {
        config.output.imbalance_levels = *v;
    }
}

}  // namespace

Result<Config, std::string> Config::load_from_file(const std::string& path) {
    // Read file contents
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<Config, std::string>::Err("Failed to open config file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Parse JSON
    json j;
    try {
        j = json::parse(content);
    } catch (const json::exception& e) {
        return Result<Config, std::string>::Err("Failed to parse JSON: " + std::string(e.what()));
    }

    // Start with defaults
    Config config = Config::defaults();

    try {
        // Network section
        if (j.contains("network")) {
            const auto& net = j["network"];
            if (net.contains("ws_host")) {
                config.network.ws_host = net["ws_host"].get<std::string>();
            }
            if (net.contains("ws_port")) {
                config.network.ws_port = net["ws_port"].get<std::string>();
            }
            if (net.contains("rest_host")) {
                config.network.rest_host = net["rest_host"].get<std::string>();
            }
            if (net.contains("rest_port")) {
                config.network.rest_port = net["rest_port"].get<std::string>();
            }
            if (net.contains("symbol")) {
                config.network.symbol = net["symbol"].get<std::string>();
            }
            if (net.contains("reconnect_delay_initial_ms")) {
                config.network.reconnect_delay_initial =
                    std::chrono::milliseconds(net["reconnect_delay_initial_ms"].get<int>());
            }
            if (net.contains("reconnect_delay_max_ms")) {
                config.network.reconnect_delay_max =
                    std::chrono::milliseconds(net["reconnect_delay_max_ms"].get<int>());
            }
            if (net.contains("reconnect_backoff_multiplier")) {
                config.network.reconnect_backoff_multiplier =
                    net["reconnect_backoff_multiplier"].get<double>();
            }
            if (net.contains("reconnect_jitter_factor")) {
                config.network.reconnect_jitter_factor =
                    net["reconnect_jitter_factor"].get<double>();
            }
        }

        // Engine section
        if (j.contains("engine")) {
            const auto& eng = j["engine"];
            if (eng.contains("queue_capacity")) {
                config.engine.queue_capacity = eng["queue_capacity"].get<std::size_t>();
            }
            if (eng.contains("vwap_window")) {
                config.engine.vwap_window = eng["vwap_window"].get<std::size_t>();
            }
            if (eng.contains("large_trade_std_devs")) {
                config.engine.large_trade_std_devs = eng["large_trade_std_devs"].get<double>();
            }
            if (eng.contains("depth_limit")) {
                config.engine.depth_limit = eng["depth_limit"].get<std::size_t>();
            }
        }

        // Output section
        if (j.contains("output")) {
            const auto& out = j["output"];
            if (out.contains("console_interval_ms")) {
                config.output.console_interval =
                    std::chrono::milliseconds(out["console_interval_ms"].get<int>());
            }
            if (out.contains("ws_server_port")) {
                config.output.ws_server_port = out["ws_server_port"].get<std::uint16_t>();
            }
            if (out.contains("imbalance_levels")) {
                config.output.imbalance_levels = out["imbalance_levels"].get<std::size_t>();
            }
        }
    } catch (const json::exception& e) {
        return Result<Config, std::string>::Err("Error reading config field: " + std::string(e.what()));
    }

    return Result<Config, std::string>::Ok(config);
}

Config Config::load(const std::optional<std::string>& config_path) {
    Config config = Config::defaults();

    // Load from file if path provided
    if (config_path) {
        auto result = load_from_file(*config_path);
        if (result.is_ok()) {
            config = result.value();
        } else {
            // Log warning when config file fails to load
            std::cerr << "Warning: Failed to load config from '" << *config_path
                      << "': " << result.error()
                      << " (using defaults with env overrides)" << std::endl;
        }
    }

    // Apply environment variable overrides (highest priority)
    apply_env_overrides(config);

    return config;
}

}  // namespace titan

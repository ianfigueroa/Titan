#include <gtest/gtest.h>
#include "core/config.hpp"
#include <fstream>
#include <cstdlib>

using namespace titan;

class ConfigTest : public ::testing::Test {
protected:
    std::string temp_config_path;

    void SetUp() override {
        temp_config_path = "test_config_temp.json";
    }

    void TearDown() override {
        // Clean up temp file
        std::remove(temp_config_path.c_str());
    }

    void write_config(const std::string& content) {
        std::ofstream file(temp_config_path);
        file << content;
    }
};

TEST_F(ConfigTest, DefaultsAreReasonable) {
    auto config = Config::defaults();

    EXPECT_EQ(config.network.ws_host, "fstream.binance.com");
    EXPECT_EQ(config.network.ws_port, "443");
    EXPECT_EQ(config.network.symbol, "btcusdt");
    EXPECT_EQ(config.engine.vwap_window, 100u);
    EXPECT_EQ(config.output.ws_server_port, 9001);
}

TEST_F(ConfigTest, LoadFromFileSuccess) {
    write_config(R"({
        "network": {
            "symbol": "ethusdt"
        },
        "engine": {
            "vwap_window": 50
        }
    })");

    auto result = Config::load_from_file(temp_config_path);

    ASSERT_TRUE(result.is_ok());
    auto config = result.value();

    // Changed values
    EXPECT_EQ(config.network.symbol, "ethusdt");
    EXPECT_EQ(config.engine.vwap_window, 50u);

    // Defaults preserved
    EXPECT_EQ(config.network.ws_host, "fstream.binance.com");
    EXPECT_EQ(config.output.ws_server_port, 9001);
}

TEST_F(ConfigTest, LoadFromFileNotFound) {
    auto result = Config::load_from_file("nonexistent_file.json");

    EXPECT_TRUE(result.is_err());
    EXPECT_TRUE(result.error().find("Failed to open") != std::string::npos);
}

TEST_F(ConfigTest, LoadFromFileInvalidJson) {
    write_config("{ invalid json }");

    auto result = Config::load_from_file(temp_config_path);

    EXPECT_TRUE(result.is_err());
    EXPECT_TRUE(result.error().find("parse") != std::string::npos);
}

TEST_F(ConfigTest, LoadFromFileEmptyJson) {
    write_config("{}");

    auto result = Config::load_from_file(temp_config_path);

    ASSERT_TRUE(result.is_ok());
    // Should return all defaults
    auto config = result.value();
    EXPECT_EQ(config.network.symbol, "btcusdt");
}

TEST_F(ConfigTest, LoadFromFileAllFields) {
    write_config(R"({
        "network": {
            "ws_host": "custom.host.com",
            "ws_port": "8443",
            "rest_host": "rest.host.com",
            "rest_port": "8444",
            "symbol": "solusdt",
            "reconnect_delay_initial_ms": 2000,
            "reconnect_delay_max_ms": 60000,
            "reconnect_backoff_multiplier": 3.0,
            "reconnect_jitter_factor": 0.5
        },
        "engine": {
            "queue_capacity": 32768,
            "vwap_window": 200,
            "large_trade_std_devs": 3.0,
            "depth_limit": 500
        },
        "output": {
            "console_interval_ms": 1000,
            "ws_server_port": 9002,
            "imbalance_levels": 10
        }
    })");

    auto result = Config::load_from_file(temp_config_path);

    ASSERT_TRUE(result.is_ok());
    auto config = result.value();

    // Network
    EXPECT_EQ(config.network.ws_host, "custom.host.com");
    EXPECT_EQ(config.network.ws_port, "8443");
    EXPECT_EQ(config.network.rest_host, "rest.host.com");
    EXPECT_EQ(config.network.rest_port, "8444");
    EXPECT_EQ(config.network.symbol, "solusdt");
    EXPECT_EQ(config.network.reconnect_delay_initial.count(), 2000);
    EXPECT_EQ(config.network.reconnect_delay_max.count(), 60000);
    EXPECT_DOUBLE_EQ(config.network.reconnect_backoff_multiplier, 3.0);
    EXPECT_DOUBLE_EQ(config.network.reconnect_jitter_factor, 0.5);

    // Engine
    EXPECT_EQ(config.engine.queue_capacity, 32768u);
    EXPECT_EQ(config.engine.vwap_window, 200u);
    EXPECT_DOUBLE_EQ(config.engine.large_trade_std_devs, 3.0);
    EXPECT_EQ(config.engine.depth_limit, 500u);

    // Output
    EXPECT_EQ(config.output.console_interval.count(), 1000);
    EXPECT_EQ(config.output.ws_server_port, 9002);
    EXPECT_EQ(config.output.imbalance_levels, 10u);
}

TEST_F(ConfigTest, LoadWithoutPathReturnsDefaults) {
    auto config = Config::load(std::nullopt);

    EXPECT_EQ(config.network.symbol, "btcusdt");
    EXPECT_EQ(config.engine.vwap_window, 100u);
}

TEST_F(ConfigTest, LoadWithPathLoadsFile) {
    write_config(R"({
        "network": { "symbol": "xrpusdt" }
    })");

    auto config = Config::load(temp_config_path);

    EXPECT_EQ(config.network.symbol, "xrpusdt");
}

TEST_F(ConfigTest, WsStreamPathBuildsCorrectly) {
    auto config = Config::defaults();
    config.network.symbol = "ethusdt";

    auto path = config.ws_stream_path();

    EXPECT_EQ(path, "/stream?streams=ethusdt@depth@100ms/ethusdt@aggTrade");
}

TEST_F(ConfigTest, RestDepthPathBuildsCorrectly) {
    auto config = Config::defaults();
    config.network.symbol = "ethusdt";
    config.engine.depth_limit = 500;

    auto path = config.rest_depth_path();

    EXPECT_EQ(path, "/fapi/v1/depth?symbol=ETHUSDT&limit=500");
}

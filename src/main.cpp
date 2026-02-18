#include "core/config.hpp"
#include "engine/market_data_engine.hpp"
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>

namespace {

std::unique_ptr<titan::MarketDataEngine> g_engine;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown requested..." << std::endl;
        if (g_engine) {
            g_engine->request_shutdown();
        }
    }
}

void print_banner() {
    std::cout << "\ntitan\n" << std::endl;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "\nOptions:\n"
              << "  -c, --config <path>  Load configuration from JSON file\n"
              << "  -s, --symbol <sym>   Trading symbol (e.g., btcusdt, ethusdt)\n"
              << "  -h, --help           Show this help message\n"
              << "  -v, --version        Show version information\n"
              << "\nEnvironment Variables:\n"
              << "  TITAN_SYMBOL         Override trading symbol\n"
              << "  TITAN_WS_HOST        WebSocket host\n"
              << "  TITAN_WS_PORT        WebSocket port\n"
              << "  TITAN_REST_HOST      REST API host\n"
              << "  TITAN_REST_PORT      REST API port\n"
              << "  TITAN_WS_SERVER_PORT Local WebSocket server port\n"
              << "  TITAN_VWAP_WINDOW    VWAP calculation window size\n"
              << "\nPriority: CLI args > Environment > Config file > Defaults\n"
              << std::endl;
}

void print_version() {
    std::cout << "titan v1.0.0\n"
              << "Market data engine for Binance Futures\n"
              << std::endl;
}

struct CliArgs {
    std::optional<std::string> config_path;
    std::optional<std::string> symbol;
    bool show_help = false;
    bool show_version = false;
};

CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
        } else if (arg == "-v" || arg == "--version") {
            args.show_version = true;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            args.config_path = argv[++i];
        } else if ((arg == "-s" || arg == "--symbol") && i + 1 < argc) {
            args.symbol = argv[++i];
        }
    }

    return args;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Parse command line arguments
    auto args = parse_args(argc, argv);

    if (args.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (args.show_version) {
        print_version();
        return 0;
    }

    print_banner();

    // Load configuration with priority: CLI > env > file > defaults
    auto config = titan::Config::load(args.config_path);

    // CLI argument overrides (highest priority)
    if (args.symbol) {
        config.network.symbol = *args.symbol;
    }

    // Print active configuration
    std::cout << "Configuration:\n"
              << "  Symbol: " << config.network.symbol << "\n"
              << "  WebSocket: " << config.network.ws_host << ":" << config.network.ws_port << "\n"
              << "  REST API: " << config.network.rest_host << ":" << config.network.rest_port << "\n"
              << "  Local WS port: " << config.output.ws_server_port << "\n"
              << std::endl;

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        // Create and run engine
        g_engine = std::make_unique<titan::MarketDataEngine>(config);
        g_engine->run();
        g_engine.reset();

        std::cout << "Goodbye!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

#include "core/config.hpp"
#include "engine/market_data_engine.hpp"
#include <csignal>
#include <iostream>
#include <memory>

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

}  // namespace

int main(int argc, char* argv[]) {
    print_banner();

    // Parse command line arguments (future: config file path)
    (void)argc;
    (void)argv;

    // Load configuration
    auto config = titan::Config::defaults();

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

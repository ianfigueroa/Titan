// Replays recorded binance depth messages through parse -> queue -> book.
// Run from the repo root: ./build/bench_ingest_replay [messages] [snapshot] [reps]
#include "binance/message_parser.hpp"
#include "core/messages.hpp"
#include "orderbook/order_book.hpp"
#include "queue/spsc_queue.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace titan;
using namespace titan::binance;

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

DepthUpdate parse_depth(const std::string& msg) {
    auto result = MessageParser::parse_stream_event(msg);
    if (result.is_err()) {
        std::fprintf(stderr, "parse failed: %s\n", result.error().c_str());
        std::exit(1);
    }
    return std::get<DepthUpdate>(std::move(result).take_value());
}

}  // namespace

int main(int argc, char** argv) {
    const char* messages_path = argc > 1 ? argv[1] : "benchmarks/data/btcusdt_depth.jsonl";
    const char* snapshot_path = argc > 2 ? argv[2] : "benchmarks/data/btcusdt_snapshot.json";
    const int reps = argc > 3 ? std::atoi(argv[3]) : 200;

    std::vector<std::string> msgs;
    std::size_t bytes = 0;
    std::ifstream in(messages_path);
    for (std::string line; std::getline(in, line);) {
        bytes += line.size();
        msgs.push_back(std::move(line));
    }
    auto snapshot = MessageParser::parse_depth_snapshot(read_file(snapshot_path), "BTCUSDT");
    if (msgs.empty() || snapshot.is_err()) {
        std::fprintf(stderr, "could not load %s / %s\n", messages_path, snapshot_path);
        return 1;
    }

    std::size_t levels = 0;
    for (const auto& m : msgs) {
        auto u = parse_depth(m);
        levels += u.bids.size() + u.asks.size();
    }
    std::printf("%zu msgs, %.2f MB, %.0f price levels/msg, %d reps\n",
                msgs.size(), static_cast<double>(bytes) / 1e6,
                static_cast<double>(levels) / static_cast<double>(msgs.size()), reps);

    const double total = static_cast<double>(msgs.size()) * reps;
    auto report = [&](const char* name, double sec) {
        std::printf("%-10s %9.0f msgs/s  %7.1f MB/s  %6.2f us/msg\n", name, total / sec,
                    static_cast<double>(bytes) * reps / sec / 1e6, sec / total * 1e6);
    };

    {
        std::size_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int r = 0; r < reps; ++r) {
            for (const auto& m : msgs) {
                checksum += parse_depth(m).bids.size();
            }
        }
        report("parse", std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        if (checksum == 0) std::printf("(no bids parsed)\n");
    }

    {
        static SpscQueue<EngineMessage, 65536> queue;
        const std::size_t n = msgs.size() * static_cast<std::size_t>(reps);

        const auto start = std::chrono::steady_clock::now();
        std::thread engine([&] {
            OrderBook book(5);
            (void)book.apply_snapshot(snapshot.value());
            for (std::size_t done = 0; done < n;) {
                auto msg = queue.try_pop();
                if (!msg) continue;
                (void)book.apply_update(std::get<DepthUpdateMsg>(*msg).data);
                ++done;
            }
        });
        for (int r = 0; r < reps; ++r) {
            for (const auto& m : msgs) {
                EngineMessage msg{DepthUpdateMsg{parse_depth(m), std::chrono::steady_clock::now()}};
                while (!queue.try_push(std::move(msg))) {}
            }
        }
        engine.join();
        report("pipeline", std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    }
}

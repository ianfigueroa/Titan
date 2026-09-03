// Cross-thread throughput/latency microbench for the SPSC queue.
//
// The Google Benchmark suite in bench_spsc_queue.cpp measures single-threaded
// push/pop cost. This bench measures the real producer/consumer hand-off across
// two threads, which is the figure quoted for the queue. Run it in isolation:
// concurrent load on the machine understates throughput by ~25%.
//
//   ./bench_spsc_throughput [N]     (default N = 500,000,000)
//
#include "queue/spsc_queue.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main(int argc, char** argv) {
    constexpr std::size_t kCapacity = 1u << 16;
    const std::uint64_t n = (argc > 1) ? std::strtoull(argv[1], nullptr, 10)
                                       : 500'000'000ULL;

    titan::SpscQueue<std::uint64_t, kCapacity> queue;

    const auto start = std::chrono::steady_clock::now();
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < n;) {
            if (queue.try_push(i)) ++i;
        }
    });

    std::uint64_t received = 0, checksum = 0;
    while (received < n) {
        if (auto v = queue.try_pop()) {
            checksum += *v;
            ++received;
        }
    }
    const auto end = std::chrono::steady_clock::now();
    producer.join();

    const double ns = std::chrono::duration<double, std::nano>(end - start).count();
    std::printf("SPSC hand-off: N=%llu  %.3fs  %.1f M events/s  %.3f ns/handoff  (checksum=%llu)\n",
                static_cast<unsigned long long>(n), ns / 1e9,
                static_cast<double>(n) / (ns / 1e9) / 1e6, ns / static_cast<double>(n),
                static_cast<unsigned long long>(checksum));
    return 0;
}

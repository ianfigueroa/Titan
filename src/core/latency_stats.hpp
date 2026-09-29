#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace titan {

class LatencyStats {
public:
    struct Summary {
        std::size_t count{0};
        std::chrono::nanoseconds p50{0};
        std::chrono::nanoseconds p99{0};
        std::chrono::nanoseconds max{0};
    };

    explicit LatencyStats(std::size_t window = 4096)
        : samples_(window == 0 ? 1 : window) {}

    void record(std::chrono::nanoseconds latency) noexcept {
        samples_[next_] = latency.count();
        next_ = (next_ + 1) % samples_.size();
        filled_ = std::min(filled_ + 1, samples_.size());
        ++total_;
    }

    [[nodiscard]] Summary summary() const {
        Summary s;
        s.count = filled_;
        if (filled_ == 0) {
            return s;
        }

        std::vector<std::int64_t> sorted(samples_.begin(),
                                         samples_.begin() + static_cast<std::ptrdiff_t>(filled_));
        std::sort(sorted.begin(), sorted.end());
        s.p50 = std::chrono::nanoseconds(sorted[rank(50)]);
        s.p99 = std::chrono::nanoseconds(sorted[rank(99)]);
        s.max = std::chrono::nanoseconds(sorted.back());
        return s;
    }

    [[nodiscard]] std::size_t total() const noexcept { return total_; }

    void clear() noexcept {
        next_ = 0;
        filled_ = 0;
        total_ = 0;
    }

private:
    [[nodiscard]] std::size_t rank(std::size_t percent) const noexcept {
        std::size_t r = (percent * filled_ + 99) / 100;
        return std::clamp<std::size_t>(r, 1, filled_) - 1;
    }

    std::vector<std::int64_t> samples_;
    std::size_t next_{0};
    std::size_t filled_{0};
    std::size_t total_{0};
};

}  // namespace titan

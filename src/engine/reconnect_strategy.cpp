#include "engine/reconnect_strategy.hpp"
#include <algorithm>

namespace titan {

ReconnectStrategy::ReconnectStrategy(
    std::chrono::milliseconds base_delay,
    std::chrono::milliseconds max_delay,
    double multiplier,
    double jitter_factor
)
    : base_delay_(base_delay)
    , max_delay_(max_delay)
    , current_delay_(base_delay)
    , multiplier_(multiplier)
    , jitter_factor_(jitter_factor)
{}

std::chrono::milliseconds ReconnectStrategy::next_delay() {
    ++attempt_count_;

    // Cap at max delay
    auto delay = std::min(current_delay_, max_delay_);

    // Apply random jitter: delay * (1 ± jitter_factor)
    std::uniform_real_distribution<double> dist(
        1.0 - jitter_factor_,
        1.0 + jitter_factor_
    );
    double jitter_multiplier = dist(rng_);
    auto jittered_count = static_cast<std::int64_t>(
        static_cast<double>(delay.count()) * jitter_multiplier
    );
    auto jittered = std::chrono::milliseconds{jittered_count};

    // Increase delay for next time
    auto next_count = static_cast<std::int64_t>(
        static_cast<double>(current_delay_.count()) * multiplier_
    );
    current_delay_ = std::chrono::milliseconds{next_count};

    return jittered;
}

void ReconnectStrategy::reset() {
    current_delay_ = base_delay_;
    attempt_count_ = 0;
}

std::chrono::milliseconds ReconnectStrategy::current_delay() const noexcept {
    return current_delay_;
}

std::size_t ReconnectStrategy::attempt_count() const noexcept {
    return attempt_count_;
}

}  // namespace titan

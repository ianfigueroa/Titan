#pragma once

#include <chrono>
#include <random>

namespace titan {

/// Exponential backoff with random jitter for reconnection
class ReconnectStrategy {
public:
    /// Create a reconnect strategy
    /// @param base_delay Initial delay
    /// @param max_delay Maximum delay cap
    /// @param multiplier Backoff multiplier
    /// @param jitter_factor Random jitter factor (e.g., 0.3 for ±30%)
    ReconnectStrategy(
        std::chrono::milliseconds base_delay = std::chrono::milliseconds{1000},
        std::chrono::milliseconds max_delay = std::chrono::milliseconds{30000},
        double multiplier = 2.0,
        double jitter_factor = 0.3
    );

    /// Get the next delay with jitter applied
    /// Increases internal delay for subsequent calls
    [[nodiscard]] std::chrono::milliseconds next_delay();

    /// Reset delay back to base
    void reset();

    /// Get current delay (without jitter, without incrementing)
    [[nodiscard]] std::chrono::milliseconds current_delay() const noexcept;

    /// Get attempt count since last reset
    [[nodiscard]] std::size_t attempt_count() const noexcept;

private:
    std::chrono::milliseconds base_delay_;
    std::chrono::milliseconds max_delay_;
    std::chrono::milliseconds current_delay_;
    double multiplier_;
    double jitter_factor_;
    std::size_t attempt_count_{0};

    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace titan

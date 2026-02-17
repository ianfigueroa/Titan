#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>

namespace titan {

/// Lock-free Single-Producer Single-Consumer ring buffer queue
///
/// Thread safety: Exactly one thread may call try_push(), and exactly
/// one (potentially different) thread may call try_pop(). All other
/// methods are thread-safe for read.
///
/// @tparam T Element type (must be move-constructible)
/// @tparam Capacity Queue capacity (must be power of 2)
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");
    static_assert(Capacity > 0, "Capacity must be greater than 0");
    static_assert(std::is_move_constructible_v<T>,
                  "T must be move constructible");

public:
    SpscQueue() noexcept : head_(0), tail_(0) {
        for (std::size_t i = 0; i < Capacity; ++i) {
            slots_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~SpscQueue() {
        // Destroy any remaining elements
        while (try_pop().has_value()) {}
    }

    // Non-copyable, non-movable (contains atomics)
    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&) = delete;
    SpscQueue& operator=(SpscQueue&&) = delete;

    /// Try to push an element to the queue (lvalue version, makes a copy)
    /// @return true if successful, false if queue is full
    [[nodiscard]] bool try_push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires std::is_copy_constructible_v<T>
    {
        const std::size_t pos = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[pos & kMask];

        const std::size_t seq = slot.sequence.load(std::memory_order_acquire);

        if (seq != pos) {
            // Queue is full
            return false;
        }

        // Construct the element in place (copy)
        new (&slot.storage) T(value);

        // Publish the element
        slot.sequence.store(pos + 1, std::memory_order_release);
        tail_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    /// Try to push an element to the queue (rvalue version, moves)
    /// @return true if successful, false if queue is full
    [[nodiscard]] bool try_push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        const std::size_t pos = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[pos & kMask];

        const std::size_t seq = slot.sequence.load(std::memory_order_acquire);

        if (seq != pos) {
            // Queue is full
            return false;
        }

        // Construct the element in place
        new (&slot.storage) T(std::move(value));

        // Publish the element
        slot.sequence.store(pos + 1, std::memory_order_release);
        tail_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    /// Try to pop an element from the queue
    /// @return The element if successful, std::nullopt if queue is empty
    [[nodiscard]] std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        const std::size_t pos = head_.load(std::memory_order_relaxed);
        Slot& slot = slots_[pos & kMask];

        const std::size_t seq = slot.sequence.load(std::memory_order_acquire);

        if (seq != pos + 1) {
            // Queue is empty
            return std::nullopt;
        }

        // Move the element out
        T* ptr = std::launder(reinterpret_cast<T*>(&slot.storage));
        std::optional<T> result(std::move(*ptr));
        ptr->~T();

        // Mark slot as available for reuse
        slot.sequence.store(pos + Capacity, std::memory_order_release);
        head_.store(pos + 1, std::memory_order_relaxed);

        return result;
    }

    /// Approximate size (may be slightly inaccurate under concurrent access)
    [[nodiscard]] std::size_t size_approx() const noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_relaxed);
        return tail - head;
    }

    /// Check if queue appears empty
    [[nodiscard]] bool is_empty() const noexcept {
        return size_approx() == 0;
    }

    /// Get queue capacity
    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    // Cache line size for padding
    static constexpr std::size_t kCacheLineSize = 64;

    struct Slot {
        std::atomic<std::size_t> sequence;
        alignas(T) std::byte storage[sizeof(T)];
    };

    // Pad to avoid false sharing between head and tail
    alignas(kCacheLineSize) std::atomic<std::size_t> head_;
    char pad1_[kCacheLineSize - sizeof(std::atomic<std::size_t>)];

    alignas(kCacheLineSize) std::atomic<std::size_t> tail_;
    char pad2_[kCacheLineSize - sizeof(std::atomic<std::size_t>)];

    std::array<Slot, Capacity> slots_;
};

}  // namespace titan

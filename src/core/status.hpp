#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace titan {

/// Result monad for error handling without exceptions
/// Inspired by Rust's Result<T, E>
template <typename T, typename E = std::string>
class Result {
public:
    /// Create a successful result
    [[nodiscard]] static Result Ok(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    /// Create an error result
    [[nodiscard]] static Result Err(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    /// Check if result is successful
    /// Uses index-based check to handle T==E case
    [[nodiscard]] bool is_ok() const noexcept {
        return data_.index() == 0;
    }

    /// Check if result is an error
    /// Uses index-based check to handle T==E case
    [[nodiscard]] bool is_err() const noexcept {
        return data_.index() == 1;
    }

    /// Get the value (throws if error)
    [[nodiscard]] const T& value() const& {
        if (is_err()) {
            throw std::runtime_error("Called value() on error Result");
        }
        return std::get<0>(data_);
    }

    /// Get the value (throws if error) - rvalue version
    [[nodiscard]] T value() && {
        if (is_err()) {
            throw std::runtime_error("Called value() on error Result");
        }
        return std::get<0>(std::move(data_));
    }

    /// Get the error (throws if ok)
    [[nodiscard]] const E& error() const& {
        if (is_ok()) {
            throw std::runtime_error("Called error() on ok Result");
        }
        return std::get<1>(data_);
    }

    /// Get value or default
    [[nodiscard]] T value_or(T default_value) const& {
        if (is_ok()) {
            return std::get<0>(data_);
        }
        return default_value;
    }

    /// Take the value (move semantics for move-only types)
    [[nodiscard]] T take_value() && {
        if (is_err()) {
            throw std::runtime_error("Called take_value() on error Result");
        }
        return std::get<0>(std::move(data_));
    }

    /// Transform the value if Ok, preserve error if Err
    template <typename F>
    [[nodiscard]] auto map(F&& func) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            return Result<U, E>::Ok(func(std::get<0>(data_)));
        }
        return Result<U, E>::Err(std::get<1>(data_));
    }

    /// Transform the error if Err, preserve value if Ok
    template <typename F>
    [[nodiscard]] auto map_error(F&& func) const& -> Result<T, std::invoke_result_t<F, const E&>> {
        using NewE = std::invoke_result_t<F, const E&>;
        if (is_err()) {
            return Result<T, NewE>::Err(func(std::get<1>(data_)));
        }
        return Result<T, NewE>::Ok(std::get<0>(data_));
    }

    /// Chain operations that may fail
    template <typename F>
    [[nodiscard]] auto and_then(F&& func) const& -> std::invoke_result_t<F, const T&> {
        using ResultType = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            return func(std::get<0>(data_));
        }
        return ResultType::Err(std::get<1>(data_));
    }

private:
    template <size_t I, typename... Args>
    explicit Result(std::in_place_index_t<I> tag, Args&&... args)
        : data_(tag, std::forward<Args>(args)...) {}

    std::variant<T, E> data_;
};

}  // namespace titan

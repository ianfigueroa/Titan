#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace titan {

namespace detail {

// Use double for intermediate multiplication to avoid overflow
// This maintains precision for typical crypto price ranges (up to ~1M with 8 decimals)
inline std::int64_t mul_div(std::int64_t a, std::int64_t b, std::int64_t divisor) {
    double result = (static_cast<double>(a) * static_cast<double>(b)) /
                    static_cast<double>(divisor);
    return static_cast<std::int64_t>(std::round(result));
}

inline std::int64_t mul_scale(std::int64_t a, std::int64_t scale, std::int64_t divisor) {
    double result = (static_cast<double>(a) * static_cast<double>(scale)) /
                    static_cast<double>(divisor);
    return static_cast<std::int64_t>(std::round(result));
}

}  // namespace detail

/// Fixed-point decimal type for precise financial calculations
/// Uses int64_t internally with configurable decimal places
/// @tparam Decimals Number of decimal places (e.g., 8 for crypto prices)
template <int Decimals>
class FixedPoint {
public:
    static_assert(Decimals >= 0 && Decimals <= 18,
                  "Decimals must be between 0 and 18");

    using underlying_type = std::int64_t;

    /// Scale factor (10^Decimals)
    static constexpr underlying_type scale = []() {
        underlying_type s = 1;
        for (int i = 0; i < Decimals; ++i) {
            s *= 10;
        }
        return s;
    }();

    /// Default constructor - zero value
    constexpr FixedPoint() noexcept : value_(0) {}

    /// Construct from raw underlying value
    static constexpr FixedPoint from_raw(underlying_type raw) noexcept {
        FixedPoint fp;
        fp.value_ = raw;
        return fp;
    }

    /// Construct from double (with rounding)
    explicit constexpr FixedPoint(double d) noexcept
        : value_(static_cast<underlying_type>(
              std::round(d * static_cast<double>(scale)))) {}

    /// Construct from integer
    template <typename T,
              typename = std::enable_if_t<std::is_integral_v<T>>>
    explicit constexpr FixedPoint(T i) noexcept
        : value_(static_cast<underlying_type>(i) * scale) {}

    /// Get raw underlying value
    [[nodiscard]] constexpr underlying_type raw() const noexcept {
        return value_;
    }

    /// Convert to double (for display/logging only)
    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(value_) / static_cast<double>(scale);
    }

    /// Convert to string with full precision
    [[nodiscard]] std::string to_string() const {
        if (value_ == 0) {
            return "0";
        }

        bool negative = value_ < 0;
        underlying_type abs_val = negative ? -value_ : value_;

        std::string result;
        underlying_type integer_part = abs_val / scale;
        underlying_type frac_part = abs_val % scale;

        result = std::to_string(integer_part);

        if (frac_part > 0) {
            result += '.';
            std::string frac_str = std::to_string(frac_part);
            // Pad with leading zeros if needed
            while (static_cast<int>(frac_str.length()) < Decimals) {
                frac_str = "0" + frac_str;
            }
            // Remove trailing zeros
            while (!frac_str.empty() && frac_str.back() == '0') {
                frac_str.pop_back();
            }
            result += frac_str;
        }

        if (negative) {
            result = "-" + result;
        }
        return result;
    }

    /// Parse from string
    /// @throws std::invalid_argument for invalid format
    /// @throws std::overflow_error if value is too large
    [[nodiscard]] static FixedPoint parse(const std::string& str) {
        if (str.empty()) {
            return FixedPoint{};
        }

        bool negative = false;
        std::size_t pos = 0;

        if (str[0] == '-') {
            negative = true;
            pos = 1;
        } else if (str[0] == '+') {
            pos = 1;
        }

        // Check for sign-only string
        if (pos >= str.length()) {
            throw std::invalid_argument("Invalid number format: sign only");
        }

        underlying_type integer_part = 0;
        underlying_type frac_part = 0;
        int frac_digits = 0;
        bool in_fraction = false;
        bool has_digits = false;

        // Maximum safe integer_part before multiplication would overflow
        constexpr underlying_type max_safe_int =
            std::numeric_limits<underlying_type>::max() / 10;

        for (; pos < str.length(); ++pos) {
            char c = str[pos];
            if (c == '.') {
                if (in_fraction) {
                    throw std::invalid_argument("Multiple decimal points");
                }
                in_fraction = true;
                continue;
            }
            if (c < '0' || c > '9') {
                throw std::invalid_argument("Invalid character in number");
            }

            has_digits = true;
            int digit = c - '0';
            if (in_fraction) {
                if (frac_digits < Decimals) {
                    frac_part = frac_part * 10 + digit;
                    ++frac_digits;
                }
                // Ignore digits beyond precision
            } else {
                // Check for overflow before multiplication
                if (integer_part > max_safe_int) {
                    throw std::overflow_error("Integer part overflow during parse");
                }
                integer_part = integer_part * 10 + digit;
            }
        }

        if (!has_digits) {
            throw std::invalid_argument("No digits found in number");
        }

        // Scale fractional part to correct position
        while (frac_digits < Decimals) {
            frac_part *= 10;
            ++frac_digits;
        }

        // Check for overflow in final calculation
        constexpr underlying_type max_integer_part =
            std::numeric_limits<underlying_type>::max() / scale;
        if (integer_part > max_integer_part) {
            throw std::overflow_error("Value too large for fixed-point representation");
        }

        underlying_type result = integer_part * scale + frac_part;
        if (negative) {
            result = -result;
        }

        return from_raw(result);
    }

    // Comparison operators
    [[nodiscard]] constexpr bool operator==(const FixedPoint& other) const noexcept {
        return value_ == other.value_;
    }

    [[nodiscard]] constexpr bool operator!=(const FixedPoint& other) const noexcept {
        return value_ != other.value_;
    }

    [[nodiscard]] constexpr bool operator<(const FixedPoint& other) const noexcept {
        return value_ < other.value_;
    }

    [[nodiscard]] constexpr bool operator<=(const FixedPoint& other) const noexcept {
        return value_ <= other.value_;
    }

    [[nodiscard]] constexpr bool operator>(const FixedPoint& other) const noexcept {
        return value_ > other.value_;
    }

    [[nodiscard]] constexpr bool operator>=(const FixedPoint& other) const noexcept {
        return value_ >= other.value_;
    }

    // Arithmetic operators
    [[nodiscard]] constexpr FixedPoint operator+(const FixedPoint& other) const noexcept {
        return from_raw(value_ + other.value_);
    }

    [[nodiscard]] constexpr FixedPoint operator-(const FixedPoint& other) const noexcept {
        return from_raw(value_ - other.value_);
    }

    [[nodiscard]] constexpr FixedPoint operator-() const noexcept {
        return from_raw(-value_);
    }

    /// Multiplication: result = (a * b) / scale
    [[nodiscard]] FixedPoint operator*(const FixedPoint& other) const noexcept {
        return from_raw(detail::mul_div(value_, other.value_, scale));
    }

    /// Division: result = (a * scale) / b
    /// NOTE: Division by zero returns zero by design. This is a deliberate safety
    /// choice for financial calculations where a zero result is often more desirable
    /// than a crash or exception. Code using this class should validate divisors
    /// before performing division if different behavior is needed.
    [[nodiscard]] FixedPoint operator/(const FixedPoint& other) const noexcept {
        if (other.value_ == 0) {
            return FixedPoint{}; // Return zero for division by zero
        }
        return from_raw(detail::mul_scale(value_, scale, other.value_));
    }

    // Compound assignment
    constexpr FixedPoint& operator+=(const FixedPoint& other) noexcept {
        value_ += other.value_;
        return *this;
    }

    constexpr FixedPoint& operator-=(const FixedPoint& other) noexcept {
        value_ -= other.value_;
        return *this;
    }

    constexpr FixedPoint& operator*=(const FixedPoint& other) noexcept {
        *this = *this * other;
        return *this;
    }

    constexpr FixedPoint& operator/=(const FixedPoint& other) noexcept {
        *this = *this / other;
        return *this;
    }

    /// Check if value is zero
    [[nodiscard]] constexpr bool is_zero() const noexcept {
        return value_ == 0;
    }

    /// Check if value is positive (> 0)
    [[nodiscard]] constexpr bool is_positive() const noexcept {
        return value_ > 0;
    }

    /// Check if value is negative (< 0)
    [[nodiscard]] constexpr bool is_negative() const noexcept {
        return value_ < 0;
    }

    /// Get absolute value
    [[nodiscard]] constexpr FixedPoint abs() const noexcept {
        return from_raw(value_ < 0 ? -value_ : value_);
    }

    /// Zero constant
    static constexpr FixedPoint zero() noexcept {
        return FixedPoint{};
    }

    /// One constant
    static constexpr FixedPoint one() noexcept {
        return from_raw(scale);
    }

private:
    underlying_type value_;
};

/// Hash specialization for FixedPoint
template <int Decimals>
struct FixedPointHash {
    std::size_t operator()(const FixedPoint<Decimals>& fp) const noexcept {
        return std::hash<typename FixedPoint<Decimals>::underlying_type>{}(fp.raw());
    }
};

}  // namespace titan

// Standard library hash specialization
namespace std {
template <int Decimals>
struct hash<titan::FixedPoint<Decimals>> {
    std::size_t operator()(const titan::FixedPoint<Decimals>& fp) const noexcept {
        return std::hash<typename titan::FixedPoint<Decimals>::underlying_type>{}(fp.raw());
    }
};
}  // namespace std

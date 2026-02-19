#include <gtest/gtest.h>
#include "core/fixed_point.hpp"
#include <limits>
#include <string>

using namespace titan;

// Use 8 decimal places to match the actual usage (FixedPrice)
using FP8 = FixedPoint<8>;

class FixedPointTest : public ::testing::Test {};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(FixedPointTest, DefaultConstructorIsZero) {
    FP8 fp;
    EXPECT_EQ(fp.raw(), 0);
    EXPECT_TRUE(fp.is_zero());
}

TEST_F(FixedPointTest, ConstructFromDouble) {
    FP8 fp(42.5);
    EXPECT_DOUBLE_EQ(fp.to_double(), 42.5);
}

TEST_F(FixedPointTest, ConstructFromInteger) {
    FP8 fp(100);
    EXPECT_EQ(fp.to_double(), 100.0);
}

TEST_F(FixedPointTest, ConstructFromNegativeDouble) {
    FP8 fp(-123.456);
    EXPECT_NEAR(fp.to_double(), -123.456, 1e-8);
}

TEST_F(FixedPointTest, FromRawValue) {
    auto fp = FP8::from_raw(12345678900000000LL);
    EXPECT_EQ(fp.raw(), 12345678900000000LL);
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST_F(FixedPointTest, Equality) {
    FP8 a(42.5);
    FP8 b(42.5);
    FP8 c(42.6);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST_F(FixedPointTest, LessThan) {
    FP8 a(42.5);
    FP8 b(42.6);

    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_FALSE(a < a);
}

TEST_F(FixedPointTest, LessThanOrEqual) {
    FP8 a(42.5);
    FP8 b(42.6);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a <= a);
    EXPECT_FALSE(b <= a);
}

TEST_F(FixedPointTest, GreaterThan) {
    FP8 a(42.5);
    FP8 b(42.6);

    EXPECT_TRUE(b > a);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a > a);
}

TEST_F(FixedPointTest, GreaterThanOrEqual) {
    FP8 a(42.5);
    FP8 b(42.6);

    EXPECT_TRUE(b >= a);
    EXPECT_TRUE(a >= a);
    EXPECT_FALSE(a >= b);
}

// ============================================================================
// Arithmetic Tests
// ============================================================================

TEST_F(FixedPointTest, Addition) {
    FP8 a(100.5);
    FP8 b(50.25);
    FP8 result = a + b;

    EXPECT_DOUBLE_EQ(result.to_double(), 150.75);
}

TEST_F(FixedPointTest, Subtraction) {
    FP8 a(100.5);
    FP8 b(50.25);
    FP8 result = a - b;

    EXPECT_DOUBLE_EQ(result.to_double(), 50.25);
}

TEST_F(FixedPointTest, SubtractionResultingInNegative) {
    FP8 a(50.0);
    FP8 b(100.0);
    FP8 result = a - b;

    EXPECT_DOUBLE_EQ(result.to_double(), -50.0);
    EXPECT_TRUE(result.is_negative());
}

TEST_F(FixedPointTest, Negation) {
    FP8 a(42.5);
    FP8 neg = -a;

    EXPECT_DOUBLE_EQ(neg.to_double(), -42.5);
}

TEST_F(FixedPointTest, Multiplication) {
    FP8 a(10.0);
    FP8 b(5.5);
    FP8 result = a * b;

    EXPECT_DOUBLE_EQ(result.to_double(), 55.0);
}

TEST_F(FixedPointTest, MultiplicationWithDecimals) {
    FP8 a(2.5);
    FP8 b(4.0);
    FP8 result = a * b;

    EXPECT_DOUBLE_EQ(result.to_double(), 10.0);
}

// Suppress deprecation warnings for tests of legacy operator/
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

TEST_F(FixedPointTest, Division) {
    FP8 a(100.0);
    FP8 b(4.0);
    FP8 result = a / b;

    EXPECT_DOUBLE_EQ(result.to_double(), 25.0);
}

TEST_F(FixedPointTest, DivisionWithDecimals) {
    FP8 a(10.0);
    FP8 b(4.0);
    FP8 result = a / b;

    EXPECT_DOUBLE_EQ(result.to_double(), 2.5);
}

TEST_F(FixedPointTest, DivisionByZeroReturnsZero) {
    // NOTE: Division by zero returns zero by design (see fixed_point.hpp documentation)
    // This is a deliberate design choice for safety in financial calculations
    // where a zero result is often more desirable than a crash/exception
    FP8 a(100.0);
    FP8 b(0.0);
    FP8 result = a / b;

    EXPECT_TRUE(result.is_zero());
}

TEST_F(FixedPointTest, CompoundAddition) {
    FP8 a(100.0);
    a += FP8(50.0);
    EXPECT_DOUBLE_EQ(a.to_double(), 150.0);
}

TEST_F(FixedPointTest, CompoundSubtraction) {
    FP8 a(100.0);
    a -= FP8(30.0);
    EXPECT_DOUBLE_EQ(a.to_double(), 70.0);
}

TEST_F(FixedPointTest, CompoundMultiplication) {
    FP8 a(10.0);
    a *= FP8(5.0);
    EXPECT_DOUBLE_EQ(a.to_double(), 50.0);
}

TEST_F(FixedPointTest, CompoundDivision) {
    FP8 a(100.0);
    a /= FP8(4.0);
    EXPECT_DOUBLE_EQ(a.to_double(), 25.0);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

// ============================================================================
// Parsing Tests
// ============================================================================

TEST_F(FixedPointTest, ParseInteger) {
    auto fp = FP8::parse("42150");
    EXPECT_DOUBLE_EQ(fp.to_double(), 42150.0);
}

TEST_F(FixedPointTest, ParseDecimal) {
    auto fp = FP8::parse("42150.50");
    EXPECT_DOUBLE_EQ(fp.to_double(), 42150.50);
}

TEST_F(FixedPointTest, ParseWithLeadingPlus) {
    auto fp = FP8::parse("+42150.50");
    EXPECT_DOUBLE_EQ(fp.to_double(), 42150.50);
}

TEST_F(FixedPointTest, ParseNegative) {
    auto fp = FP8::parse("-42150.50");
    EXPECT_DOUBLE_EQ(fp.to_double(), -42150.50);
}

TEST_F(FixedPointTest, ParseZero) {
    auto fp = FP8::parse("0");
    EXPECT_TRUE(fp.is_zero());
}

TEST_F(FixedPointTest, ParseZeroWithDecimals) {
    auto fp = FP8::parse("0.00");
    EXPECT_TRUE(fp.is_zero());
}

TEST_F(FixedPointTest, ParseEmpty) {
    auto fp = FP8::parse("");
    EXPECT_TRUE(fp.is_zero());
}

TEST_F(FixedPointTest, ParseSmallDecimal) {
    auto fp = FP8::parse("0.00000001");
    EXPECT_EQ(fp.raw(), 1);  // 1 in the smallest unit
}

TEST_F(FixedPointTest, ParseManyDecimals) {
    // Extra decimals beyond precision should be ignored
    auto fp = FP8::parse("1.123456789999");
    EXPECT_NEAR(fp.to_double(), 1.12345678, 1e-8);
}

TEST_F(FixedPointTest, ParseThrowsOnMultipleDecimals) {
    EXPECT_THROW((void)FP8::parse("42.15.50"), std::invalid_argument);
}

TEST_F(FixedPointTest, ParseThrowsOnInvalidChar) {
    EXPECT_THROW((void)FP8::parse("42.15abc"), std::invalid_argument);
}

TEST_F(FixedPointTest, ParseThrowsOnOnlySign) {
    // An implementation decision - this currently throws
    EXPECT_THROW((void)FP8::parse("-"), std::invalid_argument);
}

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_F(FixedPointTest, ToStringZero) {
    FP8 fp(0.0);
    EXPECT_EQ(fp.to_string(), "0");
}

TEST_F(FixedPointTest, ToStringInteger) {
    FP8 fp(42150.0);
    EXPECT_EQ(fp.to_string(), "42150");
}

TEST_F(FixedPointTest, ToStringDecimal) {
    FP8 fp(42150.5);
    EXPECT_EQ(fp.to_string(), "42150.5");
}

TEST_F(FixedPointTest, ToStringNegative) {
    FP8 fp(-42150.5);
    EXPECT_EQ(fp.to_string(), "-42150.5");
}

TEST_F(FixedPointTest, ToStringSmallDecimal) {
    auto fp = FP8::from_raw(1);  // Smallest representable value
    EXPECT_EQ(fp.to_string(), "0.00000001");
}

TEST_F(FixedPointTest, RoundTripConversion) {
    std::vector<std::string> test_values = {
        "0",
        "42150.5",
        "100",
        "0.12345678",
        "99999.99999999"
    };

    for (const auto& s : test_values) {
        auto fp = FP8::parse(s);
        EXPECT_EQ(fp.to_string(), s) << "Failed round-trip for: " << s;
    }
}

// ============================================================================
// Utility Method Tests
// ============================================================================

TEST_F(FixedPointTest, IsZero) {
    EXPECT_TRUE(FP8(0.0).is_zero());
    EXPECT_FALSE(FP8(0.00000001).is_zero());
    EXPECT_FALSE(FP8(100.0).is_zero());
}

TEST_F(FixedPointTest, IsPositive) {
    EXPECT_TRUE(FP8(1.0).is_positive());
    EXPECT_TRUE(FP8(0.00000001).is_positive());
    EXPECT_FALSE(FP8(0.0).is_positive());
    EXPECT_FALSE(FP8(-1.0).is_positive());
}

TEST_F(FixedPointTest, IsNegative) {
    EXPECT_TRUE(FP8(-1.0).is_negative());
    EXPECT_FALSE(FP8(0.0).is_negative());
    EXPECT_FALSE(FP8(1.0).is_negative());
}

TEST_F(FixedPointTest, Abs) {
    EXPECT_DOUBLE_EQ(FP8(42.5).abs().to_double(), 42.5);
    EXPECT_DOUBLE_EQ(FP8(-42.5).abs().to_double(), 42.5);
    EXPECT_DOUBLE_EQ(FP8(0.0).abs().to_double(), 0.0);
}

TEST_F(FixedPointTest, StaticZero) {
    EXPECT_TRUE(FP8::zero().is_zero());
}

TEST_F(FixedPointTest, StaticOne) {
    EXPECT_DOUBLE_EQ(FP8::one().to_double(), 1.0);
}

// ============================================================================
// Map Key Behavior Tests (Critical for Order Book)
// ============================================================================

TEST_F(FixedPointTest, MapKeyBehavior) {
    std::map<FP8, int> price_map;

    // Insert prices
    price_map[FP8::parse("42150.50")] = 100;
    price_map[FP8::parse("42150.51")] = 200;
    price_map[FP8::parse("42150.52")] = 300;

    // Exact lookup should work
    EXPECT_EQ(price_map[FP8::parse("42150.50")], 100);
    EXPECT_EQ(price_map[FP8::parse("42150.51")], 200);
    EXPECT_EQ(price_map[FP8::parse("42150.52")], 300);

    // Parse again - should find same key (no floating point issues)
    auto key = FP8::parse("42150.50");
    EXPECT_EQ(price_map.count(key), 1u);
}

TEST_F(FixedPointTest, MapKeyOrdering) {
    std::map<FP8, int> price_map;

    price_map[FP8(42152.0)] = 3;
    price_map[FP8(42150.0)] = 1;
    price_map[FP8(42151.0)] = 2;

    // Should iterate in ascending order
    auto it = price_map.begin();
    EXPECT_DOUBLE_EQ(it->first.to_double(), 42150.0);
    ++it;
    EXPECT_DOUBLE_EQ(it->first.to_double(), 42151.0);
    ++it;
    EXPECT_DOUBLE_EQ(it->first.to_double(), 42152.0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(FixedPointTest, SmallestPositiveValue) {
    auto fp = FP8::from_raw(1);
    EXPECT_GT(fp, FP8::zero());
    EXPECT_DOUBLE_EQ(fp.to_double(), 0.00000001);
}

TEST_F(FixedPointTest, LargeValues) {
    // BTC at 100,000 USDT should work fine
    FP8 price(100000.0);
    FP8 qty(1.5);
    FP8 value = price * qty;

    EXPECT_DOUBLE_EQ(value.to_double(), 150000.0);
}

TEST_F(FixedPointTest, VerySmallMultiplication) {
    FP8 a(0.0001);
    FP8 b(0.0001);
    FP8 result = a * b;

    EXPECT_NEAR(result.to_double(), 0.00000001, 1e-8);
}

// ============================================================================
// Hash Support Tests (for potential use in unordered containers)
// ============================================================================

TEST_F(FixedPointTest, HashConsistency) {
    std::hash<FP8> hasher;

    FP8 a(42150.50);
    FP8 b(42150.50);
    FP8 c(42150.51);

    EXPECT_EQ(hasher(a), hasher(b));
    EXPECT_NE(hasher(a), hasher(c));
}

// ============================================================================
// Safe Division Tests (try_divide)
// ============================================================================

TEST_F(FixedPointTest, TryDivideReturnsNulloptOnDivisionByZero) {
    FP8 a(100.0);
    FP8 zero(0.0);

    auto result = a.try_divide(zero);

    EXPECT_FALSE(result.has_value());
}

TEST_F(FixedPointTest, TryDivideReturnsValueOnValidDivision) {
    FP8 a(100.0);
    FP8 b(4.0);

    auto result = a.try_divide(b);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->to_double(), 25.0);
}

TEST_F(FixedPointTest, TryDivideWithDecimals) {
    FP8 a(10.0);
    FP8 b(4.0);

    auto result = a.try_divide(b);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->to_double(), 2.5);
}

TEST_F(FixedPointTest, TryDivideNegativeNumbers) {
    FP8 a(-100.0);
    FP8 b(4.0);

    auto result = a.try_divide(b);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->to_double(), -25.0);
}

TEST_F(FixedPointTest, TryDivideZeroDividend) {
    FP8 zero(0.0);
    FP8 b(4.0);

    auto result = zero.try_divide(b);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->to_double(), 0.0);
}

TEST_F(FixedPointTest, TryDivideNegativeDivisor) {
    FP8 a(100.0);
    FP8 b(-4.0);

    auto result = a.try_divide(b);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->to_double(), -25.0);
}

TEST_F(FixedPointTest, TryDivideBothNegative) {
    FP8 a(-100.0);
    FP8 b(-4.0);

    auto result = a.try_divide(b);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->to_double(), 25.0);
}

#include <gtest/gtest.h>
#include "core/status.hpp"
#include <string>

using namespace titan;

// Test Result<T, E> monad

TEST(StatusTest, OkCreation) {
    auto result = Result<int, std::string>::Ok(42);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_EQ(result.value(), 42);
}

TEST(StatusTest, ErrCreation) {
    auto result = Result<int, std::string>::Err("something failed");
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), "something failed");
}

TEST(StatusTest, ValueThrowsOnError) {
    auto result = Result<int, std::string>::Err("error");
    EXPECT_THROW(result.value(), std::runtime_error);
}

TEST(StatusTest, ErrorThrowsOnOk) {
    auto result = Result<int, std::string>::Ok(42);
    EXPECT_THROW(result.error(), std::runtime_error);
}

TEST(StatusTest, MapTransformsValue) {
    auto result = Result<int, std::string>::Ok(10);
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 20);
}

TEST(StatusTest, MapPreservesError) {
    auto result = Result<int, std::string>::Err("oops");
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error(), "oops");
}

TEST(StatusTest, AndThenChainsOperations) {
    auto divide = [](int x) -> Result<int, std::string> {
        if (x == 0) {
            return Result<int, std::string>::Err("division by zero");
        }
        return Result<int, std::string>::Ok(100 / x);
    };

    auto result = Result<int, std::string>::Ok(10);
    auto chained = result.and_then(divide);

    EXPECT_TRUE(chained.is_ok());
    EXPECT_EQ(chained.value(), 10);
}

TEST(StatusTest, AndThenPropagatesError) {
    auto divide = [](int x) -> Result<int, std::string> {
        if (x == 0) {
            return Result<int, std::string>::Err("division by zero");
        }
        return Result<int, std::string>::Ok(100 / x);
    };

    auto result = Result<int, std::string>::Ok(0);
    auto chained = result.and_then(divide);

    EXPECT_TRUE(chained.is_err());
    EXPECT_EQ(chained.error(), "division by zero");
}

TEST(StatusTest, AndThenSkipsOnError) {
    auto divide = [](int x) -> Result<int, std::string> {
        return Result<int, std::string>::Ok(100 / x);
    };

    auto result = Result<int, std::string>::Err("already failed");
    auto chained = result.and_then(divide);

    EXPECT_TRUE(chained.is_err());
    EXPECT_EQ(chained.error(), "already failed");
}

TEST(StatusTest, ValueOrReturnsValueOnOk) {
    auto result = Result<int, std::string>::Ok(42);
    EXPECT_EQ(result.value_or(0), 42);
}

TEST(StatusTest, ValueOrReturnsDefaultOnError) {
    auto result = Result<int, std::string>::Err("error");
    EXPECT_EQ(result.value_or(99), 99);
}

TEST(StatusTest, MapErrorTransformsError) {
    auto result = Result<int, int>::Err(404);
    auto mapped = result.map_error([](int code) {
        return "Error code: " + std::to_string(code);
    });

    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error(), "Error code: 404");
}

TEST(StatusTest, MapErrorPreservesOk) {
    auto result = Result<int, int>::Ok(42);
    auto mapped = result.map_error([](int code) {
        return "Error code: " + std::to_string(code);
    });

    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 42);
}

TEST(StatusTest, WorksWithMoveOnlyTypes) {
    auto result = Result<std::unique_ptr<int>, std::string>::Ok(
        std::make_unique<int>(42)
    );

    EXPECT_TRUE(result.is_ok());
    auto ptr = std::move(result).take_value();
    EXPECT_EQ(*ptr, 42);
}

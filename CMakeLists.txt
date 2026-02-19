cmake_minimum_required(VERSION 3.20)
project(titan VERSION 1.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(TITAN_BUILD_TESTS "Build unit tests" ON)
option(TITAN_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# Find system dependencies
find_package(Boost 1.74 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Fetch external dependencies
include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(json spdlog)

# Collect source files
set(TITAN_SOURCES
    src/core/config.cpp
    src/network/ssl_context.cpp
    src/network/websocket_client.cpp
    src/network/rest_client.cpp
    src/binance/message_parser.cpp
    src/binance/feed_handler.cpp
    src/orderbook/order_book.cpp
    src/orderbook/book_metrics.cpp
    src/trade/vwap_calculator.cpp
    src/trade/alert_detector.cpp
    src/trade/trade_flow.cpp
    src/output/console_logger.cpp
    src/output/websocket_server.cpp
    src/output/json_formatter.cpp
    src/engine/market_data_engine.cpp
    src/engine/reconnect_strategy.cpp
)

# Static library for testability
add_library(titan_lib STATIC ${TITAN_SOURCES})

target_include_directories(titan_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(titan_lib PUBLIC
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Windows-specific socket libraries
if(WIN32)
    target_link_libraries(titan_lib PUBLIC ws2_32 mswsock)
endif()

set_project_warnings(titan_lib)

if(TITAN_ENABLE_SANITIZERS)
    enable_sanitizers(titan_lib)
endif()

# Main executable
add_executable(titan src/main.cpp)
target_link_libraries(titan PRIVATE titan_lib)
set_project_warnings(titan)

if(TITAN_ENABLE_SANITIZERS)
    enable_sanitizers(titan)
endif()

# Tests
if(TITAN_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    # Prevent gtest from overriding compiler flags on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)

    # Unit tests
    add_executable(titan_tests
        tests/unit/test_spsc_queue.cpp
        tests/unit/test_order_book.cpp
        tests/unit/test_vwap.cpp
        tests/unit/test_message_parser.cpp
        tests/unit/test_status.cpp
        tests/unit/test_config.cpp
        tests/unit/test_fixed_point.cpp
    )

    target_link_libraries(titan_tests PRIVATE
        titan_lib
        GTest::gtest_main
    )

    set_project_warnings(titan_tests)

    if(TITAN_ENABLE_SANITIZERS)
        enable_sanitizers(titan_tests)
    endif()

    gtest_discover_tests(titan_tests)

    # Integration tests
    add_executable(titan_integration_tests
        tests/integration/test_pipeline.cpp
    )

    target_link_libraries(titan_integration_tests PRIVATE
        titan_lib
        GTest::gtest_main
    )

    set_project_warnings(titan_integration_tests)

    if(TITAN_ENABLE_SANITIZERS)
        enable_sanitizers(titan_integration_tests)
    endif()

    gtest_discover_tests(titan_integration_tests)
endif()

# Benchmarks (optional)
option(TITAN_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
if(TITAN_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    # SPSC Queue benchmark
    add_executable(bench_spsc_queue benchmarks/bench_spsc_queue.cpp)
    target_link_libraries(bench_spsc_queue PRIVATE titan_lib benchmark::benchmark)

    # Order Book benchmark
    add_executable(bench_order_book benchmarks/bench_order_book.cpp)
    target_link_libraries(bench_order_book PRIVATE titan_lib benchmark::benchmark)

    # VWAP benchmark
    add_executable(bench_vwap benchmarks/bench_vwap.cpp)
    target_link_libraries(bench_vwap PRIVATE titan_lib benchmark::benchmark)
endif()

# Install targets
include(GNUInstallDirs)
install(TARGETS titan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config.example.json DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/titan RENAME config.json)

# CPack configuration
set(CPACK_PACKAGE_NAME "titan")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Real-time cryptocurrency market data engine")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
include(CPack)

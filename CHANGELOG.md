# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-02-19

### Added
- Safe division method `try_divide()` on FixedPoint that returns `std::optional`
- Integration tests for message pipeline covering queue, order book, and VWAP
- Performance benchmarks for SPSC queue, order book, and VWAP calculator
- Optional benchmark build target with `TITAN_BUILD_BENCHMARKS` cmake option

### Changed
- Deprecated `operator/` and `operator/=` on FixedPoint (use `try_divide()` instead)

### Fixed
- Silent division by zero in FixedPoint now returns explicit failure via optional

## [1.0.0] - 2026-02-18

### Added
- CI/CD pipeline for Linux, macOS, and Docker
- Docker publishing to GitHub Container Registry
- SSL certificate support in Docker image

## [0.1.0] - 2025-02-18

### Added
- Real-time order book with gap detection and automatic resync
- VWAP calculation with fixed-point precision
- WebSocket server streaming JSON metrics on port 9001
- Whale trade alerts using sigma-based detection
- Docker support with docker-compose
- JSON configuration with environment variable overrides
- CLI arguments (`-c`, `-s`, `-h`, `-v`)
- Lock-free SPSC queue for thread communication
- Exponential backoff with jitter for reconnection

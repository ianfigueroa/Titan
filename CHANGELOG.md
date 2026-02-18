# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

# Titan.cpp

[![Build](https://github.com/ianfigueroa/Titan/actions/workflows/build.yml/badge.svg)](https://github.com/ianfigueroa/Titan/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/ianfigueroa/Titan)](https://github.com/ianfigueroa/Titan/releases)

Real-time market data infrastructure for cryptocurrency trading. Connects to Binance Futures WebSocket, maintains a local order book, and streams processed data.

## Try It Now (No Build Required)

1. Download the latest binary from [Releases](https://github.com/ianfigueroa/Titan/releases)
2. Run: `./titan`
3. Open `examples/web/index.html` in your browser to see live data

## Features

- **Live Order Book**: Real-time depth updates with sequence gap detection
- **Trade Flow Analysis**: VWAP, volume tracking, large trade alerts
- **WebSocket Server**: Stream processed data to clients on `ws://localhost:9001`
- **Robust Reconnection**: Exponential backoff with jitter
- **Lock-free Architecture**: SPSC queue for zero-copy thread communication
- **Fixed-Point Arithmetic**: Precise price calculations without floating-point errors

## Quick Start

```bash
# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run with default settings (BTCUSDT)
./titan

# Run with config file
./titan -c ../config.example.json

# Run with different symbol
./titan -s ethusdt
```

## Requirements

- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.20+
- Boost 1.80+ (system component)
- OpenSSL

## Build Instructions

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake libboost-all-dev libssl-dev

# Clone and build
git clone https://github.com/ianfigueroa/Titan.git titan.cpp
cd titan.cpp
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./titan
```

### macOS

```bash
# Install dependencies
brew install cmake boost openssl

# Clone and build
git clone https://github.com/ianfigueroa/Titan.git titan.cpp
cd titan.cpp
mkdir build && cd build
cmake .. -DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
make -j$(sysctl -n hw.ncpu)

# Run
./titan
```

### Windows (MSYS2)

```bash
# Install dependencies in MSYS2 MinGW64 shell
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-gcc

# Clone and build
git clone https://github.com/ianfigueroa/Titan.git titan.cpp
cd titan.cpp
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j$(nproc)

# Run
./titan.exe
```

### Build Options

```bash
# Release build (default)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# With sanitizers (memory error detection)
cmake .. -DTITAN_ENABLE_SANITIZERS=ON

# Without tests
cmake .. -DTITAN_BUILD_TESTS=OFF
```

## Usage

### Command Line Options

```
./titan [OPTIONS]

Options:
  -c, --config FILE    Load configuration from JSON file
  -s, --symbol SYMBOL  Trading symbol (e.g., btcusdt, ethusdt)
  -h, --help           Show help message
  -v, --version        Show version
```

### Examples

```bash
# Default: BTCUSDT with default settings
./titan

# Use a config file
./titan -c config.json

# Override symbol
./titan -s ethusdt

# Both: config file with symbol override
./titan -c config.json -s solusdt
```

### Sample Output

```
titan

[2026-02-17 20:38:22.443] [info] Starting titan market data engine
[2026-02-17 20:38:22.443] [info] Symbol: btcusdt
[2026-02-17 20:38:22.500] [info] WebSocket connected
[2026-02-17 20:38:22.800] [info] Sync: Synchronized
[2026-02-17 20:38:23.000] [info] BID: 67542.20 (3.24) | ASK: 67542.30 (1.93) | SPREAD: 0.0bps | IMB: +17% | VWAP: 67542.30 | TRADES: 5
[2026-02-17 20:38:42.188] [warning] ALERT: LARGE BUY 0.318 BTC @ 67559.30 (2.3 sigma)
```

**Output Fields:**
- `BID/ASK`: Best bid/ask price with quantity in parentheses
- `SPREAD`: Bid-ask spread in basis points
- `IMB`: Order book imbalance (+100% = all bids, -100% = all asks)
- `VWAP`: Volume-weighted average price
- `TRADES`: Trade count in the window

### WebSocket Server

Connect to `ws://localhost:9001` to receive JSON data:

```bash
# Using wscat
npm install -g wscat
wscat -c ws://localhost:9001
```

JSON messages:
```json
{
  "type": "metrics",
  "timestamp": "2026-02-17T20:38:23.000Z",
  "book": {
    "bestBid": 67542.20,
    "bestAsk": 67542.30,
    "spreadBps": 0.15,
    "imbalance": 0.17
  },
  "trade": {
    "vwap": 67542.30,
    "buyVolume": 5.2,
    "sellVolume": 3.1,
    "tradeCount": 42
  }
}
```

Alert messages:
```json
{
  "type": "alert",
  "timestamp": "2026-02-17T20:38:42.188Z",
  "side": "BUY",
  "price": 67559.30,
  "quantity": 0.318,
  "sigma": 2.3
}
```

### Graceful Shutdown

Press `Ctrl+C` to shut down gracefully:
```
^C
Shutdown requested...
[info] Engine shutdown complete
Goodbye!
```

## Configuration

Configuration priority (highest to lowest):
1. Command line arguments (`-s`)
2. Environment variables (`TITAN_*`)
3. Config file (`-c`)
4. Default values

### Config File (JSON)

See `config.example.json` for a full example:

```json
{
  "network": {
    "symbol": "btcusdt",
    "ws_host": "fstream.binance.com",
    "rest_host": "fapi.binance.com"
  },
  "engine": {
    "vwap_window": 100,
    "large_trade_std_devs": 2.0,
    "depth_limit": 1000
  },
  "output": {
    "console_interval_ms": 500,
    "ws_server_port": 9001,
    "imbalance_levels": 10
  }
}
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `TITAN_SYMBOL` | Trading pair | `btcusdt` |
| `TITAN_WS_HOST` | WebSocket host | `fstream.binance.com` |
| `TITAN_REST_HOST` | REST API host | `fapi.binance.com` |
| `TITAN_CONSOLE_INTERVAL_MS` | Output interval (ms) | `500` |
| `TITAN_WS_SERVER_PORT` | WebSocket server port | `9001` |
| `TITAN_VWAP_WINDOW` | VWAP trade window | `100` |
| `TITAN_LARGE_TRADE_STD_DEVS` | Alert threshold | `2.0` |

Example:
```bash
TITAN_SYMBOL=ethusdt TITAN_WS_SERVER_PORT=9002 ./titan
```

### Configuration Reference

| Setting | Default | Description |
|---------|---------|-------------|
| `network.symbol` | `btcusdt` | Trading pair (lowercase) |
| `network.ws_host` | `fstream.binance.com` | Binance Futures WebSocket |
| `network.rest_host` | `fapi.binance.com` | Binance Futures REST API |
| `engine.vwap_window` | `100` | Number of trades in VWAP |
| `engine.large_trade_std_devs` | `2.0` | Sigma threshold for alerts |
| `engine.depth_limit` | `1000` | Max order book levels |
| `output.console_interval_ms` | `500` | Console output rate |
| `output.ws_server_port` | `9001` | WebSocket server port |
| `output.imbalance_levels` | `10` | Levels for imbalance calc |

## Docker

Build and run with Docker:

```bash
# Build the image
docker build -t titan .

# Run with default config
docker run -p 9001:9001 titan

# Run with custom config
docker run -p 9001:9001 -v $(pwd)/config.json:/etc/titan/config.json:ro titan

# Using docker-compose
docker-compose up -d
```

Connect to the WebSocket server at `ws://localhost:9001`.

## Testing

```bash
cd build

# Run all tests
ctest --output-on-failure

# Or run directly
./titan_tests
```

## Architecture

```
┌─────────────────┐     ┌─────────────────┐
│  Binance WS     │────▶│  FeedHandler    │
│  (depth+trade)  │     │  (parse/sync)   │
└─────────────────┘     └────────┬────────┘
                                 │
                                 ▼
┌─────────────────┐     ┌─────────────────┐
│  REST API       │────▶│  SPSC Queue     │
│  (snapshots)    │     │  (lock-free)    │
└─────────────────┘     └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  Engine Thread  │
                        │  ┌───────────┐  │
                        │  │ OrderBook │  │
                        │  │ TradeFlow │  │
                        │  └───────────┘  │
                        └────────┬────────┘
                                 │
                    ┌────────────┴────────────┐
                    ▼                         ▼
           ┌─────────────┐           ┌─────────────┐
           │   Console   │           │  WS Server  │
           │   Output    │           │  (9001)     │
           └─────────────┘           └─────────────┘
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed design documentation.

## License

MIT

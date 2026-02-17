# Titan.cpp

Real-time market data infrastructure for cryptocurrency trading. Connects to Binance Futures WebSocket, maintains a local order book, and streams processed data.

## Features

- **Live Order Book**: Real-time depth updates with sequence gap detection
- **Trade Flow Analysis**: VWAP, volume tracking, large trade alerts
- **WebSocket Server**: Stream processed data to clients on `ws://localhost:9001`
- **Robust Reconnection**: Exponential backoff with jitter
- **Lock-free Architecture**: SPSC queue for zero-copy thread communication

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
git clone <repo-url> titan.cpp
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
git clone <repo-url> titan.cpp
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
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-openssl

# Clone and build
git clone <repo-url> titan.cpp
cd titan.cpp
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j$(nproc)

# Run
./titan.exe
```

### Build with Sanitizers

```bash
cmake .. -DTITAN_ENABLE_SANITIZERS=ON
make
./titan  # Will detect memory errors and undefined behavior
```

## Running

```bash
./titan
```

Output:
```
[2024-01-15 10:30:45.123] [info] Starting titan market data engine
[2024-01-15 10:30:45.124] [info] Symbol: btcusdt
[2024-01-15 10:30:45.500] [info] WebSocket connected
[2024-01-15 10:30:45.800] [info] Sync: Synchronized
[2024-01-15 10:30:46.000] [info] BID: 42150.50 (2.5) | ASK: 42150.75 (1.8) | SPREAD: 0.6bps | IMB: +12% | VWAP: 42148.32 | TRADES: 142
```

### WebSocket Client

Connect to `ws://localhost:9001` to receive JSON data:

```bash
# Using wscat
npm install -g wscat
wscat -c ws://localhost:9001
```

JSON format:
```json
{
  "type": "metrics",
  "timestamp": "2024-01-15T10:30:45.123Z",
  "book": {
    "bestBid": 42150.50,
    "bestAsk": 42150.75,
    "spreadBps": 0.6,
    "imbalance": 0.12
  },
  "trade": {
    "vwap": 42148.32,
    "netFlow": 15.3,
    "tradeCount": 1542
  }
}
```

### Graceful Shutdown

Press `Ctrl+C` to gracefully shutdown:
```
^C
Shutdown requested...
[info] Engine shutdown complete
Goodbye!
```

## Testing

```bash
cd build
ctest --output-on-failure
```

## Configuration

Default configuration is hardcoded in `src/core/config.hpp`. Key settings:

| Setting | Default | Description |
|---------|---------|-------------|
| `network.symbol` | `btcusdt` | Trading pair |
| `output.console_interval` | `500ms` | Console output rate |
| `output.ws_server_port` | `9001` | WebSocket server port |
| `engine.vwap_window` | `100` | Trades in VWAP calculation |
| `engine.large_trade_std_devs` | `2.0` | Alert threshold |

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed design documentation.

## License

MIT

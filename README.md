# Titan.cpp

[![Build](https://github.com/ianfigueroa/Titan/actions/workflows/build.yml/badge.svg)](https://github.com/ianfigueroa/Titan/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/ianfigueroa/Titan)](https://github.com/ianfigueroa/Titan/releases)

A C++ market data engine for Binance Futures. It keeps a local order book in sync with the exchange, computes VWAP, spread, imbalance and whale-trade alerts, and streams them to whatever you connect over WebSocket. I wrote it so TapeFlow and a few scripts could share one Binance connection and get the number crunching done in C++ instead of in the browser.

## How it fits

Titan runs as its own process and your app talks to it over a local WebSocket:

```
Your App (Python/Node/Go/etc)
       │
       │ ws://localhost:9001
       ▼
┌─────────────────────────────────┐
│           Titan.cpp             │
├─────────────────────────────────┤
│  • Real-time order book         │
│  • VWAP calculation             │
│  • Whale trade detection        │
│  • Spread & imbalance metrics   │
└─────────────────────────────────┘
       │
       │ WebSocket
       ▼
   Binance Futures API
```

What you get over connecting to Binance yourself:
- VWAP, imbalance and spread in bps already computed
- whale alerts when a trade is more than N standard deviations above the recent mean
- an order book that detects sequence gaps and resyncs from a REST snapshot on its own
- fixed-point prices, so no float drift in the money math
- one exchange connection shared by every client

## Installation

### Option 1: Docker (Recommended)

```bash
docker pull ghcr.io/ianfigueroa/titan:latest
docker run -p 9001:9001 ghcr.io/ianfigueroa/titan
```

### Option 2: Download Binary

Download from [Releases](https://github.com/ianfigueroa/Titan/releases):
```bash
# Linux
wget https://github.com/ianfigueroa/Titan/releases/latest/download/titan-linux-x64
chmod +x titan-linux-x64
./titan-linux-x64

# macOS
wget https://github.com/ianfigueroa/Titan/releases/latest/download/titan-macos-x64
chmod +x titan-macos-x64
./titan-macos-x64
```

### Option 3: Build from Source

```bash
# Ubuntu/Debian
sudo apt install -y build-essential cmake libboost-all-dev libssl-dev
git clone https://github.com/ianfigueroa/Titan.git
cd Titan && mkdir build && cd build
cmake .. && make -j$(nproc)
./titan
```

## Connect Your Application

Once Titan is running, connect via WebSocket at `ws://localhost:9001`.

### Python

```python
import asyncio
import json
import websockets

async def main():
    async with websockets.connect("ws://localhost:9001") as ws:
        async for message in ws:
            data = json.loads(message)
            if data["type"] == "metrics":
                print(f"VWAP: {data['trade']['vwap']:.2f}")
            elif data["type"] == "alert":
                print(f"WHALE {data['side']}: {data['quantity']} @ {data['price']}")

asyncio.run(main())
```

### JavaScript/Node.js

```javascript
const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:9001');

ws.on('message', (data) => {
  const msg = JSON.parse(data);
  if (msg.type === 'metrics') {
    console.log(`VWAP: ${msg.trade.vwap.toFixed(2)}`);
  } else if (msg.type === 'alert') {
    console.log(`WHALE ${msg.side}: ${msg.quantity} @ ${msg.price}`);
  }
});
```

### Web Browser

```javascript
const ws = new WebSocket('ws://localhost:9001');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  // Update your UI
};
```

See the `examples/` folder for complete working examples in Python, Node.js, and a web dashboard.

## WebSocket API

Titan streams two types of messages:

### Metrics (every 500ms)

```json
{
  "type": "metrics",
  "timestamp": "2025-02-18T20:38:23.000Z",
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

| Field | Description |
|-------|-------------|
| `book.bestBid` | Best bid price |
| `book.bestAsk` | Best ask price |
| `book.spreadBps` | Spread in basis points |
| `book.imbalance` | Order book imbalance (-1 to +1, positive = bid heavy) |
| `trade.vwap` | Volume-weighted average price |
| `trade.buyVolume` | Total buy volume in window |
| `trade.sellVolume` | Total sell volume in window |
| `trade.tradeCount` | Number of trades in window |

### Alerts (on whale trades)

```json
{
  "type": "alert",
  "timestamp": "2025-02-18T20:38:42.188Z",
  "side": "BUY",
  "price": 67559.30,
  "quantity": 0.318,
  "sigma": 2.3
}
```

Alerts trigger when a trade size exceeds the configured sigma threshold (default: 2.0 standard deviations above mean).

## Configuration

### Command Line

```bash
./titan                          # Default: BTCUSDT
./titan -s ethusdt               # Different symbol
./titan -c config.json           # Use config file
./titan -c config.json -s solusdt # Config + override symbol
```

### Environment Variables

```bash
TITAN_SYMBOL=ethusdt ./titan
TITAN_WS_SERVER_PORT=9002 ./titan
```

| Variable | Default | Description |
|----------|---------|-------------|
| `TITAN_SYMBOL` | `btcusdt` | Trading pair |
| `TITAN_WS_SERVER_PORT` | `9001` | WebSocket server port |
| `TITAN_VWAP_WINDOW` | `100` | Trades in VWAP calculation |
| `TITAN_LARGE_TRADE_STD_DEVS` | `2.0` | Sigma threshold for alerts |
| `TITAN_CONSOLE_INTERVAL_MS` | `500` | Console output interval |

### Config File

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

## Docker Compose

Run Titan alongside your application:

```yaml
services:
  titan:
    image: ghcr.io/ianfigueroa/titan:latest
    ports:
      - "9001:9001"
    environment:
      - TITAN_SYMBOL=btcusdt

  your-app:
    build: .
    depends_on:
      - titan
    environment:
      - TITAN_URL=ws://titan:9001
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
                                            │
                                            ▼
                                     Your Application
```

## Building

### Requirements

- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.20+
- Boost 1.74+
- OpenSSL

### Linux

```bash
sudo apt install -y build-essential cmake libboost-all-dev libssl-dev
git clone https://github.com/ianfigueroa/Titan.git
cd Titan && mkdir build && cd build
cmake .. && make -j$(nproc)
```

### macOS

```bash
brew install cmake boost openssl
git clone https://github.com/ianfigueroa/Titan.git
cd Titan && mkdir build && cd build
cmake .. -DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
make -j$(sysctl -n hw.ncpu)
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-gcc
git clone https://github.com/ianfigueroa/Titan.git
cd Titan && mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j$(nproc)
```

### Running Tests

```bash
cd build
./titan_tests
```

### Benchmarks

Configure with `-DTITAN_BUILD_BENCHMARKS=ON`. Alongside the Google Benchmark
microbenchmarks (`bench_spsc_queue`, `bench_order_book`, `bench_vwap`), the SPSC
queue has a standalone cross-thread throughput bench that measures the real
producer/consumer hand-off (the Google Benchmark case only covers single-threaded
push/pop):

```bash
cmake -B build -DTITAN_BUILD_BENCHMARKS=ON
cmake --build build --target bench_spsc_throughput
./build/bench_spsc_throughput          # 500M events (default)
```

On a Ryzen 9 8945HS with MinGW g++ 15.2 `-O3 -march=native`, with nothing else running (other load on the box knocks 20-25% off):

```
SPSC hand-off: ~225 M events/s, ~4.4 ns/handoff
```

That is the queue hand-off on its own, not end-to-end throughput, and it depends on the CPU.

## When something looks off

- "Requesting snapshot" over and over means the depth stream has sequence gaps. That is normal for a few seconds during a burst; if it never stops, your latency to Binance is the problem.
- If Binance refuses the connection, it is usually a region block. The symbol has to be the Futures spelling (`btcusdt`, not `btc-usdt`).
- CPU goes up with market activity. If you do not need the full book, lower `depth_limit`. Make sure it is a release build.
- Memory grows with the number of connected clients because each one gets its own send queue. Drop idle clients.

## License

MIT

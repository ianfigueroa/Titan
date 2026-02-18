# Titan.cpp

[![Build](https://github.com/ianfigueroa/Titan/actions/workflows/build.yml/badge.svg)](https://github.com/ianfigueroa/Titan/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/ianfigueroa/Titan)](https://github.com/ianfigueroa/Titan/releases)

High-performance market data engine for cryptocurrency trading applications. Connects to Binance Futures, processes order book and trade data, and streams analytics via WebSocket.

## What It Does

Titan runs as a standalone service that your application connects to via WebSocket:

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

**Why use Titan instead of connecting directly to Binance?**
- Pre-calculated analytics (VWAP, imbalance, spread in bps)
- Whale trade alerts using sigma-based detection
- Order book with gap detection and automatic resync
- Fixed-point arithmetic for precise calculations
- One Binance connection shared by multiple clients

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

## Use Cases

- **Trading terminals**: Real-time order book display, whale alerts
- **Algorithmic trading**: VWAP signals, imbalance detection
- **Analytics dashboards**: Market microstructure visualization
- **Alert systems**: Whale trade notifications

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

## Troubleshooting

### Connection Issues

**Titan can't connect to Binance**
- Check your internet connection and firewall settings
- Binance may be blocked in your region; try using a VPN
- Verify the symbol exists on Binance Futures (e.g., `btcusdt`, not `btc-usdt`)

**WebSocket clients can't connect to Titan**
- Ensure Titan is running and listening on the expected port (default: 9001)
- Check for port conflicts: `netstat -an | grep 9001`
- When using Docker, ensure port mapping is correct: `-p 9001:9001`

### Sync Issues

**"Requesting snapshot" appears repeatedly**
- This indicates sequence gaps in the depth stream
- Normal during high volatility or network issues
- If persistent, check your network latency to Binance

**Order book shows stale data**
- Verify the depth stream is connected (check console output)
- Titan auto-recovers from disconnections; wait for resync

### Performance

**High CPU usage**
- Normal during high market activity
- Reduce `depth_limit` in config if you don't need full book depth
- Ensure you're running a release build, not debug

**High memory usage**
- Memory grows with number of connected WebSocket clients
- Each client buffers pending messages during slow sends
- Disconnect idle clients or increase queue limits

### Docker Issues

**Image won't start**
- Check logs: `docker logs <container_id>`
- Ensure port 9001 is not in use
- Verify image pulled correctly: `docker images | grep titan`

**Can't pull from ghcr.io**
- Check your Docker login: `docker login ghcr.io`
- The image is public, but some networks block ghcr.io

## License

MIT

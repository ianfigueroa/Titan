# Titan.cpp Architecture

## Overview

Titan is a real-time market data infrastructure designed for low-latency processing of cryptocurrency exchange data. It connects to Binance Futures, maintains a local order book, and provides processed market data via console and WebSocket outputs.

## Threading Model

```
┌─────────────────────────────────────────────────────────────────┐
│                        MAIN THREAD                               │
│  - Signal handling (SIGINT/SIGTERM)                              │
│  - Startup/shutdown coordination                                 │
│  - Runs network_thread_func() directly                           │
└─────────────────────────────────────────────────────────────────┘
         │
         │ Creates & joins
         ▼
┌─────────────────────┐       SPSC Queue      ┌─────────────────────┐
│   NETWORK THREAD    │ ───────────────────►  │   ENGINE THREAD     │
│   (main thread)     │      EngineMessage    │                     │
│                     │                        │ - Poll queue        │
│ - io_context.run()  │                        │ - Update order book │
│ - WebSocket client  │                        │ - Process trades    │
│ - REST client       │                        │ - Calculate metrics │
│ - Feed handler      │                        │ - Console output    │
│ - Shared SSL ctx    │                        │ - WS broadcast      │
└─────────────────────┘                        └─────────────────────┘
                                                         │
                                                         │ Thread-safe broadcast
                                                         ▼
                                               ┌─────────────────────┐
                                               │  WS SERVER THREAD   │
                                               │  (own io_context)   │
                                               │                     │
                                               │ - Accept clients    │
                                               │ - Manage sessions   │
                                               │ - Send JSON data    │
                                               └─────────────────────┘
```

### Key Design Decisions

1. **No Shared State**: Network and engine threads communicate exclusively through a lock-free SPSC queue
2. **Shared SSL Context**: Single `ssl::context` reused by REST and WebSocket clients for efficiency
3. **Independent WS Server**: Runs on separate `io_context` to prevent client issues from blocking feed
4. **Async Logging**: spdlog configured with `overrun_oldest` policy to never block

## Feed State Machine

```
                                    start()
                                       │
                                       ▼
  ┌──────────────┐  connect()   ┌────────────┐   connected   ┌─────────────────┐
  │ Disconnected │ ───────────► │ Connecting │ ────────────► │ WaitingSnapshot │
  └──────────────┘              └────────────┘               └─────────────────┘
         ▲                            │                              │
         │                       error│                     snapshot │
         │                            │                     received │
         │                            ▼                              ▼
         │                     ┌────────────┐              ┌─────────────────┐
         │                     │Reconnecting│◄─────────────│     Syncing     │
         │                     └────────────┘    gap       └─────────────────┘
         │                            │       detected            │
         │                  backoff + │                    synced │
         │                  jitter    │                           │
         │                            ▼                           ▼
         │                       ┌────────┐               ┌─────────────────┐
         └───────────────────────│ retry  │               │      Live       │
               stop()            └────────┘               └─────────────────┘
                                                                  │
                                                             error│
                                                                  ▼
                                                         ┌────────────┐
                                                         │Reconnecting│
                                                         └────────────┘
```

## Data Flow

### 1. Market Data Ingestion

```
Binance WS ──► WebSocketClient ──► FeedHandler ──► SPSC Queue ──► Engine
                    │
                    └──► REST Client (snapshots)
```

### 2. Order Book Synchronization

Following [Binance's depth stream protocol](https://binance-docs.github.io/apidocs/futures/en/#diff-book-depth-streams):

1. Connect to WebSocket, start buffering depth updates
2. Fetch REST snapshot with `lastUpdateId = U`
3. Drop buffered updates where `final_update_id <= U`
4. First processed update must satisfy: `first_update_id <= U+1 AND final_update_id >= U+1`
5. Subsequent updates: `prev_final_update_id == last_processed_id`
6. On gap: clear book, request new snapshot, repeat

### 3. Message Types

```cpp
using EngineMessage = std::variant<
    DepthUpdateMsg,      // Incremental book update
    AggTradeMsg,         // Trade execution
    SnapshotMsg,         // Full book snapshot
    ConnectionLost,      // WebSocket disconnected
    ConnectionRestored,  // WebSocket reconnected
    SequenceGap,         // Sync error detected
    Shutdown             // Graceful shutdown request
>;
```

## Component Details

### Core Types (`src/core/`)

| File | Purpose |
|------|---------|
| `types.hpp` | Common type aliases (Price, Quantity, etc.) |
| `status.hpp` | Result<T,E> monad for error handling |
| `config.hpp` | Immutable configuration |
| `messages.hpp` | Unified EngineMessage variant |

### Network Layer (`src/network/`)

| File | Purpose |
|------|---------|
| `ssl_context.hpp` | Shared SSL context factory |
| `websocket_client.hpp` | Async WebSocket client |
| `rest_client.hpp` | Async HTTPS client |
| `connection_state.hpp` | Connection state enum |

### Binance Integration (`src/binance/`)

| File | Purpose |
|------|---------|
| `types.hpp` | DepthUpdate, AggTrade, Snapshot structs |
| `feed_state.hpp` | Feed state machine enum |
| `message_parser.hpp` | JSON → typed structs |
| `feed_handler.hpp` | Connection orchestration |
| `endpoints.hpp` | API URLs |

### Order Book (`src/orderbook/`)

| File | Purpose |
|------|---------|
| `order_book.hpp` | Core engine with sync logic |
| `snapshot.hpp` | Immutable book state |
| `price_level.hpp` | Bid/ask side type definitions |
| `book_metrics.hpp` | Additional metrics calculations |

### Trade Flow (`src/trade/`)

| File | Purpose |
|------|---------|
| `vwap_calculator.hpp` | Rolling VWAP with online statistics |
| `alert_detector.hpp` | Large trade detection |
| `trade_flow.hpp` | Aggregated trade metrics |

### Output (`src/output/`)

| File | Purpose |
|------|---------|
| `console_logger.hpp` | Rate-limited console output |
| `websocket_server.hpp` | WS server for clients |
| `json_formatter.hpp` | JSON serialization |

### Engine (`src/engine/`)

| File | Purpose |
|------|---------|
| `market_data_engine.hpp` | Main coordinator |
| `reconnect_strategy.hpp` | Exponential backoff + jitter |

## Key Algorithms

### SPSC Queue

Lock-free single-producer single-consumer ring buffer using atomic sequence numbers:

```
Producer (Network Thread):
  1. Load tail position
  2. Check slot sequence == tail (slot is empty)
  3. Write data to slot
  4. Store sequence = tail + 1 (publish)
  5. Store tail = tail + 1

Consumer (Engine Thread):
  1. Load head position
  2. Check slot sequence == head + 1 (slot has data)
  3. Read data from slot
  4. Store sequence = head + capacity (mark reusable)
  5. Store head = head + 1
```

### Rolling VWAP with Online Statistics

Uses Welford's algorithm for numerically stable online variance:

```cpp
// On each trade with quantity Q:
delta = Q - mean
mean += delta / count
delta2 = Q - mean
M2 += delta * delta2
variance = M2 / count
std_dev = sqrt(variance)
```

### Reconnection with Jitter

```cpp
delay = min(current_delay, max_delay)
jitter = uniform_random(1 - factor, 1 + factor)
actual_delay = delay * jitter
current_delay *= multiplier
```

## Performance Considerations

1. **O(1) BBO Access**: Cached iterators for best bid/ask
2. **Zero-Copy Queue**: Move semantics throughout message path
3. **Cache Line Padding**: SPSC queue head/tail on separate cache lines
4. **Async Logging**: spdlog never blocks engine thread

## Future Enhancements

- [ ] Feed recording/replay (`src/recording/`)
- [ ] Multiple symbol support
- [ ] Configuration file loading
- [ ] Prometheus metrics endpoint
- [ ] Consider `boost::flat_map` for high-frequency updates

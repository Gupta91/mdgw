## mdgw — Minimal Market-Data Gateway (OKX)

### What is this?
An extensible C++20 market-data gateway with a generic base interface and an OKX implementation. It connects to OKX public WebSocket, subscribes to order books for BTC-USDT-SWAP and ETH-USDT-SWAP, maintains an in-memory two-sided order book with incremental updates, prints best bid/ask changes, and reports metrics periodically.

### Features
- MarketDataGateway base interface and OkxMarketDataGateway implementation
- OrderBook with price-level aggregation (bids/asks)
- Connects to OKX public WS and subscribes to `books5` for BTC/ETH swaps
- Applies snapshot then incremental updates
- Prints best bid/ask on change; logs updates/sec and avg tick-to-book latency
- Auto-reconnect and graceful shutdown (Ctrl-C)

### Build (macOS/Linux)
Prereqs:
- CMake >= 3.20
- A C++20 compiler (Clang 14+/GCC 11+)
- Boost (headers + system)
- OpenSSL

macOS (Homebrew):
```bash
brew install cmake boost openssl@3
```

Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-all-dev libssl-dev
```

Configure and build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run
```bash
./build/mdgw
```
The program connects to OKX public WS, subscribes to `books` (full depth) for `BTC-USDT-SWAP` and `ETH-USDT-SWAP`,
prints best bid/ask on change, logs updates/sec every 5s, and average tick-to-book latency every 60s.

#### Monitoring & Logs
The application outputs real-time market data and metrics. To monitor:

**Run in background and view logs:**
```bash
# Run in background with logs
./build/mdgw > /tmp/mdgw.log 2>&1 &

# Watch live market data updates
tail -f /tmp/mdgw.log

# Check recent activity
tail -20 /tmp/mdgw.log
```

**Log output example:**
```
[2025-08-27 11:52:25.219] [info] BTC-USDT-SWAP BB 112086.20x486.220000 | BA 112086.30x177.910000
[2025-08-27 11:52:25.316] [info] ETH-USDT-SWAP BB 4629.39x721.030000 | BA 4629.40x1064.870000
```
Format: `{INSTRUMENT} BB {bid_price}x{bid_size} | BA {ask_price}x{ask_size}`

**Check metrics and performance:**
```bash
# View update rates and latency metrics
grep "updates/sec\|latency" /tmp/mdgw.log | tail -10

# Check for connection issues
grep -i "error\|warn\|connect" /tmp/mdgw.log | tail -10

# Count total updates received
grep "BB\|BA" /tmp/mdgw.log | wc -l
```

**Stop the application:**
```bash
# Graceful shutdown
pkill -f mdgw
# or Ctrl-C if running in foreground
```

### Design Notes
- Uses OKX `books` channel (full depth orderbook) with proper snapshot + incremental update handling.
- OrderBook stores bids in descending and asks in ascending order; price and size use `double` for simplicity (would switch to fixed-point integer for production).
- Checksum: Validates OKX `cs` field using CRC32 over bid/ask levels per OKX spec. Logs warnings on mismatch; production should trigger re-subscription.
- **Lock-free SPSC ring buffer**: I/O thread pushes parsed updates to a 4KB ring buffer; dedicated book thread pops and processes them. This decouples network I/O from book updates for lower latency.
- Latency is measured from WS receipt (local clock) to orderbook state updated, and averaged over the last 60s.

### What I'd do next to hit <50µs
- Replace DOM JSON parsing with simdjson and preallocated arenas
- Switch to fixed-point ints and flat, cache-friendly containers  
- ✅ **DONE**: Lock-free SPSC between I/O and book threads
- CPU pinning/affinity, huge pages, kernel socket tuning, TCP busy-poll
- Hardware-accelerated CRC32 for checksum, zero-copy paths
- Batch multiple updates per ring buffer slot

### License
MIT



# records btcusdt depth messages for bench_ingest_replay
# usage: python benchmarks/record_feed.py [seconds] [max_messages]
import asyncio
import sys
import time
import urllib.request

import websockets

WS_URL = "wss://fstream.binance.com/public/stream?streams=btcusdt@depth@100ms"
SNAPSHOT_URL = "https://fapi.binance.com/fapi/v1/depth?symbol=BTCUSDT&limit=1000"


async def main(seconds: int, max_messages: int) -> None:
    messages = []
    async with websockets.connect(WS_URL, max_size=None) as ws:
        snapshot = urllib.request.urlopen(SNAPSHOT_URL).read().decode()
        start = time.time()
        while time.time() - start < seconds and len(messages) < max_messages:
            messages.append(await ws.recv())

    with open("benchmarks/data/btcusdt_snapshot.json", "w") as f:
        f.write(snapshot)
    with open("benchmarks/data/btcusdt_depth.jsonl", "w") as f:
        f.writelines(m + "\n" for m in messages)
    print(f"recorded {len(messages)} messages")


if __name__ == "__main__":
    secs = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    cap = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    asyncio.run(main(secs, cap))

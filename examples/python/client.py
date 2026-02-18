#!/usr/bin/env python3
"""
Titan.cpp WebSocket client example

Connects to a running Titan server and displays real-time market data.

Usage:
    pip install -r requirements.txt
    python client.py [--host HOST] [--port PORT]

Example:
    python client.py
    python client.py --host 192.168.1.100 --port 9001
"""
import argparse
import asyncio
import json
import sys

try:
    import websockets
except ImportError:
    print("Error: websockets library not installed.")
    print("Install with: pip install websockets")
    sys.exit(1)


async def main(host: str, port: int) -> None:
    uri = f"ws://{host}:{port}"
    print(f"Connecting to {uri}...")

    try:
        async with websockets.connect(uri) as ws:
            print("Connected! Receiving data...\n")
            async for message in ws:
                try:
                    data = json.loads(message)
                except json.JSONDecodeError:
                    print(f"Invalid JSON: {message}")
                    continue

                if data.get("type") == "metrics":
                    book = data.get("book", {})
                    trade = data.get("trade", {})
                    print(
                        f"BID: {book.get('bestBid', 0):.2f} | "
                        f"ASK: {book.get('bestAsk', 0):.2f} | "
                        f"SPREAD: {book.get('spreadBps', 0):.2f}bps | "
                        f"VWAP: {trade.get('vwap', 0):.2f}"
                    )
                elif data.get("type") == "alert":
                    print(
                        f"WHALE {data.get('side', '?')}: "
                        f"{data.get('quantity', 0)} @ {data.get('price', 0):.2f} "
                        f"({data.get('sigma', 0):.1f} sigma)"
                    )
    except ConnectionRefusedError:
        print(f"Error: Could not connect to {uri}")
        print("Make sure Titan is running.")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nDisconnected.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Titan.cpp WebSocket client")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=9001, help="Server port (default: 9001)")
    args = parser.parse_args()

    asyncio.run(main(args.host, args.port))

#!/usr/bin/env node
/**
 * Titan.cpp WebSocket client example
 *
 * Connects to a running Titan server and displays real-time market data.
 *
 * Usage:
 *   npm install
 *   node client.js [host] [port]
 *
 * Example:
 *   node client.js
 *   node client.js 192.168.1.100 9001
 */

const WebSocket = require('ws');

const host = process.argv[2] || 'localhost';
const portArg = process.argv[3];
let port = 9001;
if (portArg !== undefined) {
  port = parseInt(portArg, 10);
  if (isNaN(port) || port < 1 || port > 65535) {
    console.error('Error: Invalid port number. Must be 1-65535.');
    process.exit(1);
  }
}
const uri = `ws://${host}:${port}`;

console.log(`Connecting to ${uri}...`);

const ws = new WebSocket(uri);

ws.on('open', () => {
  console.log('Connected! Receiving data...\n');
});

ws.on('message', (data) => {
  try {
    const msg = JSON.parse(data);

    if (msg.type === 'metrics') {
      const { book = {}, trade = {} } = msg;
      console.log(
        `BID: ${(book.bestBid || 0).toFixed(2)} | ` +
        `ASK: ${(book.bestAsk || 0).toFixed(2)} | ` +
        `SPREAD: ${(book.spreadBps || 0).toFixed(2)}bps | ` +
        `VWAP: ${(trade.vwap || 0).toFixed(2)}`
      );
    } else if (msg.type === 'alert') {
      console.log(
        `WHALE ${msg.side || '?'}: ` +
        `${msg.quantity || 0} @ ${(msg.price || 0).toFixed(2)} ` +
        `(${(msg.sigma || 0).toFixed(1)} sigma)`
      );
    }
  } catch (e) {
    console.error('Invalid JSON:', data.toString());
  }
});

ws.on('error', (err) => {
  if (err.code === 'ECONNREFUSED') {
    console.error(`Error: Could not connect to ${uri}`);
    console.error('Make sure Titan is running.');
  } else {
    console.error('WebSocket error:', err.message);
  }
  process.exit(1);
});

ws.on('close', () => {
  console.log('\nDisconnected.');
  process.exit(0);
});

process.on('SIGINT', () => {
  ws.close();
});

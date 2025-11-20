import { WebSocketClient } from '../src/WebSocketClient.js';

/**
 * Basic WebSocket Client Usage Example
 */

console.log('=== Basic WebSocket Client Example ===');

// Create a basic WebSocket client
const client = new WebSocketClient({
  url: 'ws://localhost:8080/ws',
  debug: true,
  autoConnect: true
});

// Set up event listeners
client.on('open', () => {
  console.log('Connected to WebSocket server');
  
  // Send a message
  client.send({ type: 'greeting', message: 'Hello Server!' });
});

client.on('message', (data) => {
  console.log('Received message:', data);
});

client.on('close', (event) => {
  console.log('Connection closed:', event.code, event.reason);
});

client.on('error', (error) => {
  console.error('WebSocket error:', error);
});

// Send message after connection
setTimeout(() => {
  if (client.isClientConnected()) {
    client.send({ type: 'ping', timestamp: Date.now() });
  }
}, 2000);

// Disconnect after 10 seconds
setTimeout(() => {
  client.disconnect();
  console.log('Client disconnected');
}, 10000);

export { client };

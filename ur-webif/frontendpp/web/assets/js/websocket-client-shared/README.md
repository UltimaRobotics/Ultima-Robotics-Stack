# WebSocket Client Shared

A configurable WebSocket client implementation with shared storage support, following the same principles as the HTTP client module.

## Features

- **Shared Storage**: Multiple parts of your application can share the same WebSocket connection
- **Configurable**: Extensive configuration options for reconnection, heartbeat, authentication, and more
- **Auto-reconnection**: Automatic reconnection with exponential backoff
- **Message Queuing**: Queue messages when disconnected and send them when reconnected
- **Heartbeat**: Built-in heartbeat mechanism to keep connections alive
- **Event-driven**: Comprehensive event system for handling connection lifecycle
- **Type-safe**: Full TypeScript support (planned)
- **Browser & Node**: Works in both browser and Node.js environments

## Installation

```bash
# Import as ES module
import { WebSocketClient } from './src/WebSocketClient.js';
```

## Quick Start

### Basic Usage

```javascript
import { WebSocketClient } from './src/WebSocketClient.js';

const client = new WebSocketClient({
  url: 'ws://localhost:8080/ws',
  debug: true,
  autoConnect: true
});

client.on('open', () => {
  console.log('Connected!');
  client.send({ type: 'greeting', message: 'Hello Server!' });
});

client.on('message', (data) => {
  console.log('Received:', data);
});
```

### Shared Client

```javascript
import { WebSocketClient } from './src/WebSocketClient.js';

// Get shared client - creates if doesn't exist
const client = WebSocketClient.getSharedClient({
  url: 'ws://localhost:8080/ws',
  debug: true
});

// Same client instance can be retrieved elsewhere
const sameClient = WebSocketClient.getSharedClient({
  url: 'ws://localhost:8080/ws',
  debug: true
});

console.log(client === sameClient); // true
```

## Configuration

### Using WebSocketConfig

```javascript
import { WebSocketConfig } from './src/WebSocketClient.js';

const config = WebSocketConfig.forServer('ws://localhost:8080/ws')
  .setAuth('bearer', 'your-token')
  .setProtocols(['chat-v1'])
  .setHeartbeatConfig({
    enabled: true,
    interval: 30000
  })
  .setReconnectConfig({
    enabled: true,
    maxAttempts: 5,
    interval: 2000
  });

const client = new WebSocketClient(config);
```

### Configuration Options

```javascript
const config = {
  // Connection
  url: 'ws://localhost:8080/ws',
  protocols: [],
  timeout: 10000,
  binaryType: 'blob', // 'blob' or 'arraybuffer'
  
  // Authentication
  auth: {
    type: 'bearer', // 'bearer', 'basic', 'apikey', 'token'
    token: 'your-token'
  },
  
  // Reconnection
  reconnect: true,
  maxReconnectAttempts: 10,
  reconnectInterval: 5000,
  reconnectBackoffMultiplier: 1.5,
  maxReconnectInterval: 30000,
  
  // Heartbeat
  heartbeat: true,
  heartbeatInterval: 30000,
  heartbeatTimeout: 5000,
  
  // Message queuing
  queueMessages: true,
  maxQueueSize: 100,
  
  // Other
  debug: false,
  autoConnect: true
};
```

## API Reference

### WebSocketClient

#### Constructor

```javascript
new WebSocketClient(config)
```

#### Static Methods

- `WebSocketClient.getSharedClient(config)` - Get or create shared client
- `WebSocketClient.getStorageStats()` - Get storage statistics
- `WebSocketClient.cleanupInactiveClients(timeout)` - Clean up inactive clients

#### Methods

- `connect()` - Connect to WebSocket server
- `disconnect()` - Disconnect from server
- `send(data, options)` - Send a message
- `sendAndWait(data, timeout)` - Send message and wait for response
- `queueMessage(data)` - Queue a message for later sending
- `on(event, callback)` - Add event listener
- `off(event, callback)` - Remove event listener
- `once(event, callback)` - Add one-time event listener
- `getConnectionStatus()` - Get connection status
- `getStats()` - Get client statistics
- `updateConfig(newConfig)` - Update configuration
- `close()` - Close client and remove from storage

#### Events

- `open` - Connection established
- `message` - Message received
- `close` - Connection closed
- `error` - Error occurred
- `reconnecting` - Reconnection attempt started
- `maxReconnectAttemptsReached` - Max reconnection attempts reached
- `heartbeat` - Heartbeat response received
- `sent` - Message sent
- `queued` - Message queued

### WebSocketConfig

#### Static Methods

- `WebSocketConfig.forServer(url, options)` - Create config for server
- `WebSocketConfig.forLocalhost(port, path, options)` - Create config for localhost
- `WebSocketConfig.forSecureServer(url, options)` - Create config for secure WebSocket

#### Methods

- `setAuth(type, token, options)` - Set authentication
- `setProtocols(protocols)` - Set WebSocket protocols
- `setTimeout(timeout)` - Set connection timeout
- `setReconnectConfig(options)` - Set reconnection configuration
- `setHeartbeatConfig(options)` - Set heartbeat configuration
- `setBinaryType(type)` - Set binary message type
- `setDebug(enabled)` - Enable/disable debug mode
- `setAutoConnect(enabled)` - Enable/disable auto-connect
- `setQueueMessages(enabled, maxSize)` - Configure message queuing
- `clone()` - Clone configuration
- `validate()` - Validate configuration
- `toObject()` - Convert to plain object

## Examples

### Message with Response

```javascript
// Send message and wait for response
try {
  const response = await client.sendAndWait(
    { type: 'request', data: 'user_info' },
    5000 // 5 second timeout
  );
  console.log('Response:', response);
} catch (error) {
  console.error('Request failed:', error);
}
```

### Message Queuing

```javascript
// Messages sent while disconnected will be queued
client.send({ type: 'queued', message: 'This will be queued' });

// When reconnected, queued messages are automatically sent
client.on('open', () => {
  console.log('Connected, queued messages will be sent');
});
```

### Custom Authentication

```javascript
const config = new WebSocketConfig({
  url: 'wss://api.example.com/ws'
})
.setAuth('apikey', 'your-api-key', { key: 'X-Custom-API-Key' })
.setHeaders({
  'User-Agent': 'MyApp/1.0'
});
```

### Error Handling

```javascript
client
  .on('error', (error) => {
    console.error('WebSocket error:', error);
  })
  .on('close', (event) => {
    if (event.code !== 1000) {
      console.warn('Unexpected closure:', event.code, event.reason);
    }
  })
  .on('maxReconnectAttemptsReached', () => {
    console.error('Could not reconnect after multiple attempts');
  });
```

## Storage Management

The shared storage system manages WebSocket client instances across your application:

```javascript
// Get storage statistics
const stats = WebSocketClient.getStorageStats();
console.log('Active clients:', stats.activeClients.length);
console.log('Total clients:', stats.totalClients);

// Clean up inactive clients
const cleaned = WebSocketClient.cleanupInactiveClients(30 * 60 * 1000); // 30 minutes
console.log('Cleaned up:', cleaned);
```

## Browser Compatibility

- Modern browsers with WebSocket support
- ES6 modules support required
- No external dependencies

## Node.js Usage

For Node.js environments, you'll need a WebSocket implementation:

```javascript
// Node.js example (requires ws or similar WebSocket polyfill)
global.WebSocket = require('ws');
import { WebSocketClient } from './src/WebSocketClient.js';
```

## License

MIT License - see LICENSE file for details.

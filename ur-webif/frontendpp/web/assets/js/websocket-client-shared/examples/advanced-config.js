import { WebSocketClient, WebSocketConfig } from '../src/WebSocketClient.js';

/**
 * Advanced Configuration Example
 * Demonstrates various configuration options and features
 */


// Example 1: Secure WebSocket with authentication
const secureConfig = WebSocketConfig.forSecureServer('wss://api.example.com/ws')
  .setAuth('bearer', 'your-jwt-token')
  .setProtocols(['chat-v1', 'notifications'])
  .setHeartbeatConfig({
    enabled: true,
    interval: 25000,
    message: { type: 'ping', client: 'advanced-example' }
  })
  .setReconnectConfig({
    enabled: true,
    maxAttempts: 10,
    interval: 2000,
    backoffMultiplier: 2,
    maxInterval: 30000
  })
  .setDebug(true)
  .setQueueMessages(true, 50);

const secureClient = new WebSocketClient(secureConfig);

// Example 2: Local development configuration
const localConfig = WebSocketConfig.forLocalhost(8080, '/ws')
  .setTimeout(5000)
  .setBinaryType('arraybuffer')
  .setAutoConnect(false);

const localClient = new WebSocketClient(localConfig);

// Example 3: Custom configuration with message queuing
const customConfig = new WebSocketConfig({
  url: 'ws://localhost:9000/custom',
  debug: true,
  queueMessages: true,
  maxQueueSize: 100,
  autoConnect: true,
  reconnectConfig: {
    enabled: true,
    maxAttempts: 3,
    interval: 1000,
    retryCondition: (event) => {
      // Custom retry condition - don't retry on authentication errors
      return event.code !== 1003 && event.code !== 1008;
    }
  },
  heartbeatConfig: {
    enabled: true,
    interval: 15000,
    timeout: 3000
  }
});

const customClient = new WebSocketClient(customConfig);

// Set up event handlers for secure client
secureClient.on('open', () => {
  
  // Send message with ID and expect response
  secureClient.sendAndWait(
    { type: 'authenticate', token: 'jwt-token' },
    5000
  ).then(response => {
  }).catch(error => {
    console.error('Authentication failed:', error);
  });
});

secureClient.on('message', (data) => {
});

// Demonstrate message queuing with custom client
customClient.on('open', () => {
});

// Queue messages before connection (they'll be sent when connected)
customClient.queueMessage({ type: 'queued', id: 1, message: 'First queued message' });
customClient.queueMessage({ type: 'queued', id: 2, message: 'Second queued message' });

// Demonstrate different send methods
async function demonstrateSending() {
  try {
    // Regular send
    await customClient.send({ type: 'regular', message: 'Regular message' });
    
    // Send with ID
    await customClient.send(
      { type: 'with_id', message: 'Message with ID' },
      { includeId: true }
    );
    
    // Send and wait for response
    const response = await customClient.sendAndWait(
      { type: 'request', data: 'some_data' },
      3000
    );
    
  } catch (error) {
    console.error('Send error:', error);
  }
}

// Connection management demonstration
function demonstrateConnectionManagement() {
  
  // Update configuration dynamically
  secureClient.updateConfig({
    debug: false,
    heartbeatConfig: {
      ...secureClient.config.heartbeatConfig,
      interval: 45000
    }
  });
  
}

// Event handling demonstration
customClient
  .on('queued', (message) => {
  })
  .on('sent', (message) => {
  })
  .on('heartbeat', (data) => {
  })
  .on('reconnecting', (info) => {
  });

// Error handling
customClient.on('error', (error) => {
  console.error('Custom client error:', error);
});

// Simulate connection and usage
setTimeout(() => {
  localClient.connect().then(() => {
    demonstrateSending();
  }).catch(error => {
    console.error('Local client connection failed:', error);
  });
}, 1000);

setTimeout(() => {
  demonstrateConnectionManagement();
}, 2000);

setTimeout(() => {
}, 3000);

// Cleanup demonstration
setTimeout(() => {
  const cleaned = WebSocketClient.cleanupInactiveClients(0); // Clean all inactive
}, 5000);

setTimeout(() => {
  secureClient.close();
  localClient.close();
  customClient.close();
}, 8000);

export { secureClient, localClient, customClient };

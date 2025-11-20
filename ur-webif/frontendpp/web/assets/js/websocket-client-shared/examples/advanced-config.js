import { WebSocketClient, WebSocketConfig } from '../src/WebSocketClient.js';

/**
 * Advanced Configuration Example
 * Demonstrates various configuration options and features
 */

console.log('=== Advanced Configuration Example ===');

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
  console.log('Secure client connected');
  
  // Send message with ID and expect response
  secureClient.sendAndWait(
    { type: 'authenticate', token: 'jwt-token' },
    5000
  ).then(response => {
    console.log('Authentication response:', response);
  }).catch(error => {
    console.error('Authentication failed:', error);
  });
});

secureClient.on('message', (data) => {
  console.log('Secure client received:', data);
});

// Demonstrate message queuing with custom client
customClient.on('open', () => {
  console.log('Custom client connected - queued messages will be sent');
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
    console.log('Response received:', response);
    
  } catch (error) {
    console.error('Send error:', error);
  }
}

// Connection management demonstration
function demonstrateConnectionManagement() {
  console.log('Connection status:', secureClient.getStats());
  
  // Update configuration dynamically
  secureClient.updateConfig({
    debug: false,
    heartbeatConfig: {
      ...secureClient.config.heartbeatConfig,
      interval: 45000
    }
  });
  
  console.log('Updated configuration:', secureClient.config.toObject());
}

// Event handling demonstration
customClient
  .on('queued', (message) => {
    console.log('Message queued:', message);
  })
  .on('sent', (message) => {
    console.log('Message sent:', message);
  })
  .on('heartbeat', (data) => {
    console.log('Heartbeat response:', data);
  })
  .on('reconnecting', (info) => {
    console.log(`Reconnecting: attempt ${info.attempt}, delay ${info.delay}ms`);
  });

// Error handling
customClient.on('error', (error) => {
  console.error('Custom client error:', error);
});

// Simulate connection and usage
setTimeout(() => {
  console.log('Connecting local client manually...');
  localClient.connect().then(() => {
    console.log('Local client connected');
    demonstrateSending();
  }).catch(error => {
    console.error('Local client connection failed:', error);
  });
}, 1000);

setTimeout(() => {
  demonstrateConnectionManagement();
}, 2000);

setTimeout(() => {
  console.log('Storage statistics:', WebSocketClient.getStorageStats());
}, 3000);

// Cleanup demonstration
setTimeout(() => {
  console.log('Cleaning up inactive clients...');
  const cleaned = WebSocketClient.cleanupInactiveClients(0); // Clean all inactive
  console.log('Cleaned up clients:', cleaned);
}, 5000);

setTimeout(() => {
  console.log('Closing all clients...');
  secureClient.close();
  localClient.close();
  customClient.close();
}, 8000);

export { secureClient, localClient, customClient };

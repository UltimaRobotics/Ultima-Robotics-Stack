import { WebSocketClient } from '../src/WebSocketClient.js';

/**
 * Shared WebSocket Client Example
 * Demonstrates how multiple parts of an application can share the same WebSocket connection
 */

console.log('=== Shared WebSocket Client Example ===');

// Configuration for the shared client
const config = {
  url: 'ws://localhost:8080/ws',
  debug: true,
  autoConnect: true,
  reconnectConfig: {
    enabled: true,
    maxAttempts: 5,
    interval: 3000
  },
  heartbeatConfig: {
    enabled: true,
    interval: 30000
  }
};

// Get shared client - this will create the client if it doesn't exist
const sharedClient = WebSocketClient.getSharedClient(config);

// Module 1: Chat functionality
function setupChatModule(client) {
  console.log('Setting up chat module...');
  
  client.on('message', (data) => {
    if (data.type === 'chat') {
      console.log(`[CHAT] ${data.user}: ${data.message}`);
    }
  });
  
  // Expose chat send function
  return {
    sendMessage: (user, message) => {
      client.send({ type: 'chat', user, message, timestamp: Date.now() });
    }
  };
}

// Module 2: Notifications
function setupNotificationModule(client) {
  console.log('Setting up notification module...');
  
  client.on('message', (data) => {
    if (data.type === 'notification') {
      console.log(`[NOTIFICATION] ${data.title}: ${data.body}`);
    }
  });
  
  return {
    subscribeToNotifications: () => {
      client.send({ type: 'subscribe', channel: 'notifications' });
    }
  };
}

// Module 3: Real-time data
function setupDataModule(client) {
  console.log('Setting up data module...');
  
  client.on('message', (data) => {
    if (data.type === 'data_update') {
      console.log(`[DATA] Update received:`, data.payload);
    }
  });
  
  return {
    requestData: (dataType) => {
      client.send({ type: 'request_data', data_type: dataType });
    }
  };
}

// Set up all modules
const chat = setupChatModule(sharedClient);
const notifications = setupNotificationModule(sharedClient);
const data = setupDataModule(sharedClient);

// Shared client event listeners
sharedClient.on('open', () => {
  console.log('Shared client connected');
  
  // Initialize modules
  notifications.subscribeToNotifications();
  data.requestData('initial');
});

sharedClient.on('reconnecting', (info) => {
  console.log(`Reconnecting... Attempt ${info.attempt}/${sharedClient.config.reconnectConfig.maxAttempts}`);
});

sharedClient.on('maxReconnectAttemptsReached', () => {
  console.error('Max reconnection attempts reached. Giving up.');
});

// Simulate usage from different parts of the application
setTimeout(() => {
  chat.sendMessage('Alice', 'Hello from chat module!');
}, 2000);

setTimeout(() => {
  data.requestData('user_stats');
}, 3000);

setTimeout(() => {
  chat.sendMessage('Bob', 'Another message from chat');
}, 4000);

// Get the same shared client elsewhere in the app
function anotherPartOfApp() {
  // This will return the existing client instance
  const sameClient = WebSocketClient.getSharedClient(config);
  
  console.log('Same client instance?', sameClient === sharedClient); // true
  console.log('Client stats:', sameClient.getStats());
  
  // Send a message from this part of the app
  setTimeout(() => {
    sameClient.send({ type: 'system', message: 'Message from another part of app' });
  }, 5000);
}

anotherPartOfApp();

// Show storage statistics
setTimeout(() => {
  console.log('Storage stats:', WebSocketClient.getStorageStats());
}, 6000);

// Clean up after 15 seconds
setTimeout(() => {
  sharedClient.close();
  console.log('Shared client closed and removed from storage');
}, 15000);

export { sharedClient, chat, notifications, data };

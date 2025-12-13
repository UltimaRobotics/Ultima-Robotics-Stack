import { WebSocketClient } from '../src/WebSocketClient.js';

/**
 * Shared WebSocket Client Example
 * Demonstrates how multiple parts of an application can share the same WebSocket connection
 */


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
  
  client.on('message', (data) => {
    if (data.type === 'chat') {
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
  
  client.on('message', (data) => {
    if (data.type === 'notification') {
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
  
  client.on('message', (data) => {
    if (data.type === 'data_update') {
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
  
  // Initialize modules
  notifications.subscribeToNotifications();
  data.requestData('initial');
});

sharedClient.on('reconnecting', (info) => {
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
  
  
  // Send a message from this part of the app
  setTimeout(() => {
    sameClient.send({ type: 'system', message: 'Message from another part of app' });
  }, 5000);
}

anotherPartOfApp();

// Show storage statistics
setTimeout(() => {
}, 6000);

// Clean up after 15 seconds
setTimeout(() => {
  sharedClient.close();
}, 15000);

export { sharedClient, chat, notifications, data };

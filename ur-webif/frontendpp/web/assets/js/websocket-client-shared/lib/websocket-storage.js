/**
 * WebSocket Client Storage - Manages shared WebSocket client instances
 * Allows multiple scripts to use the same connected client
 */

class WebSocketStorage {
  constructor() {
    this.clients = new Map();
    this.connections = new Map();
    this.messageQueues = new Map();
  }

  /**
   * Store a WebSocket client instance
   * @param {string} key - Unique identifier for the client
   * @param {Object} client - WebSocket client instance
   * @param {Object} config - Connection configuration
   */
  setClient(key, client, config) {
    this.clients.set(key, {
      instance: client,
      config: config,
      createdAt: new Date(),
      lastUsed: new Date()
    });
    
    this.connections.set(key, {
      status: 'disconnected',
      config: config,
      connectedAt: null,
      lastActivity: new Date(),
      reconnectAttempts: 0,
      messageCount: 0,
      errorCount: 0
    });

    // Initialize message queue if queuing is enabled
    if (config.queueMessages) {
      this.messageQueues.set(key, []);
    }
  }

  /**
   * Get a WebSocket client instance by key
   * @param {string} key - Unique identifier for the client
   * @returns {Object|null} Client instance or null if not found
   */
  getClient(key) {
    const clientData = this.clients.get(key);
    if (clientData) {
      clientData.lastUsed = new Date();
      return clientData.instance;
    }
    return null;
  }

  /**
   * Check if a client exists for the given key
   * @param {string} key - Unique identifier for the client
   * @returns {boolean} True if client exists
   */
  hasClient(key) {
    return this.clients.has(key);
  }

  /**
   * Remove a client instance
   * @param {string} key - Unique identifier for the client
   * @returns {boolean} True if client was removed
   */
  removeClient(key) {
    const clientRemoved = this.clients.delete(key);
    const connectionRemoved = this.connections.delete(key);
    const queueRemoved = this.messageQueues.delete(key);
    return clientRemoved || connectionRemoved || queueRemoved;
  }

  /**
   * Get all stored client keys
   * @returns {Array<string>} Array of client keys
   */
  getAllClientKeys() {
    return Array.from(this.clients.keys());
  }

  /**
   * Get connection status for a client
   * @param {string} key - Unique identifier for the client
   * @returns {Object|null} Connection status or null if not found
   */
  getConnectionStatus(key) {
    return this.connections.get(key) || null;
  }

  /**
   * Update connection status
   * @param {string} key - Unique identifier for the client
   * @param {string} status - New status ('connecting', 'connected', 'disconnected', 'error')
   * @param {Object} metadata - Additional metadata to update
   */
  updateConnectionStatus(key, status, metadata = {}) {
    const connection = this.connections.get(key);
    if (connection) {
      connection.status = status;
      connection.lastActivity = new Date();
      
      if (status === 'connected') {
        connection.connectedAt = new Date();
        connection.reconnectAttempts = 0;
      }
      
      // Update additional metadata
      Object.keys(metadata).forEach(metaKey => {
        if (metaKey === 'incrementReconnectAttempts') {
          connection.reconnectAttempts++;
        } else if (metaKey === 'incrementMessageCount') {
          connection.messageCount++;
        } else if (metaKey === 'incrementErrorCount') {
          connection.errorCount++;
        } else {
          connection[metaKey] = metadata[metaKey];
        }
      });
    }
  }

  /**
   * Get message queue for a client
   * @param {string} key - Unique identifier for the client
   * @returns {Array|null} Message queue or null if not found
   */
  getMessageQueue(key) {
    return this.messageQueues.get(key) || null;
  }

  /**
   * Add message to queue
   * @param {string} key - Unique identifier for the client
   * @param {Object} message - Message to queue
   * @returns {boolean} True if message was queued
   */
  queueMessage(key, message) {
    const queue = this.messageQueues.get(key);
    const clientData = this.clients.get(key);
    
    if (!queue || !clientData) {
      return false;
    }

    // Check queue size limit
    if (queue.length >= clientData.config.maxQueueSize) {
      // Remove oldest message if queue is full
      queue.shift();
    }

    queue.push({
      message,
      timestamp: new Date(),
      id: this.generateMessageId()
    });

    return true;
  }

  /**
   * Get and clear message queue
   * @param {string} key - Unique identifier for the client
   * @returns {Array} Array of queued messages
   */
  flushMessageQueue(key) {
    const queue = this.messageQueues.get(key);
    if (queue) {
      const messages = [...queue];
      queue.length = 0; // Clear the queue
      return messages;
    }
    return [];
  }

  /**
   * Clear message queue for a client
   * @param {string} key - Unique identifier for the client
   * @returns {boolean} True if queue was cleared
   */
  clearMessageQueue(key) {
    const queue = this.messageQueues.get(key);
    if (queue) {
      queue.length = 0;
      return true;
    }
    return false;
  }

  /**
   * Generate unique message ID
   * @returns {string} Unique message ID
   */
  generateMessageId() {
    return `msg_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Get clients by status
   * @param {string} status - Status to filter by
   * @returns {Array<string>} Array of client keys with specified status
   */
  getClientsByStatus(status) {
    const result = [];
    for (const [key, connection] of this.connections.entries()) {
      if (connection.status === status) {
        result.push(key);
      }
    }
    return result;
  }

  /**
   * Get active (connected) clients
   * @returns {Array<string>} Array of connected client keys
   */
  getActiveClients() {
    return this.getClientsByStatus('connected');
  }

  /**
   * Get inactive (disconnected) clients
   * @returns {Array<string>} Array of disconnected client keys
   */
  getInactiveClients() {
    return this.getClientsByStatus('disconnected');
  }

  /**
   * Get clients with errors
   * @returns {Array<string>} Array of client keys with error status
   */
  getErrorClients() {
    return this.getClientsByStatus('error');
  }

  /**
   * Clean up inactive clients (older than specified timeout)
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {Array<string>} Array of cleaned up client keys
   */
  cleanupInactiveClients(timeoutMs = 30 * 60 * 1000) { // 30 minutes default
    const now = new Date();
    const cleanedUp = [];

    for (const [key, clientData] of this.clients.entries()) {
      const connection = this.connections.get(key);
      const isInactive = (now - clientData.lastUsed > timeoutMs) && 
                         (connection && connection.status !== 'connected');
      
      if (isInactive) {
        // Disconnect client before cleanup
        const client = clientData.instance;
        if (client && typeof client.disconnect === 'function') {
          try {
            client.disconnect();
          } catch (error) {
            console.warn(`Error disconnecting client ${key} during cleanup:`, error);
          }
        }
        
        this.clients.delete(key);
        this.connections.delete(key);
        this.messageQueues.delete(key);
        cleanedUp.push(key);
      }
    }

    return cleanedUp;
  }

  /**
   * Get storage statistics
   * @returns {Object} Statistics about stored clients
   */
  getStats() {
    const connections = Array.from(this.connections.entries()).map(([key, conn]) => ({
      key,
      status: conn.status,
      connectedAt: conn.connectedAt,
      lastActivity: conn.lastActivity,
      reconnectAttempts: conn.reconnectAttempts,
      messageCount: conn.messageCount,
      errorCount: conn.errorCount,
      config: conn.config
    }));

    const queues = Array.from(this.messageQueues.entries()).map(([key, queue]) => ({
      key,
      size: queue.length,
      maxSize: this.clients.get(key)?.config?.maxQueueSize || 0
    }));

    return {
      totalClients: this.clients.size,
      totalConnections: this.connections.size,
      totalQueues: this.messageQueues.size,
      clientKeys: this.getAllClientKeys(),
      activeClients: this.getActiveClients(),
      inactiveClients: this.getInactiveClients(),
      errorClients: this.getErrorClients(),
      connections,
      messageQueues: queues
    };
  }

  /**
   * Export storage state (for persistence)
   * @returns {Object} Serializable storage state
   */
  exportState() {
    return {
      connections: Object.fromEntries(this.connections),
      messageQueues: Object.fromEntries(this.messageQueues),
      metadata: {
        totalClients: this.clients.size,
        exportedAt: new Date().toISOString()
      }
    };
  }

  /**
   * Clear all stored clients
   * @returns {number} Number of clients cleared
   */
  clearAll() {
    const count = this.clients.size;
    
    // Disconnect all clients before clearing
    for (const [key, clientData] of this.clients.entries()) {
      const client = clientData.instance;
      if (client && typeof client.disconnect === 'function') {
        try {
          client.disconnect();
        } catch (error) {
          console.warn(`Error disconnecting client ${key} during clear:`, error);
        }
      }
    }
    
    this.clients.clear();
    this.connections.clear();
    this.messageQueues.clear();
    
    return count;
  }
}

// Create a singleton instance for shared access across modules
const websocketStorage = new WebSocketStorage();

export { WebSocketStorage, websocketStorage };
export default websocketStorage;

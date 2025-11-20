/**
 * HTTP Client Storage - Manages shared HTTP client instances
 * Allows multiple scripts to use the same connected client
 */

class HttpStorage {
  constructor() {
    this.clients = new Map();
    this.connections = new Map();
  }

  /**
   * Store an HTTP client instance
   * @param {string} key - Unique identifier for the client
   * @param {Object} client - HTTP client instance
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
      status: 'connected',
      config: config,
      connectedAt: new Date()
    });
  }

  /**
   * Get an HTTP client instance by key
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
    return clientRemoved || connectionRemoved;
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
   * @param {string} status - New status ('connected', 'disconnected', 'error')
   */
  updateConnectionStatus(key, status) {
    const connection = this.connections.get(key);
    if (connection) {
      connection.status = status;
      connection.updatedAt = new Date();
    }
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
      if (now - clientData.lastUsed > timeoutMs) {
        this.clients.delete(key);
        this.connections.delete(key);
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
    return {
      totalClients: this.clients.size,
      totalConnections: this.connections.size,
      clientKeys: this.getAllClientKeys(),
      connections: Array.from(this.connections.entries()).map(([key, conn]) => ({
        key,
        status: conn.status,
        connectedAt: conn.connectedAt,
        config: conn.config
      }))
    };
  }
}

// Create a singleton instance for shared access across modules
const httpStorage = new HttpStorage();

export { HttpStorage, httpStorage };
export default httpStorage;

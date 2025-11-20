import { websocketStorage } from '../lib/websocket-storage.js';
import { WebSocketConfig } from '../lib/config.js';

/**
 * Enhanced WebSocket Client Implementation
 * Provides a unified interface for WebSocket connections with shared storage support
 */

class WebSocketClient {
  constructor(config = {}) {
    // Handle different config types
    if (typeof config === 'string') {
      // If config is a string, treat it as URL
      this.config = new WebSocketConfig({ url: config });
    } else if (config instanceof WebSocketConfig) {
      this.config = config;
    } else {
      this.config = new WebSocketConfig(config);
    }
    
    this.config.validate();
    this.storageKey = this.generateStorageKey();
    
    // WebSocket instance and state
    this.ws = null;
    this.reconnectTimer = null;
    this.heartbeatTimer = null;
    this.heartbeatTimeoutTimer = null;
    this.connectionTimeoutTimer = null;
    
    // State tracking
    this.isConnecting = false;
    this.isConnected = false;
    this.reconnectAttempts = 0;
    this.lastError = null;
    this.lastMessage = null;
    
    // Event management
    this.eventListeners = new Map();
    this.onceListeners = new Map();
    
    // Message handling
    this.messageId = 0;
    this.pendingMessages = new Map();
    this.messageCallbacks = new Map();
    
    // Initialize if auto-connect is enabled
    if (this.config.autoConnect) {
      this.init();
    }
  }

  /**
   * Generate a unique storage key based on configuration
   * @returns {string} Unique storage key
   */
  generateStorageKey() {
    const configString = JSON.stringify(this.config.toObject());
    const configHash = btoa(configString).slice(0, 16);
    return `websocket-client-${configHash}`;
  }

  /**
   * Get or create a shared client instance
   * @param {Object} config - Configuration for the client
   * @returns {WebSocketClient} Shared client instance
   */
  static getSharedClient(config = {}) {
    const client = new WebSocketClient(config);
    const storageKey = client.storageKey;

    // Check if client already exists in storage
    if (websocketStorage.hasClient(storageKey)) {
      return websocketStorage.getClient(storageKey);
    }

    // Store new client in storage
    websocketStorage.setClient(storageKey, client, client.config.toObject());
    return client;
  }

  /**
   * Initialize the client
   */
  init() {
    this.debugLog('Initializing WebSocket client...');
    this.connect();
  }

  /**
   * Connect to WebSocket server
   * @returns {Promise<void>} Promise that resolves when connected
   */
  async connect() {
    if (this.isConnecting || this.isConnected) {
      return;
    }

    return new Promise((resolve, reject) => {
      this.isConnecting = true;
      this.updateStorageStatus('connecting');
      
      this.debugLog(`Connecting to ${this.config.url}...`);

      try {
        // Create WebSocket with protocols
        this.ws = new WebSocket(this.config.url, this.config.protocols);
        this.ws.binaryType = this.config.binaryType;

        // Set up event handlers
        this.ws.onopen = (event) => this.handleOpen(event, resolve, reject);
        this.ws.onmessage = (event) => this.handleMessage(event);
        this.ws.onclose = (event) => this.handleClose(event);
        this.ws.onerror = (error) => this.handleError(error);

        // Set connection timeout
        this.setConnectionTimeout(resolve, reject);

      } catch (error) {
        this.isConnecting = false;
        this.debugLog('Connection error:', error);
        this.updateStorageStatus('error', { incrementErrorCount: true });
        this.lastError = error;
        this.emit('error', error);
        reject(error);
      }
    });
  }

  /**
   * Handle WebSocket open event
   */
  handleOpen(event, resolve, reject) {
    this.clearConnectionTimeout();
    
    this.debugLog('WebSocket connected');
    this.isConnected = true;
    this.isConnecting = false;
    this.reconnectAttempts = 0;
    this.lastError = null;
    
    this.updateStorageStatus('connected');
    this.emit('open', event);
    
    // Start heartbeat if enabled
    if (this.config.heartbeatConfig.enabled) {
      this.startHeartbeat();
    }
    
    // Send queued messages
    this.flushQueuedMessages();
    
    resolve();
  }

  /**
   * Handle WebSocket message event
   */
  handleMessage(event) {
    this.updateStorageStatus('connected', { incrementMessageCount: true });
    
    let data;
    try {
      // Try to parse as JSON first
      data = JSON.parse(event.data);
      this.lastMessage = data;
    } catch (error) {
      // If not JSON, use raw data
      data = event.data;
      this.lastMessage = data;
    }

    this.emit('message', data);

    // Handle message callbacks
    if (data && data.id && this.messageCallbacks.has(data.id)) {
      const callback = this.messageCallbacks.get(data.id);
      callback(data);
      this.messageCallbacks.delete(data.id);
    }

    // Handle heartbeat response
    if (data && data.type === 'heartbeat_response') {
      this.handleHeartbeatResponse(data);
    }
  }

  /**
   * Handle WebSocket close event
   */
  handleClose(event) {
    this.debugLog(`WebSocket closed: ${event.code} - ${event.reason}`);
    
    this.isConnected = false;
    this.isConnecting = false;
    this.ws = null;
    
    this.stopHeartbeat();
    this.clearConnectionTimeout();
    this.clearReconnectTimer();
    
    this.updateStorageStatus('disconnected');
    this.emit('close', event);

    // Schedule reconnection if enabled and appropriate
    if (this.config.reconnectConfig.enabled && 
        this.config.reconnectConfig.retryCondition(event)) {
      this.scheduleReconnect();
    }
  }

  /**
   * Handle WebSocket error event
   */
  handleError(error) {
    this.debugLog('WebSocket error:', error);
    this.lastError = error;
    this.updateStorageStatus('error', { incrementErrorCount: true });
    this.emit('error', error);
  }

  /**
   * Send a message
   * @param {*} data - Data to send
   * @param {Object} options - Send options
   * @returns {Promise<void>} Promise that resolves when message is sent
   */
  async send(data, options = {}) {
    if (!this.isConnected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
      if (this.config.queueMessages && !options.skipQueue) {
        return this.queueMessage(data);
      }
      throw new Error('WebSocket is not connected');
    }

    return new Promise((resolve, reject) => {
      try {
        let message = data;
        
        // Add message ID if requested
        if (options.includeId) {
          const id = this.generateMessageId();
          message = typeof data === 'object' ? { ...data, id } : { data, id };
          
          // Set up callback for response
          if (options.expectResponse) {
            const timeout = setTimeout(() => {
              this.messageCallbacks.delete(id);
              reject(new Error('Response timeout'));
            }, options.responseTimeout || 30000);
            
            this.messageCallbacks.set(id, (response) => {
              clearTimeout(timeout);
              resolve(response);
            });
            return;
          }
        }

        // Convert to string if needed
        const messageString = typeof message === 'string' ? message : JSON.stringify(message);
        
        this.ws.send(messageString);
        this.debugLog('Message sent:', message);
        this.emit('sent', message);
        resolve();
        
      } catch (error) {
        this.debugLog('Failed to send message:', error);
        this.updateStorageStatus('error', { incrementErrorCount: true });
        reject(error);
      }
    });
  }

  /**
   * Send a message and expect a response
   * @param {*} data - Data to send
   * @param {number} timeout - Response timeout in milliseconds
   * @returns {Promise<*>} Promise that resolves with response
   */
  async sendAndWait(data, timeout = 30000) {
    return this.send(data, { includeId: true, expectResponse: true, responseTimeout: timeout });
  }

  /**
   * Queue a message for later sending
   * @param {*} data - Data to queue
   * @returns {Promise<void>} Promise that resolves when message is queued
   */
  async queueMessage(data) {
    return new Promise((resolve, reject) => {
      if (websocketStorage.queueMessage(this.storageKey, data)) {
        this.debugLog('Message queued:', data);
        this.emit('queued', data);
        resolve();
      } else {
        reject(new Error('Failed to queue message'));
      }
    });
  }

  /**
   * Flush queued messages
   */
  async flushQueuedMessages() {
    const queuedMessages = websocketStorage.flushMessageQueue(this.storageKey);
    
    for (const { message } of queuedMessages) {
      try {
        await this.send(message, { skipQueue: true });
      } catch (error) {
        this.debugLog('Failed to send queued message:', error);
      }
    }
  }

  /**
   * Disconnect the WebSocket
   */
  disconnect() {
    this.debugLog('Disconnecting WebSocket...');
    
    this.clearReconnectTimer();
    this.stopHeartbeat();
    this.clearConnectionTimeout();
    
    if (this.ws) {
      this.ws.close(1000, 'Client disconnect');
      this.ws = null;
    }
    
    this.isConnected = false;
    this.isConnecting = false;
    this.updateStorageStatus('disconnected');
  }

  /**
   * Schedule reconnection
   */
  scheduleReconnect() {
    if (this.reconnectAttempts >= this.config.reconnectConfig.maxAttempts) {
      this.debugLog('Max reconnection attempts reached');
      this.emit('maxReconnectAttemptsReached');
      return;
    }

    this.reconnectAttempts++;
    const delay = this.calculateReconnectDelay();
    
    this.debugLog(`Scheduling reconnection attempt ${this.reconnectAttempts}/${this.config.reconnectConfig.maxAttempts} in ${delay}ms`);
    this.updateStorageStatus('connecting', { incrementReconnectAttempts: true });
    this.emit('reconnecting', { attempt: this.reconnectAttempts, delay });

    this.reconnectTimer = setTimeout(() => {
      this.connect().catch(error => {
        this.debugLog('Reconnection failed:', error);
      });
    }, delay);
  }

  /**
   * Calculate reconnection delay with exponential backoff
   * @returns {number} Delay in milliseconds
   */
  calculateReconnectDelay() {
    const baseDelay = this.config.reconnectConfig.interval;
    const multiplier = this.config.reconnectConfig.backoffMultiplier;
    const maxDelay = this.config.reconnectConfig.maxInterval;
    
    const delay = baseDelay * Math.pow(multiplier, this.reconnectAttempts - 1);
    return Math.min(delay, maxDelay);
  }

  /**
   * Start heartbeat
   */
  startHeartbeat() {
    this.stopHeartbeat();
    
    this.heartbeatTimer = setInterval(() => {
      if (this.isConnected && this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.sendHeartbeat();
      }
    }, this.config.heartbeatConfig.interval);
  }

  /**
   * Stop heartbeat
   */
  stopHeartbeat() {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
    if (this.heartbeatTimeoutTimer) {
      clearTimeout(this.heartbeatTimeoutTimer);
      this.heartbeatTimeoutTimer = null;
    }
  }

  /**
   * Send heartbeat message
   */
  async sendHeartbeat() {
    try {
      const heartbeatMessage = {
        ...this.config.heartbeatConfig.message,
        timestamp: Date.now()
      };
      
      await this.send(heartbeatMessage);
      
      // Set timeout for heartbeat response
      this.heartbeatTimeoutTimer = setTimeout(() => {
        this.debugLog('Heartbeat timeout, reconnecting...');
        this.disconnect();
        this.connect();
      }, this.config.heartbeatConfig.timeout);
      
    } catch (error) {
      this.debugLog('Heartbeat failed:', error);
    }
  }

  /**
   * Handle heartbeat response
   */
  handleHeartbeatResponse(data) {
    if (this.heartbeatTimeoutTimer) {
      clearTimeout(this.heartbeatTimeoutTimer);
      this.heartbeatTimeoutTimer = null;
    }
    this.emit('heartbeat', data);
  }

  /**
   * Set connection timeout
   */
  setConnectionTimeout(resolve, reject) {
    this.connectionTimeoutTimer = setTimeout(() => {
      if (this.isConnecting) {
        this.isConnecting = false;
        this.ws?.close();
        reject(new Error(`Connection timeout after ${this.config.timeout}ms`));
      }
    }, this.config.timeout);
  }

  /**
   * Clear connection timeout
   */
  clearConnectionTimeout() {
    if (this.connectionTimeoutTimer) {
      clearTimeout(this.connectionTimeoutTimer);
      this.connectionTimeoutTimer = null;
    }
  }

  /**
   * Clear reconnect timer
   */
  clearReconnectTimer() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  /**
   * Generate unique message ID
   * @returns {string} Unique message ID
   */
  generateMessageId() {
    return `msg_${++this.messageId}_${Date.now()}`;
  }

  /**
   * Update storage status
   * @param {string} status - New status
   * @param {Object} metadata - Additional metadata
   */
  updateStorageStatus(status, metadata = {}) {
    websocketStorage.updateConnectionStatus(this.storageKey, status, metadata);
  }

  /**
   * Debug logging
   */
  debugLog(...args) {
    if (this.config.debug) {
      console.log(`[WEBSOCKET-CLIENT-${this.storageKey}]`, ...args);
    }
  }

  /**
   * Event management methods
   */
  on(event, callback) {
    if (!this.eventListeners.has(event)) {
      this.eventListeners.set(event, []);
    }
    this.eventListeners.get(event).push(callback);
    return this;
  }

  off(event, callback) {
    if (this.eventListeners.has(event)) {
      const listeners = this.eventListeners.get(event);
      const index = listeners.indexOf(callback);
      if (index > -1) {
        listeners.splice(index, 1);
      }
    }
    return this;
  }

  once(event, callback) {
    if (!this.onceListeners.has(event)) {
      this.onceListeners.set(event, []);
    }
    this.onceListeners.get(event).push(callback);
    return this;
  }

  emit(event, data) {
    // Regular listeners
    if (this.eventListeners.has(event)) {
      this.eventListeners.get(event).forEach(callback => {
        try {
          callback(data);
        } catch (error) {
          this.debugLog(`Error in ${event} listener:`, error);
        }
      });
    }

    // Once listeners
    if (this.onceListeners.has(event)) {
      const listeners = this.onceListeners.get(event);
      listeners.forEach(callback => {
        try {
          callback(data);
        } catch (error) {
          this.debugLog(`Error in ${event} once listener:`, error);
        }
      });
      this.onceListeners.delete(event);
    }
  }

  /**
   * Get connection status
   * @returns {Object} Connection status information
   */
  getConnectionStatus() {
    return websocketStorage.getConnectionStatus(this.storageKey);
  }

  /**
   * Get ready state
   * @returns {number} WebSocket ready state
   */
  getReadyState() {
    return this.ws ? this.ws.readyState : WebSocket.CLOSED;
  }

  /**
   * Check if client is connected
   * @returns {boolean} True if connected
   */
  isClientConnected() {
    return this.isConnected && this.ws && this.ws.readyState === WebSocket.OPEN;
  }

  /**
   * Get statistics
   * @returns {Object} Client statistics
   */
  getStats() {
    return {
      storageKey: this.storageKey,
      isConnected: this.isConnected,
      isConnecting: this.isConnecting,
      reconnectAttempts: this.reconnectAttempts,
      lastError: this.lastError,
      lastMessage: this.lastMessage,
      readyState: this.getReadyState(),
      queuedMessages: websocketStorage.getMessageQueue(this.storageKey)?.length || 0,
      connectionStatus: this.getConnectionStatus()
    };
  }

  /**
   * Update configuration
   * @param {Object} newConfig - New configuration options
   * @returns {WebSocketClient} This instance for chaining
   */
  updateConfig(newConfig) {
    this.config = new WebSocketConfig({ ...this.config.toObject(), ...newConfig });
    this.config.validate();
    
    // Update storage with new configuration
    websocketStorage.setClient(this.storageKey, this, this.config.toObject());
    
    return this;
  }

  /**
   * Close the client and remove from storage
   */
  close() {
    this.disconnect();
    websocketStorage.removeClient(this.storageKey);
  }

  /**
   * Get storage statistics
   * @returns {Object} Storage statistics
   */
  static getStorageStats() {
    return websocketStorage.getStats();
  }

  /**
   * Clean up inactive clients
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {Array<string>} Array of cleaned up client keys
   */
  static cleanupInactiveClients(timeoutMs) {
    return websocketStorage.cleanupInactiveClients(timeoutMs);
  }

  /**
   * Get shared client by storage key
   * @param {string} storageKey - Storage key
   * @returns {WebSocketClient|null} Client instance or null
   */
  static getSharedClientByKey(storageKey) {
    return websocketStorage.getClient(storageKey);
  }
}

export { WebSocketClient };
export default WebSocketClient;

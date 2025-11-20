/**
 * WebSocket Client Configuration
 * Manages configuration for WebSocket server connections
 */

class WebSocketConfig {
  constructor(options = {}) {
    this.url = options.url || `ws://${window.location.host}/ws`;
    this.protocols = options.protocols || [];
    this.timeout = options.timeout || 10000; // 10 seconds default
    this.headers = options.headers || {};
    this.auth = options.auth || null;
    this.reconnectConfig = {
      enabled: options.reconnect !== false,
      maxAttempts: options.maxReconnectAttempts || 10,
      interval: options.reconnectInterval || 5000,
      backoffMultiplier: options.reconnectBackoffMultiplier || 1.5,
      maxInterval: options.maxReconnectInterval || 30000,
      retryCondition: options.retryCondition || this.defaultRetryCondition
    };
    this.heartbeatConfig = {
      enabled: options.heartbeat !== false,
      interval: options.heartbeatInterval || 30000,
      message: options.heartbeatMessage || { type: 'heartbeat', timestamp: Date.now() },
      timeout: options.heartbeatTimeout || 5000
    };
    this.binaryType = options.binaryType || 'blob';
    this.maxRetries = options.maxRetries || 3;
    this.retryDelay = options.retryDelay || 1000;
    this.debug = options.debug || false;
    this.autoConnect = options.autoConnect !== false;
    this.queueMessages = options.queueMessages !== false;
    this.maxQueueSize = options.maxQueueSize || 100;
  }

  /**
   * Default retry condition - retry on connection errors or abnormal closures
   * @param {Event} event - Close event or error
   * @returns {boolean} Whether to retry the connection
   */
  defaultRetryCondition(event) {
    if (event && event.code) {
      // Don't retry on normal closure
      if (event.code === 1000) return false;
      // Don't retry on policy violations or authentication errors
      if (event.code === 1008 || event.code === 1003) return false;
    }
    return true;
  }

  /**
   * Create a configuration for a specific server
   * @param {string} serverUrl - WebSocket server URL
   * @param {Object} options - Additional configuration options
   * @returns {WebSocketConfig} New configuration instance
   */
  static forServer(serverUrl, options = {}) {
    return new WebSocketConfig({
      url: serverUrl,
      ...options
    });
  }

  /**
   * Create a configuration for localhost development
   * @param {number} port - Port number (default: 8080)
   * @param {string} path - WebSocket path (default: '/ws')
   * @param {Object} options - Additional configuration options
   * @returns {WebSocketConfig} New configuration instance
   */
  static forLocalhost(port = 8080, path = '/ws', options = {}) {
    return new WebSocketConfig({
      url: `ws://localhost:${port}${path}`,
      timeout: 5000,
      ...options
    });
  }

  /**
   * Create a configuration for secure WebSocket (wss://)
   * @param {string} serverUrl - WebSocket server URL
   * @param {Object} options - Additional configuration options
   * @returns {WebSocketConfig} New configuration instance
   */
  static forSecureServer(serverUrl, options = {}) {
    const secureUrl = serverUrl.startsWith('wss://') ? serverUrl : serverUrl.replace('ws://', 'wss://');
    return new WebSocketConfig({
      url: secureUrl,
      ...options
    });
  }

  /**
   * Set authentication configuration
   * @param {string} type - Auth type ('bearer', 'basic', 'apikey', 'token')
   * @param {string} token - Authentication token
   * @param {Object} options - Additional auth options
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setAuth(type, token, options = {}) {
    switch (type.toLowerCase()) {
      case 'bearer':
        this.auth = { type: 'bearer', token };
        break;
      case 'basic':
        this.auth = { type: 'basic', username: options.username, password: options.password };
        break;
      case 'apikey':
        this.auth = { type: 'apikey', key: options.key || 'X-API-Key', value: token };
        break;
      case 'token':
        this.auth = { type: 'token', token };
        break;
      default:
        throw new Error(`Unsupported auth type: ${type}`);
    }
    return this;
  }

  /**
   * Set protocols
   * @param {Array<string>} protocols - WebSocket protocols
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setProtocols(protocols) {
    this.protocols = Array.isArray(protocols) ? protocols : [protocols];
    return this;
  }

  /**
   * Set timeout
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setTimeout(timeoutMs) {
    this.timeout = timeoutMs;
    return this;
  }

  /**
   * Set reconnection configuration
   * @param {Object} reconnectOptions - Reconnection options
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setReconnectConfig(reconnectOptions) {
    this.reconnectConfig = { ...this.reconnectConfig, ...reconnectOptions };
    return this;
  }

  /**
   * Set heartbeat configuration
   * @param {Object} heartbeatOptions - Heartbeat options
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setHeartbeatConfig(heartbeatOptions) {
    this.heartbeatConfig = { ...this.heartbeatConfig, ...heartbeatOptions };
    return this;
  }

  /**
   * Set binary type
   * @param {string} binaryType - Binary type ('blob' or 'arraybuffer')
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setBinaryType(binaryType) {
    if (['blob', 'arraybuffer'].includes(binaryType)) {
      this.binaryType = binaryType;
    } else {
      throw new Error('binaryType must be either "blob" or "arraybuffer"');
    }
    return this;
  }

  /**
   * Enable or disable debug mode
   * @param {boolean} enabled - Whether to enable debug mode
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setDebug(enabled) {
    this.debug = !!enabled;
    return this;
  }

  /**
   * Enable or disable auto-connect
   * @param {boolean} enabled - Whether to enable auto-connect
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setAutoConnect(enabled) {
    this.autoConnect = !!enabled;
    return this;
  }

  /**
   * Enable or disable message queuing
   * @param {boolean} enabled - Whether to enable message queuing
   * @param {number} maxSize - Maximum queue size
   * @returns {WebSocketConfig} This configuration instance for chaining
   */
  setQueueMessages(enabled, maxSize = 100) {
    this.queueMessages = !!enabled;
    this.maxQueueSize = maxSize;
    return this;
  }

  /**
   * Clone this configuration
   * @returns {WebSocketConfig} New configuration instance with same settings
   */
  clone() {
    return new WebSocketConfig({
      url: this.url,
      protocols: [...this.protocols],
      timeout: this.timeout,
      headers: { ...this.headers },
      auth: this.auth ? { ...this.auth } : null,
      reconnectConfig: { ...this.reconnectConfig },
      heartbeatConfig: { ...this.heartbeatConfig },
      binaryType: this.binaryType,
      maxRetries: this.maxRetries,
      retryDelay: this.retryDelay,
      debug: this.debug,
      autoConnect: this.autoConnect,
      queueMessages: this.queueMessages,
      maxQueueSize: this.maxQueueSize
    });
  }

  /**
   * Validate the configuration
   * @returns {boolean} True if configuration is valid
   * @throws {Error} If configuration is invalid
   */
  validate() {
    if (!this.url || typeof this.url !== 'string') {
      throw new Error('url must be a valid string');
    }

    if (!this.url.startsWith('ws://') && !this.url.startsWith('wss://')) {
      throw new Error('url must start with ws:// or wss://');
    }

    if (this.timeout && (typeof this.timeout !== 'number' || this.timeout <= 0)) {
      throw new Error('timeout must be a positive number');
    }

    if (this.reconnectConfig.maxAttempts && (typeof this.reconnectConfig.maxAttempts !== 'number' || this.reconnectConfig.maxAttempts < 0)) {
      throw new Error('maxAttempts must be a non-negative number');
    }

    if (this.reconnectConfig.interval && (typeof this.reconnectConfig.interval !== 'number' || this.reconnectConfig.interval <= 0)) {
      throw new Error('reconnect interval must be a positive number');
    }

    if (this.heartbeatConfig.interval && (typeof this.heartbeatConfig.interval !== 'number' || this.heartbeatConfig.interval <= 0)) {
      throw new Error('heartbeat interval must be a positive number');
    }

    if (this.maxQueueSize && (typeof this.maxQueueSize !== 'number' || this.maxQueueSize <= 0)) {
      throw new Error('maxQueueSize must be a positive number');
    }

    return true;
  }

  /**
   * Convert configuration to a plain object
   * @returns {Object} Plain object representation
   */
  toObject() {
    return {
      url: this.url,
      protocols: [...this.protocols],
      timeout: this.timeout,
      headers: { ...this.headers },
      auth: this.auth,
      reconnectConfig: { ...this.reconnectConfig },
      heartbeatConfig: { ...this.heartbeatConfig },
      binaryType: this.binaryType,
      maxRetries: this.maxRetries,
      retryDelay: this.retryDelay,
      debug: this.debug,
      autoConnect: this.autoConnect,
      queueMessages: this.queueMessages,
      maxQueueSize: this.maxQueueSize
    };
  }
}

export { WebSocketConfig };
export default WebSocketConfig;

/**
 * HTTP Client Configuration
 * Manages configuration for HTTP server connections
 */

class HttpConfig {
  constructor(options = {}) {
    this.baseURL = options.baseURL || '';
    this.timeout = options.timeout || 10000; // 10 seconds default
    this.headers = options.headers || {};
    this.auth = options.auth || null;
    this.retryConfig = {
      maxRetries: options.maxRetries || 3,
      retryDelay: options.retryDelay || 1000,
      retryCondition: options.retryCondition || this.defaultRetryCondition
    };
    this.ssl = options.ssl || {};
    this.proxy = options.proxy || null;
    this.followRedirects = options.followRedirects !== false;
    this.maxRedirects = options.maxRedirects || 5;
    this.validateStatus = options.validateStatus || this.defaultValidateStatus;
  }

  /**
   * Default retry condition - retry on network errors or 5xx status codes
   * @param {Error} error - Error that occurred
   * @param {Object} response - HTTP response object
   * @returns {boolean} Whether to retry the request
   */
  defaultRetryCondition(error, response) {
    return !!error || (response && response.status >= 500);
  }

  /**
   * Default status validation - consider 2xx and 3xx as successful
   * @param {number} status - HTTP status code
   * @returns {boolean} Whether the status indicates success
   */
  defaultValidateStatus(status) {
    return status >= 200 && status < 400;
  }

  /**
   * Create a configuration for a specific server
   * @param {string} serverUrl - Base URL of the server
   * @param {Object} options - Additional configuration options
   * @returns {HttpConfig} New configuration instance
   */
  static forServer(serverUrl, options = {}) {
    return new HttpConfig({
      baseURL: serverUrl,
      ...options
    });
  }

  /**
   * Create a configuration for localhost development
   * @param {number} port - Port number (default: 3000)
   * @param {Object} options - Additional configuration options
   * @returns {HttpConfig} New configuration instance
   */
  static forLocalhost(port = 3000, options = {}) {
    return new HttpConfig({
      baseURL: `http://localhost:${port}`,
      timeout: 5000,
      ...options
    });
  }

  /**
   * Set authentication configuration
   * @param {string} type - Auth type ('bearer', 'basic', 'apikey')
   * @param {string} token - Authentication token
   * @param {Object} options - Additional auth options
   * @returns {HttpConfig} This configuration instance for chaining
   */
  setAuth(type, token, options = {}) {
    switch (type.toLowerCase()) {
      case 'bearer':
        this.auth = { type: 'bearer', token };
        this.headers.Authorization = `Bearer ${token}`;
        break;
      case 'basic':
        this.auth = { type: 'basic', username: options.username, password: options.password };
        const credentials = btoa(`${options.username}:${options.password}`);
        this.headers.Authorization = `Basic ${credentials}`;
        break;
      case 'apikey':
        this.auth = { type: 'apikey', key: options.key || 'X-API-Key', value: token };
        this.headers[options.key || 'X-API-Key'] = token;
        break;
      default:
        throw new Error(`Unsupported auth type: ${type}`);
    }
    return this;
  }

  /**
   * Set default headers
   * @param {Object} headers - Headers to set
   * @returns {HttpConfig} This configuration instance for chaining
   */
  setHeaders(headers) {
    this.headers = { ...this.headers, ...headers };
    return this;
  }

  /**
   * Set timeout
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {HttpConfig} This configuration instance for chaining
   */
  setTimeout(timeoutMs) {
    this.timeout = timeoutMs;
    return this;
  }

  /**
   * Set retry configuration
   * @param {Object} retryOptions - Retry options
   * @returns {HttpConfig} This configuration instance for chaining
   */
  setRetryConfig(retryOptions) {
    this.retryConfig = { ...this.retryConfig, ...retryOptions };
    return this;
  }

  /**
   * Set SSL configuration
   * @param {Object} sslOptions - SSL options
   * @returns {HttpConfig} This configuration instance for chaining
   */
  setSslConfig(sslOptions) {
    this.ssl = { ...this.ssl, ...sslOptions };
    return this;
  }

  /**
   * Set proxy configuration
   * @param {string} proxyUrl - Proxy URL
   * @param {Object} options - Additional proxy options
   * @returns {HttpConfig} This configuration instance for chaining
   */
  setProxy(proxyUrl, options = {}) {
    this.proxy = {
      url: proxyUrl,
      ...options
    };
    return this;
  }

  /**
   * Clone this configuration
   * @returns {HttpConfig} New configuration instance with same settings
   */
  clone() {
    return new HttpConfig({
      baseURL: this.baseURL,
      timeout: this.timeout,
      headers: { ...this.headers },
      auth: this.auth ? { ...this.auth } : null,
      retryConfig: { ...this.retryConfig },
      ssl: { ...this.ssl },
      proxy: this.proxy ? { ...this.proxy } : null,
      followRedirects: this.followRedirects,
      maxRedirects: this.maxRedirects,
      validateStatus: this.validateStatus
    });
  }

  /**
   * Validate the configuration
   * @returns {boolean} True if configuration is valid
   * @throws {Error} If configuration is invalid
   */
  validate() {
    if (!this.baseURL && typeof this.baseURL !== 'string') {
      throw new Error('baseURL must be a valid string');
    }

    if (this.timeout && (typeof this.timeout !== 'number' || this.timeout <= 0)) {
      throw new Error('timeout must be a positive number');
    }

    if (this.auth && this.auth.type === 'basic') {
      if (!this.auth.username || !this.auth.password) {
        throw new Error('Basic auth requires username and password');
      }
    }

    return true;
  }

  /**
   * Convert configuration to a plain object
   * @returns {Object} Plain object representation
   */
  toObject() {
    return {
      baseURL: this.baseURL,
      timeout: this.timeout,
      headers: { ...this.headers },
      auth: this.auth,
      retryConfig: { ...this.retryConfig },
      ssl: { ...this.ssl },
      proxy: this.proxy,
      followRedirects: this.followRedirects,
      maxRedirects: this.maxRedirects
    };
  }
}

export { HttpConfig };
export default HttpConfig;

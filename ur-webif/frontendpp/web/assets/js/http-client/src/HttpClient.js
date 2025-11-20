import { httpStorage } from '../lib/http-storage.js';
import { HttpConfig } from '../lib/config.js';

/**
 * Generic HTTP Client Implementation
 * Provides a unified interface for making HTTP requests with shared storage support
 */

class HttpClient {
  constructor(config = {}) {
    if (typeof config === 'string') {
      // If config is a string, treat it as baseURL
      this.config = new HttpConfig({ baseURL: config });
    } else if (config instanceof HttpConfig) {
      this.config = config;
    } else {
      this.config = new HttpConfig(config);
    }
    
    this.config.validate();
    this.storageKey = this.generateStorageKey();
  }

  /**
   * Generate a unique storage key based on configuration
   * @returns {string} Unique storage key
   */
  generateStorageKey() {
    const configString = JSON.stringify(this.config.toObject());
    const configHash = btoa(configString).slice(0, 16);
    return `http-client-${configHash}`;
  }

  /**
   * Get or create a shared client instance
   * @param {Object} config - Configuration for the client
   * @returns {HttpClient} Shared client instance
   */
  static getSharedClient(config = {}) {
    const client = new HttpClient(config);
    const storageKey = client.storageKey;

    // Check if client already exists in storage
    if (httpStorage.hasClient(storageKey)) {
      return httpStorage.getClient(storageKey);
    }

    // Store new client in storage
    httpStorage.setClient(storageKey, client, client.config.toObject());
    return client;
  }

  /**
   * Make an HTTP request
   * @param {string} method - HTTP method (GET, POST, PUT, DELETE, etc.)
   * @param {string} url - Request URL (relative to baseURL or absolute)
   * @param {Object} options - Request options
   * @returns {Promise<Object>} Response object
   */
  async request(method, url, options = {}) {
    const fullUrl = this.buildUrl(url);
    const requestOptions = this.buildRequestOptions(method, options);

    let lastError;
    let attempt = 0;

    while (attempt <= this.config.retryConfig.maxRetries) {
      try {
        const response = await this.makeRequestWithTimeout(fullUrl, requestOptions);
        
        // Update storage usage
        httpStorage.updateConnectionStatus(this.storageKey, 'connected');
        
        return this.processResponse(response);
      } catch (error) {
        lastError = error;
        attempt++;

        // Check if we should retry
        if (attempt <= this.config.retryConfig.maxRetries && 
            this.config.retryConfig.retryCondition(error, null)) {
          await this.delay(this.config.retryConfig.retryDelay * attempt);
          continue;
        }

        // Update storage status on error
        httpStorage.updateConnectionStatus(this.storageKey, 'error');
        throw error;
      }
    }

    throw lastError;
  }

  /**
   * Make HTTP GET request
   * @param {string} url - Request URL
   * @param {Object} options - Request options
   * @returns {Promise<Object>} Response object
   */
  async get(url, options = {}) {
    return this.request('GET', url, options);
  }

  /**
   * Make HTTP POST request
   * @param {string} url - Request URL
   * @param {Object} data - Request body data
   * @param {Object} options - Request options
   * @returns {Promise<Object>} Response object
   */
  async post(url, data = null, options = {}) {
    return this.request('POST', url, { ...options, data });
  }

  /**
   * Make HTTP PUT request
   * @param {string} url - Request URL
   * @param {Object} data - Request body data
   * @param {Object} options - Request options
   * @returns {Promise<Object>} Response object
   */
  async put(url, data = null, options = {}) {
    return this.request('PUT', url, { ...options, data });
  }

  /**
   * Make HTTP PATCH request
   * @param {string} url - Request URL
   * @param {Object} data - Request body data
   * @param {Object} options - Request options
   * @returns {Promise<Object>} Response object
   */
  async patch(url, data = null, options = {}) {
    return this.request('PATCH', url, { ...options, data });
  }

  /**
   * Make HTTP DELETE request
   * @param {string} url - Request URL
   * @param {Object} options - Request options
   * @returns {Promise<Object>} Response object
   */
  async delete(url, options = {}) {
    return this.request('DELETE', url, options);
  }

  /**
   * Build full URL from base URL and relative path
   * @param {string} url - Relative or absolute URL
   * @returns {string} Full URL
   */
  buildUrl(url) {
    if (url.startsWith('http://') || url.startsWith('https://')) {
      return url;
    }
    
    const baseURL = this.config.baseURL.endsWith('/') 
      ? this.config.baseURL.slice(0, -1) 
      : this.config.baseURL;
    const path = url.startsWith('/') ? url : `/${url}`;
    
    return `${baseURL}${path}`;
  }

  /**
   * Build request options for fetch
   * @param {string} method - HTTP method
   * @param {Object} options - Additional options
   * @returns {Object} Fetch request options
   */
  buildRequestOptions(method, options = {}) {
    const requestOptions = {
      method: method.toUpperCase(),
      headers: { ...this.config.headers, ...options.headers }
    };

    // Add request body if provided
    if (options.data) {
      if (typeof options.data === 'object') {
        requestOptions.body = JSON.stringify(options.data);
        requestOptions.headers['Content-Type'] = 'application/json';
      } else {
        requestOptions.body = options.data;
      }
    }

    // Add query parameters
    if (options.params) {
      const url = new URL(requestOptions.url || this.config.baseURL);
      Object.entries(options.params).forEach(([key, value]) => {
        if (value !== null && value !== undefined) {
          url.searchParams.append(key, value);
        }
      });
      requestOptions.url = url.toString();
    }

    return requestOptions;
  }

  /**
   * Make request with timeout using AbortController
   * @param {string} url - Request URL
   * @param {Object} options - Request options
   * @returns {Promise<Response>} Fetch response
   */
  async makeRequestWithTimeout(url, options) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), this.config.timeout);

    try {
      const response = await fetch(url, {
        ...options,
        signal: controller.signal
      });
      clearTimeout(timeoutId);
      return response;
    } catch (error) {
      clearTimeout(timeoutId);
      
      if (error.name === 'AbortError') {
        throw new Error(`Request timeout after ${this.config.timeout}ms`);
      }
      
      throw error;
    }
  }

  /**
   * Process HTTP response
   * @param {Response} response - Fetch response object
   * @returns {Object} Processed response
   */
  async processResponse(response) {
    let data;
    const contentType = response.headers.get('content-type');

    try {
      if (contentType && contentType.includes('application/json')) {
        data = await response.json();
      } else {
        data = await response.text();
      }
    } catch (error) {
      data = null;
    }

    const processedResponse = {
      status: response.status,
      statusText: response.statusText,
      headers: Object.fromEntries(response.headers.entries()),
      data: data,
      url: response.url,
      ok: response.ok
    };

    // Check if response status is considered successful
    if (!this.config.validateStatus(response.status)) {
      const error = new Error(`HTTP Error: ${response.status} ${response.statusText}`);
      error.response = processedResponse;
      throw error;
    }

    return processedResponse;
  }

  /**
   * Delay execution for specified milliseconds
   * @param {number} ms - Milliseconds to delay
   * @returns {Promise} Promise that resolves after delay
   */
  delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Get connection status
   * @returns {Object} Connection status information
   */
  getConnectionStatus() {
    return httpStorage.getConnectionStatus(this.storageKey);
  }

  /**
   * Update configuration
   * @param {Object} newConfig - New configuration options
   * @returns {HttpClient} This instance for chaining
   */
  updateConfig(newConfig) {
    this.config = new HttpConfig({ ...this.config.toObject(), ...newConfig });
    this.config.validate();
    
    // Update storage with new configuration
    httpStorage.setClient(this.storageKey, this, this.config.toObject());
    
    return this;
  }

  /**
   * Close the client and remove from storage
   */
  close() {
    httpStorage.removeClient(this.storageKey);
  }

  /**
   * Get storage statistics
   * @returns {Object} Storage statistics
   */
  static getStorageStats() {
    return httpStorage.getStats();
  }

  /**
   * Clean up inactive clients
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {Array<string>} Array of cleaned up client keys
   */
  static cleanupInactiveClients(timeoutMs) {
    return httpStorage.cleanupInactiveClients(timeoutMs);
  }
}

export { HttpClient };
export default HttpClient;

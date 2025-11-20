/**
 * JWT Configuration
 * Manages configuration for JWT session handling
 */

class JwtConfig {
  constructor(options = {}) {
    // JWT Token Configuration
    // Use browser-compatible defaults (process.env is not available in browser)
    this.secretKey = options.secretKey || 'frontendpp-jwt-secret-key';
    this.algorithm = options.algorithm || 'HS256';
    this.issuer = options.issuer || 'webmodules-auth';
    this.audience = options.audience || 'webmodules-client';
    
    // Token Expiration
    this.accessTokenExpiry = options.accessTokenExpiry || '15m';
    this.refreshTokenExpiry = options.refreshTokenExpiry || '7d';
    this.sessionTimeout = options.sessionTimeout || 60 * 60 * 1000; // 1 hour in ms
    
    // Token Refresh Configuration
    this.refreshThreshold = options.refreshThreshold || 5 * 60 * 1000; // 5 minutes before expiry
    this.autoRefresh = options.autoRefresh !== false;
    this.maxRefreshAttempts = options.maxRefreshAttempts || 3;
    this.refreshRetryDelay = options.refreshRetryDelay || 1000;
    
    // Storage Configuration
    this.storageKey = options.storageKey || 'jwt-session';
    this.persistSession = options.persistSession !== false;
    this.sessionStorageType = options.sessionStorageType || 'memory'; // 'memory', 'localStorage', 'sessionStorage'
    
    // Validation Configuration
    this.validateIssuer = options.validateIssuer !== false;
    this.validateAudience = options.validateAudience !== false;
    this.validateExpiration = options.validateExpiration !== false;
    this.clockSkewSeconds = options.clockSkewSeconds || 30;
    
    // Security Configuration
    this.blacklistEnabled = options.blacklistEnabled || false;
    this.rateLimitEnabled = options.rateLimitEnabled || false;
    this.maxLoginAttempts = options.maxLoginAttempts || 5;
    this.lockoutDuration = options.lockoutDuration || 15 * 60 * 1000; // 15 minutes
    
    // Endpoints
    this.endpoints = {
      login: options.loginEndpoint || '/auth/login',
      logout: options.logoutEndpoint || '/auth/logout',
      refresh: options.refreshEndpoint || '/auth/refresh',
      validate: options.validateEndpoint || '/auth/validate',
      ...options.endpoints
    };
    
    // Headers
    this.headers = {
      authorization: options.authorizationHeader || 'Authorization',
      refreshToken: options.refreshTokenHeader || 'X-Refresh-Token',
      ...options.headers
    };
  }

  /**
   * Create a configuration for development
   * @param {Object} options - Additional configuration options
   * @returns {JwtConfig} New configuration instance
   */
  static forDevelopment(options = {}) {
    return new JwtConfig({
      secretKey: 'dev-secret-key',
      accessTokenExpiry: '1h',
      refreshTokenExpiry: '1d',
      sessionTimeout: 30 * 60 * 1000, // 30 minutes
      validateIssuer: false,
      validateAudience: false,
      ...options
    });
  }

  /**
   * Create production configuration
   * @param {Object} options - Configuration options
   * @returns {JwtConfig} New configuration instance
   */
  static forProduction(options = {}) {
    // Browser-compatible: require explicit secret key for production
    if (!options.secretKey) {
      throw new Error('JWT secret key is required for production');
    }

    return new JwtConfig({
      accessTokenExpiry: '15m',
      refreshTokenExpiry: '7d',
      sessionTimeout: 60 * 60 * 1000, // 1 hour
      validateIssuer: true,
      validateAudience: true,
      blacklistEnabled: true,
      rateLimitEnabled: true,
      ...options
    });
  }

  /**
   * Create a configuration for testing
   * @param {Object} options - Additional configuration options
   * @returns {JwtConfig} New configuration instance
   */
  static forTesting(options = {}) {
    return new JwtConfig({
      secretKey: 'test-secret-key',
      accessTokenExpiry: '60s',
      refreshTokenExpiry: '5m',
      sessionTimeout: 10 * 60 * 1000, // 10 minutes
      validateIssuer: false,
      validateAudience: false,
      autoRefresh: false,
      ...options
    });
  }

  /**
   * Set JWT secret key
   * @param {string} secretKey - JWT secret key
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setSecretKey(secretKey) {
    this.secretKey = secretKey;
    return this;
  }

  /**
   * Set token expiration times
   * @param {string} accessTokenExpiry - Access token expiry (e.g., '15m', '1h')
   * @param {string} refreshTokenExpiry - Refresh token expiry (e.g., '7d', '30d')
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setTokenExpiry(accessTokenExpiry, refreshTokenExpiry) {
    this.accessTokenExpiry = accessTokenExpiry;
    this.refreshTokenExpiry = refreshTokenExpiry;
    return this;
  }

  /**
   * Set session timeout
   * @param {number} timeoutMs - Session timeout in milliseconds
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setSessionTimeout(timeoutMs) {
    this.sessionTimeout = timeoutMs;
    return this;
  }

  /**
   * Enable or disable auto refresh
   * @param {boolean} enabled - Whether auto refresh is enabled
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setAutoRefresh(enabled) {
    this.autoRefresh = enabled;
    return this;
  }

  /**
   * Set refresh configuration
   * @param {Object} refreshOptions - Refresh options
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setRefreshConfig(refreshOptions) {
    this.refreshThreshold = refreshOptions.threshold || this.refreshThreshold;
    this.maxRefreshAttempts = refreshOptions.maxAttempts || this.maxRefreshAttempts;
    this.refreshRetryDelay = refreshOptions.retryDelay || this.refreshRetryDelay;
    return this;
  }

  /**
   * Set authentication endpoints
   * @param {Object} endpoints - Endpoint URLs
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setEndpoints(endpoints) {
    this.endpoints = { ...this.endpoints, ...endpoints };
    return this;
  }

  /**
   * Set custom headers
   * @param {Object} headers - Custom headers
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setHeaders(headers) {
    this.headers = { ...this.headers, ...headers };
    return this;
  }

  /**
   * Enable security features
   * @param {Object} securityOptions - Security options
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setSecurity(securityOptions) {
    if (securityOptions.blacklist !== undefined) {
      this.blacklistEnabled = securityOptions.blacklist;
    }
    if (securityOptions.rateLimit !== undefined) {
      this.rateLimitEnabled = securityOptions.rateLimit;
    }
    if (securityOptions.maxLoginAttempts) {
      this.maxLoginAttempts = securityOptions.maxLoginAttempts;
    }
    if (securityOptions.lockoutDuration) {
      this.lockoutDuration = securityOptions.lockoutDuration;
    }
    return this;
  }

  /**
   * Set validation options
   * @param {Object} validationOptions - Validation options
   * @returns {JwtConfig} This configuration instance for chaining
   */
  setValidation(validationOptions) {
    if (validationOptions.issuer !== undefined) {
      this.validateIssuer = validationOptions.issuer;
    }
    if (validationOptions.audience !== undefined) {
      this.validateAudience = validationOptions.audience;
    }
    if (validationOptions.expiration !== undefined) {
      this.validateExpiration = validationOptions.expiration;
    }
    if (validationOptions.clockSkewSeconds !== undefined) {
      this.clockSkewSeconds = validationOptions.clockSkewSeconds;
    }
    return this;
  }

  /**
   * Convert time string to milliseconds
   * @param {string} timeStr - Time string (e.g., '15m', '1h', '7d')
   * @returns {number} Time in milliseconds
   */
  static timeToMilliseconds(timeStr) {
    const timeValue = parseInt(timeStr);
    const unit = timeStr.slice(-1);
    
    switch (unit) {
      case 's': return timeValue * 1000;
      case 'm': return timeValue * 60 * 1000;
      case 'h': return timeValue * 60 * 60 * 1000;
      case 'd': return timeValue * 24 * 60 * 60 * 1000;
      default: return timeValue;
    }
  }

  /**
   * Get access token expiry in milliseconds
   * @returns {number} Access token expiry in milliseconds
   */
  getAccessTokenExpiryMs() {
    return JwtConfig.timeToMilliseconds(this.accessTokenExpiry);
  }

  /**
   * Get refresh token expiry in milliseconds
   * @returns {number} Refresh token expiry in milliseconds
   */
  getRefreshTokenExpiryMs() {
    return JwtConfig.timeToMilliseconds(this.refreshTokenExpiry);
  }

  /**
   * Validate the configuration
   * @returns {boolean} True if configuration is valid
   * @throws {Error} If configuration is invalid
   */
  validate() {
    if (!this.secretKey || typeof this.secretKey !== 'string') {
      throw new Error('secretKey must be a non-empty string');
    }

    if (!this.algorithm || typeof this.algorithm !== 'string') {
      throw new Error('algorithm must be a valid string');
    }

    if (this.sessionTimeout && (typeof this.sessionTimeout !== 'number' || this.sessionTimeout <= 0)) {
      throw new Error('sessionTimeout must be a positive number');
    }

    const validAlgorithms = ['HS256', 'HS384', 'HS512', 'RS256', 'RS384', 'RS512'];
    if (!validAlgorithms.includes(this.algorithm)) {
      throw new Error(`Invalid algorithm. Supported: ${validAlgorithms.join(', ')}`);
    }

    return true;
  }

  /**
   * Clone this configuration
   * @returns {JwtConfig} New configuration instance with same settings
   */
  clone() {
    return new JwtConfig({
      secretKey: this.secretKey,
      algorithm: this.algorithm,
      issuer: this.issuer,
      audience: this.audience,
      accessTokenExpiry: this.accessTokenExpiry,
      refreshTokenExpiry: this.refreshTokenExpiry,
      sessionTimeout: this.sessionTimeout,
      refreshThreshold: this.refreshThreshold,
      autoRefresh: this.autoRefresh,
      maxRefreshAttempts: this.maxRefreshAttempts,
      refreshRetryDelay: this.refreshRetryDelay,
      storageKey: this.storageKey,
      persistSession: this.persistSession,
      sessionStorageType: this.sessionStorageType,
      validateIssuer: this.validateIssuer,
      validateAudience: this.validateAudience,
      validateExpiration: this.validateExpiration,
      clockSkewSeconds: this.clockSkewSeconds,
      blacklistEnabled: this.blacklistEnabled,
      rateLimitEnabled: this.rateLimitEnabled,
      maxLoginAttempts: this.maxLoginAttempts,
      lockoutDuration: this.lockoutDuration,
      endpoints: { ...this.endpoints },
      headers: { ...this.headers }
    });
  }

  /**
   * Convert configuration to a plain object
   * @returns {Object} Plain object representation
   */
  toObject() {
    return {
      secretKey: this.secretKey,
      algorithm: this.algorithm,
      issuer: this.issuer,
      audience: this.audience,
      accessTokenExpiry: this.accessTokenExpiry,
      refreshTokenExpiry: this.refreshTokenExpiry,
      sessionTimeout: this.sessionTimeout,
      refreshThreshold: this.refreshThreshold,
      autoRefresh: this.autoRefresh,
      maxRefreshAttempts: this.maxRefreshAttempts,
      refreshRetryDelay: this.refreshRetryDelay,
      storageKey: this.storageKey,
      persistSession: this.persistSession,
      sessionStorageType: this.sessionStorageType,
      validateIssuer: this.validateIssuer,
      validateAudience: this.validateAudience,
      validateExpiration: this.validateExpiration,
      clockSkewSeconds: this.clockSkewSeconds,
      blacklistEnabled: this.blacklistEnabled,
      rateLimitEnabled: this.rateLimitEnabled,
      maxLoginAttempts: this.maxLoginAttempts,
      lockoutDuration: this.lockoutDuration,
      endpoints: { ...this.endpoints },
      headers: { ...this.headers }
    };
  }
}

export { JwtConfig };
export default JwtConfig;

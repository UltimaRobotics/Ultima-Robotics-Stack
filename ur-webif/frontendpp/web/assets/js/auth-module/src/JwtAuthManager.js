import { authStorage } from '../lib/auth-storage.js';
import { JwtConfig } from '../lib/jwt-config.js';
import { JwtUtils } from '../lib/jwt-utils.js';

/**
 * JWT Authentication Manager
 * Main class for handling JWT-based authentication with shared session storage
 */

class JwtAuthManager {
  constructor(config = {}) {
    if (config instanceof JwtConfig) {
      this.config = config;
    } else {
      this.config = new JwtConfig(config);
    }
    
    this.config.validate();
    this.storageKey = this.config.storageKey;
    this.refreshTimer = null;
    this.isInitialized = false;
  }

  /**
   * Initialize the auth manager
   * @returns {Promise<void>}
   */
  async initialize() {
    if (this.isInitialized) {
      return;
    }

    // Clean up expired sessions
    authStorage.cleanupExpiredSessions();

    // Start auto-refresh timer if enabled
    if (this.config.autoRefresh) {
      this.startRefreshTimer();
    }

    this.isInitialized = true;
  }

  /**
   * Get or create a shared auth manager instance
   * @param {Object} config - Configuration for the auth manager
   * @returns {JwtAuthManager} Shared auth manager instance
   */
  static getSharedAuthManager(config = {}) {
    const manager = new JwtAuthManager(config);
    const storageKey = manager.storageKey;

    // Check if auth manager already exists in storage
    if (authStorage.hasSession(storageKey)) {
      const existingSession = authStorage.getSession(storageKey);
      if (existingSession.authManager) {
        return existingSession.authManager;
      }
    }

    // Store new auth manager in storage
    authStorage.setSession(storageKey, { authManager: manager }, this.config.toObject());
    return manager;
  }

  /**
   * Authenticate user with credentials
   * @param {Object} credentials - User credentials (username, password, etc.)
   * @param {Object} options - Additional authentication options
   * @returns {Promise<Object>} Authentication result with tokens
   */
  async authenticate(credentials, options = {}) {
    this.ensureInitialized();

    try {
      // This would typically make an API call to your authentication endpoint
      // For demo purposes, we'll create tokens directly
      const authResponse = await this.performAuthentication(credentials, options);
      
      // Create session data
      const sessionData = {
        user: authResponse.user,
        accessToken: authResponse.accessToken,
        refreshToken: authResponse.refreshToken,
        tokenType: authResponse.tokenType || 'Bearer',
        expiresAt: this.calculateExpiryDate(authResponse.expiresIn),
        isAuthenticated: true,
        authProvider: options.provider || 'default'
      };

      // Store session in shared storage
      authStorage.setSession(this.storageKey, sessionData, this.config.toObject());

      // Start refresh timer if enabled
      if (this.config.autoRefresh) {
        this.startRefreshTimer();
      }

      return {
        success: true,
        user: sessionData.user,
        tokens: {
          accessToken: sessionData.accessToken,
          refreshToken: sessionData.refreshToken,
          tokenType: sessionData.tokenType,
          expiresIn: authResponse.expiresIn
        },
        sessionKey: this.storageKey
      };

    } catch (error) {
      throw new Error(`Authentication failed: ${error.message}`);
    }
  }

  /**
   * Perform actual authentication (override in implementation)
   * @param {Object} credentials - User credentials
   * @param {Object} options - Authentication options
   * @returns {Promise<Object>} Authentication response
   */
  async performAuthentication(credentials, options = {}) {
    // This is a mock implementation - override with actual authentication logic
    if (!credentials.username || !credentials.password) {
      throw new Error('Username and password are required');
    }

    // Mock user validation
    if (credentials.username === 'demo' && credentials.password === 'demo') {
      const user = {
        id: '1',
        username: 'demo',
        email: 'demo@example.com',
        roles: ['user']
      };

      // Create JWT tokens
      const tokens = JwtUtils.createTokenPair(
        { 
          sub: user.id,
          username: user.username,
          email: user.email,
          roles: user.roles
        },
        this.config.secretKey,
        {
          algorithm: this.config.algorithm,
          issuer: this.config.issuer,
          audience: this.config.audience,
          accessTokenExpiry: this.config.accessTokenExpiry,
          refreshTokenExpiry: this.config.refreshTokenExpiry
        }
      );

      return {
        user,
        ...tokens,
        expiresIn: JwtUtils.parseExpiration(this.config.accessTokenExpiry)
      };
    }

    throw new Error('Invalid credentials');
  }

  /**
   * Get current session
   * @returns {Object|null} Current session data or null if not authenticated
   */
  getCurrentSession() {
    const session = authStorage.getSession(this.storageKey);
    if (!session || !session.isAuthenticated) {
      return null;
    }

    // Check if session is expired
    if (authStorage.isSessionExpired(this.storageKey)) {
      this.logout();
      return null;
    }

    return session;
  }

  /**
   * Check if user is authenticated
   * @returns {boolean} True if authenticated
   */
  isAuthenticated() {
    const session = this.getCurrentSession();
    return session && session.isAuthenticated;
  }

  /**
   * Get current user
   * @returns {Object|null} Current user data or null if not authenticated
   */
  getCurrentUser() {
    const session = this.getCurrentSession();
    return session ? session.user : null;
  }

  /**
   * Get access token
   * @returns {string|null} Access token or null if not authenticated
   */
  getAccessToken() {
    const session = this.getCurrentSession();
    return session ? session.accessToken : null;
  }

  /**
   * Get refresh token
   * @returns {string|null} Refresh token or null if not authenticated
   */
  getRefreshToken() {
    const session = this.getCurrentSession();
    return session ? session.refreshToken : null;
  }

  /**
   * Refresh access token
   * @param {string} refreshToken - Refresh token (optional, will use current if not provided)
   * @returns {Promise<Object>} New tokens
   */
  async refreshTokens(refreshToken = null) {
    this.ensureInitialized();

    const tokenToUse = refreshToken || this.getRefreshToken();
    if (!tokenToUse) {
      throw new Error('No refresh token available');
    }

    try {
      // Verify refresh token
      const payload = JwtUtils.verifyToken(tokenToUse, this.config.secretKey, {
        algorithm: this.config.algorithm,
        issuer: this.config.issuer,
        audience: this.config.audience
      });

      if (payload.type !== 'refresh') {
        throw new Error('Invalid refresh token');
      }

      // Create new access token
      const newAccessToken = JwtUtils.createToken(
        {
          sub: payload.sub,
          username: payload.username,
          email: payload.email,
          roles: payload.roles
        },
        this.config.secretKey,
        {
          algorithm: this.config.algorithm,
          issuer: this.config.issuer,
          audience: this.config.audience,
          expiresIn: this.config.accessTokenExpiry,
          jwtId: JwtUtils.generateJwtId()
        }
      );

      // Update session with new access token
      const session = authStorage.getSession(this.storageKey);
      if (session) {
        authStorage.updateSessionTokens(this.storageKey, {
          accessToken: newAccessToken,
          expiresAt: this.calculateExpiryDate(JwtUtils.parseExpiration(this.config.accessTokenExpiry))
        });
      }

      return {
        accessToken: newAccessToken,
        refreshToken: tokenToUse, // Keep the same refresh token
        tokenType: 'Bearer',
        expiresIn: JwtUtils.parseExpiration(this.config.accessTokenExpiry)
      };

    } catch (error) {
      // Refresh token is invalid, logout user
      this.logout();
      throw new Error(`Token refresh failed: ${error.message}`);
    }
  }

  /**
   * Validate access token
   * @param {string} token - Access token to validate (optional, will use current if not provided)
   * @returns {Object|null} Token payload or null if invalid
   */
  validateToken(token = null) {
    const tokenToValidate = token || this.getAccessToken();
    if (!tokenToValidate) {
      return null;
    }

    try {
      const payload = JwtUtils.verifyToken(tokenToValidate, this.config.secretKey, {
        algorithm: this.config.algorithm,
        issuer: this.config.issuer,
        audience: this.config.audience,
        validateExpiration: this.config.validateExpiration,
        clockSkewSeconds: this.config.clockSkewSeconds
      });

      return payload;
    } catch (error) {
      return null;
    }
  }

  /**
   * Logout user and clear session
   */
  logout() {
    // Stop refresh timer
    this.stopRefreshTimer();

    // Remove session from storage
    authStorage.removeSession(this.storageKey);
  }

  /**
   * Add authorization header to request options
   * @param {Object} requestOptions - Request options object
   * @returns {Object} Request options with authorization header
   */
  addAuthorizationHeader(requestOptions = {}) {
    const token = this.getAccessToken();
    if (!token) {
      return requestOptions;
    }

    const headers = {
      ...requestOptions.headers,
      [this.config.headers.authorization]: `Bearer ${token}`
    };

    return {
      ...requestOptions,
      headers
    };
  }

  /**
   * Check if token needs refresh
   * @returns {boolean} True if token should be refreshed
   */
  shouldRefreshToken() {
    const token = this.getAccessToken();
    if (!token) {
      return false;
    }

    const remainingTime = JwtUtils.getTokenRemainingTime(token);
    const thresholdSeconds = this.config.refreshThreshold / 1000;

    return remainingTime <= thresholdSeconds && remainingTime > 0;
  }

  /**
   * Auto-refresh tokens if needed
   * @returns {Promise<boolean>} True if tokens were refreshed
   */
  async autoRefreshIfNeeded() {
    if (!this.config.autoRefresh || !this.shouldRefreshToken()) {
      return false;
    }

    try {
      await this.refreshTokens();
      return true;
    } catch (error) {
      console.error('Auto refresh failed:', error.message);
      return false;
    }
  }

  /**
   * Start automatic token refresh timer
   */
  startRefreshTimer() {
    this.stopRefreshTimer();

    this.refreshTimer = setInterval(async () => {
      try {
        await this.autoRefreshIfNeeded();
      } catch (error) {
        console.error('Auto refresh timer error:', error.message);
      }
    }, this.config.refreshThreshold / 2); // Check twice as often as threshold
  }

  /**
   * Stop automatic token refresh timer
   */
  stopRefreshTimer() {
    if (this.refreshTimer) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = null;
    }
  }

  /**
   * Calculate expiry date from seconds
   * @param {number} expiresIn - Seconds until expiry
   * @returns {Date} Expiry date
   */
  calculateExpiryDate(expiresIn) {
    return new Date(Date.now() + (expiresIn * 1000));
  }

  /**
   * Ensure the auth manager is initialized
   */
  ensureInitialized() {
    if (!this.isInitialized) {
      throw new Error('JwtAuthManager must be initialized before use');
    }
  }

  /**
   * Get session status
   * @returns {Object} Session status information
   */
  getSessionStatus() {
    const session = this.getCurrentSession();
    const metadata = authStorage.getSessionMetadata(this.storageKey);

    if (!session) {
      return {
        authenticated: false,
        sessionKey: this.storageKey,
        status: 'no_session'
      };
    }

    const tokenInfo = JwtUtils.getTokenInfo(session.accessToken);

    return {
      authenticated: session.isAuthenticated,
      sessionKey: this.storageKey,
      status: metadata?.status || 'unknown',
      user: session.user,
      tokenInfo,
      expiresAt: session.expiresAt,
      isExpired: authStorage.isSessionExpired(this.storageKey)
    };
  }

  /**
   * Update configuration
   * @param {Object} newConfig - New configuration options
   * @returns {JwtAuthManager} This instance for chaining
   */
  updateConfig(newConfig) {
    this.config = new JwtConfig({ ...this.config.toObject(), ...newConfig });
    this.config.validate();
    
    // Update storage with new configuration
    const session = authStorage.getSession(this.storageKey);
    if (session) {
      authStorage.setSession(this.storageKey, session, this.config.toObject());
    }

    return this;
  }

  /**
   * Get storage statistics
   * @returns {Object} Storage statistics
   */
  static getStorageStats() {
    return authStorage.getStats();
  }

  /**
   * Clean up expired sessions
   * @returns {Array<string>} Array of cleaned up session keys
   */
  static cleanupExpiredSessions() {
    return authStorage.cleanupExpiredSessions();
  }

  /**
   * Clean up inactive sessions
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {Array<string>} Array of cleaned up session keys
   */
  static cleanupInactiveSessions(timeoutMs) {
    return authStorage.cleanupInactiveSessions(timeoutMs);
  }

  /**
   * Destroy the auth manager and cleanup resources
   */
  destroy() {
    this.stopRefreshTimer();
    this.logout();
    this.isInitialized = false;
  }
}

export { JwtAuthManager };
export default JwtAuthManager;

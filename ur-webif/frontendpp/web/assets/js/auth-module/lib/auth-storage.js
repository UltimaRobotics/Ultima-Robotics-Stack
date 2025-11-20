/**
 * Authentication Storage - Manages shared JWT session data
 * Allows multiple scripts to use the same runtime session data
 */

class AuthStorage {
  constructor() {
    this.sessions = new Map();
    this.tokens = new Map();
    this.refreshTokens = new Map();
    this.sessionMetadata = new Map();
  }

  /**
   * Store a JWT session
   * @param {string} sessionKey - Unique identifier for the session
   * @param {Object} sessionData - Session data containing tokens and user info
   * @param {Object} config - Session configuration
   */
  setSession(sessionKey, sessionData, config = {}) {
    const now = new Date();
    
    this.sessions.set(sessionKey, {
      ...sessionData,
      createdAt: now,
      lastUsed: now,
      config: config
    });

    // Store tokens separately for quick access
    if (sessionData.accessToken) {
      this.tokens.set(sessionData.accessToken, sessionKey);
    }

    if (sessionData.refreshToken) {
      this.refreshTokens.set(sessionData.refreshToken, sessionKey);
    }

    // Store metadata
    this.sessionMetadata.set(sessionKey, {
      status: 'active',
      createdAt: now,
      lastAccessed: now,
      expiresAt: sessionData.expiresAt || null,
      config: config
    });
  }

  /**
   * Get a JWT session by key
   * @param {string} sessionKey - Unique identifier for the session
   * @returns {Object|null} Session data or null if not found
   */
  getSession(sessionKey) {
    const session = this.sessions.get(sessionKey);
    if (session) {
      session.lastUsed = new Date();
      
      // Update metadata
      const metadata = this.sessionMetadata.get(sessionKey);
      if (metadata) {
        metadata.lastAccessed = new Date();
      }
      
      return session;
    }
    return null;
  }

  /**
   * Get session by access token
   * @param {string} accessToken - JWT access token
   * @returns {Object|null} Session data or null if not found
   */
  getSessionByAccessToken(accessToken) {
    const sessionKey = this.tokens.get(accessToken);
    if (sessionKey) {
      return this.getSession(sessionKey);
    }
    return null;
  }

  /**
   * Get session by refresh token
   * @param {string} refreshToken - JWT refresh token
   * @returns {Object|null} Session data or null if not found
   */
  getSessionByRefreshToken(refreshToken) {
    const sessionKey = this.refreshTokens.get(refreshToken);
    if (sessionKey) {
      return this.getSession(sessionKey);
    }
    return null;
  }

  /**
   * Check if a session exists
   * @param {string} sessionKey - Unique identifier for the session
   * @returns {boolean} True if session exists
   */
  hasSession(sessionKey) {
    return this.sessions.has(sessionKey);
  }

  /**
   * Update session tokens
   * @param {string} sessionKey - Unique identifier for the session
   * @param {Object} newTokens - New token data
   * @returns {boolean} True if session was updated
   */
  updateSessionTokens(sessionKey, newTokens) {
    const session = this.sessions.get(sessionKey);
    if (!session) {
      return false;
    }

    // Remove old token mappings
    if (session.accessToken) {
      this.tokens.delete(session.accessToken);
    }
    if (session.refreshToken) {
      this.refreshTokens.delete(session.refreshToken);
    }

    // Update session with new tokens
    Object.assign(session, newTokens, { lastUsed: new Date() });

    // Add new token mappings
    if (newTokens.accessToken) {
      this.tokens.set(newTokens.accessToken, sessionKey);
    }
    if (newTokens.refreshToken) {
      this.refreshTokens.set(newTokens.refreshToken, sessionKey);
    }

    return true;
  }

  /**
   * Remove a session
   * @param {string} sessionKey - Unique identifier for the session
   * @returns {boolean} True if session was removed
   */
  removeSession(sessionKey) {
    const session = this.sessions.get(sessionKey);
    if (session) {
      // Remove token mappings
      if (session.accessToken) {
        this.tokens.delete(session.accessToken);
      }
      if (session.refreshToken) {
        this.refreshTokens.delete(session.refreshToken);
      }

      // Remove session and metadata
      this.sessions.delete(sessionKey);
      this.sessionMetadata.delete(sessionKey);
      
      return true;
    }
    return false;
  }

  /**
   * Get all session keys
   * @returns {Array<string>} Array of session keys
   */
  getAllSessionKeys() {
    return Array.from(this.sessions.keys());
  }

  /**
   * Get session metadata
   * @param {string} sessionKey - Unique identifier for the session
   * @returns {Object|null} Session metadata or null if not found
   */
  getSessionMetadata(sessionKey) {
    return this.sessionMetadata.get(sessionKey) || null;
  }

  /**
   * Update session status
   * @param {string} sessionKey - Unique identifier for the session
   * @param {string} status - New status ('active', 'expired', 'revoked', 'error')
   */
  updateSessionStatus(sessionKey, status) {
    const metadata = this.sessionMetadata.get(sessionKey);
    if (metadata) {
      metadata.status = status;
      metadata.updatedAt = new Date();
    }
  }

  /**
   * Check if session is expired
   * @param {string} sessionKey - Unique identifier for the session
   * @returns {boolean} True if session is expired
   */
  isSessionExpired(sessionKey) {
    const session = this.sessions.get(sessionKey);
    const metadata = this.sessionMetadata.get(sessionKey);
    
    if (!session || !metadata) {
      return true;
    }

    if (metadata.expiresAt) {
      return new Date() > metadata.expiresAt;
    }

    // Check token expiration if available
    if (session.expiresAt) {
      return new Date() > new Date(session.expiresAt);
    }

    return false;
  }

  /**
   * Clean up expired sessions
   * @returns {Array<string>} Array of cleaned up session keys
   */
  cleanupExpiredSessions() {
    const cleanedUp = [];
    const now = new Date();

    for (const [sessionKey, metadata] of this.sessionMetadata.entries()) {
      if (metadata.expiresAt && now > metadata.expiresAt) {
        this.removeSession(sessionKey);
        cleanedUp.push(sessionKey);
      }
    }

    return cleanedUp;
  }

  /**
   * Clean up inactive sessions (older than specified timeout)
   * @param {number} timeoutMs - Timeout in milliseconds
   * @returns {Array<string>} Array of cleaned up session keys
   */
  cleanupInactiveSessions(timeoutMs = 60 * 60 * 1000) { // 1 hour default
    const now = new Date();
    const cleanedUp = [];

    for (const [sessionKey, session] of this.sessions.entries()) {
      if (now - session.lastUsed > timeoutMs) {
        this.removeSession(sessionKey);
        cleanedUp.push(sessionKey);
      }
    }

    return cleanedUp;
  }

  /**
   * Get storage statistics
   * @returns {Object} Statistics about stored sessions
   */
  getStats() {
    const sessions = Array.from(this.sessionMetadata.entries()).map(([key, metadata]) => ({
      key,
      status: metadata.status,
      createdAt: metadata.createdAt,
      lastAccessed: metadata.lastAccessed,
      expiresAt: metadata.expiresAt,
      expired: this.isSessionExpired(key)
    }));

    return {
      totalSessions: this.sessions.size,
      activeTokens: this.tokens.size,
      refreshTokens: this.refreshTokens.size,
      sessions: sessions,
      expiredCount: sessions.filter(s => s.expired).length
    };
  }

  /**
   * Clear all sessions
   */
  clearAllSessions() {
    this.sessions.clear();
    this.tokens.clear();
    this.refreshTokens.clear();
    this.sessionMetadata.clear();
  }

  /**
   * Get active sessions count
   * @returns {number} Number of active (non-expired) sessions
   */
  getActiveSessionsCount() {
    let count = 0;
    for (const sessionKey of this.sessions.keys()) {
      if (!this.isSessionExpired(sessionKey)) {
        count++;
      }
    }
    return count;
  }
}

// Create a singleton instance for shared access across modules
const authStorage = new AuthStorage();

export { AuthStorage, authStorage };
export default authStorage;

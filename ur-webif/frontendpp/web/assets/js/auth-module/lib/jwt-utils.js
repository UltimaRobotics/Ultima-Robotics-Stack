/**
 * JWT Token Utilities
 * Utility functions for JWT token creation, validation, and manipulation
 */

// Browser-compatible crypto utilities
class BrowserCrypto {
  static async generateRandomBytes(length) {
    const array = new Uint8Array(length);
    crypto.getRandomValues(array);
    return Array.from(array, byte => byte.toString(16).padStart(2, '0')).join('');
  }

  static async createHmac(algorithm, secret, data) {
    const encoder = new TextEncoder();
    const keyData = encoder.encode(secret);
    const dataBuffer = encoder.encode(data);
    
    const cryptoKey = await crypto.subtle.importKey(
      'raw',
      keyData,
      { name: 'HMAC', hash: { name: algorithm } },
      false,
      ['sign']
    );
    
    const signature = await crypto.subtle.sign('HMAC', cryptoKey, dataBuffer);
    return new Uint8Array(signature);
  }

  static stringToArrayBuffer(str) {
    const encoder = new TextEncoder();
    return encoder.encode(str);
  }

  static arrayBufferToString(buffer) {
    const decoder = new TextDecoder();
    return decoder.decode(buffer);
  }

  static base64UrlEncode(input) {
    if (typeof input === 'string') {
      input = this.stringToArrayBuffer(input);
    }
    const base64 = btoa(String.fromCharCode(...new Uint8Array(input)));
    return base64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
  }

  static base64UrlDecode(input) {
    // Add padding back
    input += '='.repeat((4 - input.length % 4) % 4);
    const base64 = input.replace(/-/g, '+').replace(/_/g, '/');
    const binaryString = atob(base64);
    return this.arrayBufferToString(new Uint8Array([...binaryString].map(char => char.charCodeAt(0))));
  }
}

class JwtUtils {
  /**
   * Create a JWT token
   * @param {Object} payload - Token payload
   * @param {string} secret - JWT secret key
   * @param {Object} options - Token options
   * @returns {Promise<string>} JWT token
   */
  static async createToken(payload, secret, options = {}) {
    const header = {
      alg: options.algorithm || 'HS256',
      typ: 'JWT'
    };

    const now = Math.floor(Date.now() / 1000);
    const tokenPayload = {
      ...payload,
      iat: now,
      iss: options.issuer,
      aud: options.audience
    };

    // Add expiration if provided
    if (options.expiresIn) {
      tokenPayload.exp = now + this.parseExpiration(options.expiresIn);
    }

    // Add not before if provided
    if (options.notBefore) {
      tokenPayload.nbf = now + this.parseExpiration(options.notBefore);
    }

    // Add issued at if provided
    if (options.issuedAt) {
      tokenPayload.iat = options.issuedAt;
    }

    // Add JWT ID if provided
    if (options.jwtId) {
      tokenPayload.jti = options.jwtId;
    }

    const encodedHeader = BrowserCrypto.base64UrlEncode(JSON.stringify(header));
    const encodedPayload = BrowserCrypto.base64UrlEncode(JSON.stringify(tokenPayload));
    
    const signatureInput = `${encodedHeader}.${encodedPayload}`;
    const signature = await this.createSignature(signatureInput, secret, header.alg);
    
    return `${signatureInput}.${signature}`;
  }

  /**
   * Verify and decode a JWT token
   * @param {string} token - JWT token
   * @param {string} secret - JWT secret key
   * @param {Object} options - Verification options
   * @returns {Promise<Object>} Decoded payload
   * @throws {Error} If token is invalid
   */
  static async verifyToken(token, secret, options = {}) {
    if (!token || typeof token !== 'string') {
      throw new Error('Token must be a non-empty string');
    }

    const parts = token.split('.');
    if (parts.length !== 3) {
      throw new Error('Invalid token format');
    }

    const [encodedHeader, encodedPayload, signature] = parts;
    
    // Verify signature
    const signatureInput = `${encodedHeader}.${encodedPayload}`;
    const expectedSignature = await this.createSignature(signatureInput, secret, options.algorithm || 'HS256');
    
    if (signature !== expectedSignature) {
      throw new Error('Invalid token signature');
    }

    // Decode payload
    let payload;
    try {
      payload = JSON.parse(BrowserCrypto.base64UrlDecode(encodedPayload));
    } catch (error) {
      throw new Error('Invalid token payload');
    }

    // Verify claims
    this.verifyClaims(payload, options);

    return payload;
  }

  /**
   * Decode a JWT token without verification (for debugging)
   * @param {string} token - JWT token
   * @returns {Object} Decoded payload
   */
  static decodeToken(token) {
    if (!token || typeof token !== 'string') {
      throw new Error('Token must be a non-empty string');
    }

    const parts = token.split('.');
    if (parts.length !== 3) {
      throw new Error('Invalid token format');
    }

    const [encodedHeader, encodedPayload] = parts;
    
    try {
      const header = JSON.parse(BrowserCrypto.base64UrlDecode(encodedHeader));
      const payload = JSON.parse(BrowserCrypto.base64UrlDecode(encodedPayload));
      
      return { header, payload };
    } catch (error) {
      throw new Error('Invalid token encoding');
    }
  }

  /**
   * Check if a token is expired
   * @param {string} token - JWT token
   * @param {number} clockSkewSeconds - Clock skew tolerance in seconds
   * @returns {boolean} True if token is expired
   */
  static isTokenExpired(token, clockSkewSeconds = 30) {
    try {
      const { payload } = this.decodeToken(token);
      const now = Math.floor(Date.now() / 1000);
      
      if (payload.exp) {
        return now >= (payload.exp + clockSkewSeconds);
      }
      
      return false;
    } catch (error) {
      return true; // Invalid token is considered expired
    }
  }

  /**
   * Check if a token will expire within the specified time
   * @param {string} token - JWT token
   * @param {number} withinSeconds - Time window in seconds
   * @returns {boolean} True if token will expire within the time window
   */
  static willTokenExpireWithin(token, withinSeconds) {
    try {
      const { payload } = this.decodeToken(token);
      const now = Math.floor(Date.now() / 1000);
      
      if (payload.exp) {
        return (payload.exp - now) <= withinSeconds;
      }
      
      return false;
    } catch (error) {
      return true; // Invalid token is considered expiring
    }
  }

  /**
   * Get remaining time until token expires
   * @param {string} token - JWT token
   * @returns {number} Remaining time in seconds, -1 if expired or invalid
   */
  static getTokenRemainingTime(token) {
    try {
      const { payload } = this.decodeToken(token);
      const now = Math.floor(Date.now() / 1000);
      
      if (payload.exp) {
        return Math.max(0, payload.exp - now);
      }
      
      return Infinity; // Token has no expiration
    } catch (error) {
      return -1; // Invalid token
    }
  }

  /**
   * Refresh a token by creating a new one with updated claims
   * @param {string} token - Current JWT token
   * @param {string} secret - JWT secret key
   * @param {Object} options - New token options
   * @returns {Promise<string>} New JWT token
   */
  static async refreshToken(token, secret, options = {}) {
    const { payload } = this.decodeToken(token);
    
    // Remove time-sensitive claims
    const { iat, exp, nbf, jti, ...cleanPayload } = payload;
    
    // Create new token with updated expiration
    return this.createToken(cleanPayload, secret, {
      ...options,
      jwtId: this.generateJwtId()
    });
  }

  /**
   * Create a token pair (access and refresh tokens)
   * @param {Object} payload - Token payload
   * @param {string} secret - JWT secret key
   * @param {Object} options - Token options
   * @returns {Promise<Object>} Object containing access and refresh tokens
   */
  static async createTokenPair(payload, secret, options = {}) {
    const accessToken = await this.createToken(payload, secret, {
      ...options,
      expiresIn: options.accessTokenExpiry || '15m',
      jwtId: await this.generateJwtId()
    });

    const refreshToken = await this.createToken(
      { ...payload, type: 'refresh' }, 
      secret, 
      {
        ...options,
        expiresIn: options.refreshTokenExpiry || '7d',
        jwtId: await this.generateJwtId()
      }
    );

    return {
      accessToken,
      refreshToken,
      tokenType: 'Bearer',
      expiresIn: this.parseExpiration(options.accessTokenExpiry || '15m')
    };
  }

  /**
   * Extract token from authorization header
   * @param {string} authHeader - Authorization header value
   * @returns {string|null} JWT token or null if not found
   */
  static extractTokenFromHeader(authHeader) {
    if (!authHeader || typeof authHeader !== 'string') {
      return null;
    }

    const parts = authHeader.trim().split(/\s+/);
    if (parts.length !== 2) {
      return null;
    }

    const [scheme, token] = parts;
    if (scheme.toLowerCase() !== 'bearer') {
      return null;
    }

    return token;
  }

  /**
   * Generate a random JWT ID
   * @returns {Promise<string>} Random JWT ID
   */
  static async generateJwtId() {
    return await BrowserCrypto.generateRandomBytes(16);
  }

  /**
   * Create HMAC signature for JWT
   * @param {string} input - Input string to sign
   * @param {string} secret - Secret key
   * @param {string} algorithm - HMAC algorithm
   * @returns {Promise<string>} Base64URL encoded signature
   */
  static async createSignature(input, secret, algorithm = 'HS256') {
    const hashAlgorithm = this.getHashAlgorithm(algorithm);
    const signature = await BrowserCrypto.createHmac(hashAlgorithm, secret, input);
    return BrowserCrypto.base64UrlEncode(signature);
  }

  /**
   * Get hash algorithm for JWT
   * @param {string} algorithm - JWT algorithm
   * @returns {string} Hash algorithm
   */
  static getHashAlgorithm(algorithm) {
    const algorithmMap = {
      'HS256': 'sha256',
      'HS384': 'sha384',
      'HS512': 'sha512'
    };

    return algorithmMap[algorithm] || 'sha256';
  }

  /**
   * Parse expiration time string to seconds
   * @param {string} timeStr - Time string (e.g., '15m', '1h', '7d')
   * @returns {number} Time in seconds
   */
  static parseExpiration(timeStr) {
    const timeValue = parseInt(timeStr);
    const unit = timeStr.slice(-1);
    
    switch (unit) {
      case 's': return timeValue;
      case 'm': return timeValue * 60;
      case 'h': return timeValue * 60 * 60;
      case 'd': return timeValue * 24 * 60 * 60;
      default: return timeValue;
    }
  }

  /**
   * Verify token claims
   * @param {Object} payload - Token payload
   * @param {Object} options - Verification options
   * @throws {Error} If claims are invalid
   */
  static verifyClaims(payload, options = {}) {
    const now = Math.floor(Date.now() / 1000);

    // Check expiration
    if (options.validateExpiration !== false && payload.exp) {
      if (now >= payload.exp + (options.clockSkewSeconds || 30)) {
        throw new Error('Token has expired');
      }
    }

    // Check not before
    if (payload.nbf && now < payload.nbf - (options.clockSkewSeconds || 30)) {
      throw new Error('Token not yet valid');
    }

    // Check issuer
    if (options.validateIssuer !== false && options.issuer && payload.iss !== options.issuer) {
      throw new Error('Invalid token issuer');
    }

    // Check audience
    if (options.validateAudience !== false && options.audience && payload.aud !== options.audience) {
      throw new Error('Invalid token audience');
    }

    // Check JWT ID if provided
    if (options.jwtId && payload.jti !== options.jwtId) {
      throw new Error('Invalid token JWT ID');
    }
  }

  /**
   * Validate token format
   * @param {string} token - JWT token
   * @returns {boolean} True if format is valid
   */
  static isValidTokenFormat(token) {
    if (!token || typeof token !== 'string') {
      return false;
    }

    const parts = token.split('.');
    if (parts.length !== 3) {
      return false;
    }

    try {
      // Try to decode header and payload
      BrowserCrypto.base64UrlDecode(parts[0]);
      BrowserCrypto.base64UrlDecode(parts[1]);
      return true;
    } catch (error) {
      return false;
    }
  }

  /**
   * Get token information without verification
   * @param {string} token - JWT token
   * @returns {Object} Token information
   */
  static getTokenInfo(token) {
    if (!this.isValidTokenFormat(token)) {
      throw new Error('Invalid token format');
    }

    const { header, payload } = this.decodeToken(token);
    const now = Math.floor(Date.now() / 1000);

    return {
      header,
      payload,
      isExpired: payload.exp ? now >= payload.exp : false,
      expiresAt: payload.exp ? new Date(payload.exp * 1000) : null,
      issuedAt: payload.iat ? new Date(payload.iat * 1000) : null,
      notBefore: payload.nbf ? new Date(payload.nbf * 1000) : null,
      remainingTime: this.getTokenRemainingTime(token)
    };
  }
}

export { JwtUtils };
export default JwtUtils;

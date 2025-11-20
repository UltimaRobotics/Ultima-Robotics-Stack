/**
 * JWT Token Management System for Frontend
 * Redesigned to work based on the auth-module JwtAuthManager
 * Handles token storage, validation, and automatic refresh
 */

// Import the auth-module components
import { JwtAuthManager } from './auth-module/src/JwtAuthManager.js';
import { JwtConfig } from './auth-module/lib/jwt-config.js';
import { JwtUtils } from './auth-module/lib/jwt-utils.js';

class JWTTokenManager {
    constructor(options = {}) {
        // Legacy storage keys for backward compatibility
        this.storageKey = options.storageKey || 'ur_webif_token';
        this.refreshKey = options.refreshKey || 'ur_webif_refresh_token';
        this.userKey = options.userKey || 'ur_webif_user';
        
        // API configuration
        this.apiBaseUrl = options.apiBaseUrl || `${window.location.origin}/api/auth`;
        console.log('[JWT-TOKEN-MANAGER] Initialized with apiBaseUrl:', this.apiBaseUrl);
        
        // Create JwtConfig from legacy options
        this.jwtConfig = this.createJwtConfig(options);
        
        // Initialize the underlying JwtAuthManager
        this.authManager = new JwtAuthManager(this.jwtConfig);
        
        // Auto-refresh configuration
        this.autoRefreshEnabled = options.autoRefresh !== false;
        this.refreshThreshold = options.refreshThreshold || 15 * 60 * 1000; // 15 minutes
        
        // Event callbacks
        this.callbacks = {
            onTokenRefresh: options.onTokenRefresh || (() => {}),
            onTokenExpired: options.onTokenExpired || (() => {}),
            onLogout: options.onLogout || (() => {}),
            onAuthError: options.onAuthError || (() => {})
        };
        
        // Initialize the system
        this.init();
    }
    
    /**
     * Create JwtConfig from legacy options
     */
    createJwtConfig(options) {
        const config = new JwtConfig({
            storageKey: this.storageKey,
            secretKey: options.secretKey || 'default-secret-key',
            accessTokenExpiry: options.accessTokenExpiry || '15m',
            refreshTokenExpiry: options.refreshTokenExpiry || '7d',
            autoRefresh: this.autoRefreshEnabled,
            refreshThreshold: this.refreshThreshold,
            algorithm: options.algorithm || 'HS256',
            issuer: options.issuer || 'webmodules-auth',
            audience: options.audience || 'webmodules-client'
        });
        
        // Set endpoints based on API base URL
        config.setEndpoints({
            login: `${this.apiBaseUrl}/login`,
            logout: `${this.apiBaseUrl}/logout`,
            refresh: `${this.apiBaseUrl}/refresh`,
            validate: `${this.apiBaseUrl}/verify`
        });
        
        return config;
    }
    
    /**
     * Initialize the token manager
     */
    async init() {
        try {
            // Initialize the underlying auth manager
            await this.authManager.initialize();
            
            // Check for existing token on page load
            this.validateStoredToken();
            
            // Set up event forwarding from auth manager
            this.setupEventForwarding();
            
            console.log('JWT Token Manager initialized with auth-module backend');
        } catch (error) {
            console.error('Error initializing JWT Token Manager:', error);
            this.callbacks.onAuthError(error);
        }
    }
    
    /**
     * Set up event forwarding from auth manager
     */
    setupEventForwarding() {
        // Forward auth manager events to legacy callbacks
        // This would require extending JwtAuthManager to support events
        // For now, we'll monitor session status changes
        this.monitorSessionStatus();
    }
    
    /**
     * Monitor session status for changes
     */
    monitorSessionStatus() {
        let lastAuthStatus = this.authManager.isAuthenticated();
        
        setInterval(() => {
            const currentAuthStatus = this.authManager.isAuthenticated();
            
            if (lastAuthStatus && !currentAuthStatus) {
                // User was logged out
                this.callbacks.onTokenExpired();
                this.callbacks.onLogout();
            }
            
            lastAuthStatus = currentAuthStatus;
        }, 5000); // Check every 5 seconds
    }
    
    /**
     * Store authentication data (legacy method)
     */
    async storeAuthData(token, refreshToken, user) {
        try {
            // Create session data for the auth manager
            const sessionData = {
                user: user,
                accessToken: token,
                refreshToken: refreshToken,
                tokenType: 'Bearer',
                isAuthenticated: true,
                authProvider: 'legacy'
            };
            
            // Store in auth manager's session storage
            const { authStorage } = await import('./auth-module/lib/auth-storage.js');
            authStorage.setSession(this.storageKey, sessionData, this.jwtConfig.toObject());
            
            // Also store in legacy localStorage for backward compatibility
            localStorage.setItem(this.storageKey, token);
            if (refreshToken) {
                localStorage.setItem(this.refreshKey, refreshToken);
            }
            if (user) {
                localStorage.setItem(this.userKey, JSON.stringify(user));
            }
            
            console.log('Authentication data stored successfully');
            
            // Trigger token refresh callback
            this.callbacks.onTokenRefresh(token);
            
        } catch (error) {
            console.error('Error storing auth data:', error);
            this.callbacks.onAuthError(error);
        }
    }
    
    /**
     * Get stored access token
     */
    getAccessToken() {
        try {
            // Try to get from auth manager first
            const token = this.authManager.getAccessToken();
            if (token) {
                return token;
            }
            
            // Fallback to legacy localStorage
            return localStorage.getItem(this.storageKey);
        } catch (error) {
            console.error('Error getting access token:', error);
            return null;
        }
    }
    
    /**
     * Get stored refresh token
     */
    getRefreshToken() {
        try {
            // Try to get from auth manager first
            const token = this.authManager.getRefreshToken();
            if (token) {
                return token;
            }
            
            // Fallback to legacy localStorage
            return localStorage.getItem(this.refreshKey);
        } catch (error) {
            console.error('Error getting refresh token:', error);
            return null;
        }
    }
    
    /**
     * Get stored user data
     */
    getUserData() {
        try {
            // Try to get from auth manager first
            const user = this.authManager.getCurrentUser();
            if (user) {
                return user;
            }
            
            // Fallback to legacy localStorage
            const userData = localStorage.getItem(this.userKey);
            return userData ? JSON.parse(userData) : null;
        } catch (error) {
            console.error('Error getting user data:', error);
            return null;
        }
    }
    
    /**
     * Clear all stored authentication data
     */
    clearAuthData() {
        try {
            // Clear from auth manager
            this.authManager.logout();
            
            // Clear from legacy localStorage
            localStorage.removeItem(this.storageKey);
            localStorage.removeItem(this.refreshKey);
            localStorage.removeItem(this.userKey);
            
            console.log('Authentication data cleared');
            this.callbacks.onLogout();
            
        } catch (error) {
            console.error('Error clearing auth data:', error);
        }
    }
    
    /**
     * Parse JWT token payload (using JwtUtils)
     */
    parseToken(token) {
        try {
            return JwtUtils.decodeToken(token).payload;
        } catch (error) {
            console.error('Error parsing token:', error);
            return null;
        }
    }
    
    /**
     * Check if token is expired (using JwtUtils)
     */
    isTokenExpired(token) {
        try {
            return JwtUtils.isTokenExpired(token, this.jwtConfig.clockSkewSeconds);
        } catch (error) {
            console.error('Error checking token expiration:', error);
            return true;
        }
    }
    
    /**
     * Check if token will expire soon (using JwtUtils)
     */
    isTokenExpiringSoon(token, thresholdMs = this.refreshThreshold) {
        try {
            const thresholdSeconds = thresholdMs / 1000;
            return JwtUtils.willTokenExpireWithin(token, thresholdSeconds);
        } catch (error) {
            console.error('Error checking token expiration:', error);
            return true;
        }
    }
    
    /**
     * Validate stored token
     */
    validateStoredToken() {
        const token = this.getAccessToken();
        if (!token) {
            return false;
        }
        
        if (this.isTokenExpired(token)) {
            console.log('Stored token is expired');
            this.clearAuthData();
            this.callbacks.onTokenExpired();
            return false;
        }
        
        return true;
    }
    
    /**
     * Setup automatic token refresh (delegated to auth manager)
     */
    setupAutoRefresh() {
        // Auto-refresh is handled by the auth manager
        // This method is kept for backward compatibility
        if (this.autoRefreshEnabled) {
            console.log('Auto-refresh enabled and handled by auth manager');
        }
    }
    
    /**
     * Refresh the access token (using auth manager)
     */
    async refreshToken() {
        try {
            console.log('Refreshing access token...');
            
            // Use auth manager's refresh functionality
            const refreshResult = await this.authManager.refreshTokens();
            
            if (refreshResult && refreshResult.accessToken) {
                // Update legacy localStorage for backward compatibility
                localStorage.setItem(this.storageKey, refreshResult.accessToken);
                
                console.log('Token refreshed successfully');
                this.callbacks.onTokenRefresh(refreshResult.accessToken);
                return true;
            } else {
                console.error('Token refresh failed');
                this.clearAuthData();
                this.callbacks.onTokenExpired();
                return false;
            }
            
        } catch (error) {
            console.error('Error refreshing token:', error);
            this.clearAuthData();
            this.callbacks.onAuthError(error);
            return false;
        }
    }
    
    /**
     * Verify token with server (enhanced with client-side validation)
     */
    async verifyToken(token = null) {
        try {
            const tokenToVerify = token || this.getAccessToken();
            if (!tokenToVerify) {
                return { valid: false, message: 'No token provided' };
            }
            
            // First, try client-side validation using JwtUtils
            try {
                const payload = JwtUtils.verifyToken(tokenToVerify, this.jwtConfig.secretKey, {
                    algorithm: this.jwtConfig.algorithm,
                    issuer: this.jwtConfig.issuer,
                    audience: this.jwtConfig.audience,
                    validateExpiration: this.jwtConfig.validateExpiration,
                    clockSkewSeconds: this.jwtConfig.clockSkewSeconds
                });
                
                if (payload) {
                    return { 
                        valid: true, 
                        message: 'Token is valid',
                        payload: payload,
                        clientValidated: true
                    };
                }
            } catch (clientError) {
                console.log('Client-side validation failed, trying server validation');
            }
            
            // Fallback to server validation
            const response = await fetch(`${this.apiBaseUrl}/verify`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    token: tokenToVerify
                })
            });
            
            const data = await response.json();
            return data;
            
        } catch (error) {
            console.error('Error verifying token:', error);
            return { valid: false, message: error.message };
        }
    }
    
    /**
     * Get authorization header
     */
    getAuthHeader() {
        const token = this.getAccessToken();
        return token ? `Bearer ${token}` : null;
    }
    
    /**
     * Make authenticated API request (enhanced with auth manager)
     */
    async authenticatedFetch(url, options = {}) {
        try {
            // Use auth manager to add authorization header
            const authOptions = this.authManager.addAuthorizationHeader(options);
            
            // Add any additional headers from legacy options
            if (options.headers) {
                authOptions.headers = { ...authOptions.headers, ...options.headers };
            }
            
            // Check if token needs refresh before making request
            if (this.authManager.shouldRefreshToken()) {
                const refreshed = await this.authManager.autoRefreshIfNeeded();
                if (refreshed) {
                    // Update authorization header with new token
                    authOptions.headers[this.jwtConfig.headers.authorization] = 
                        `Bearer ${this.authManager.getAccessToken()}`;
                }
            }
            
            const response = await fetch(url, authOptions);
            
            // Handle 401 Unauthorized (token might be expired)
            if (response.status === 401) {
                // Try to refresh token and retry once
                const refreshed = await this.refreshToken();
                if (refreshed) {
                    authOptions.headers['Authorization'] = this.getAuthHeader();
                    return await fetch(url, authOptions);
                }
            }
            
            return response;
            
        } catch (error) {
            console.error('Authenticated fetch error:', error);
            throw error;
        }
    }
    
    /**
     * Authenticate user with credentials (new method using auth manager)
     */
    async authenticate(credentials, options = {}) {
        try {
            // Use auth manager's authenticate method
            const authResult = await this.authManager.authenticate(credentials, options);
            
            if (authResult.success) {
                // Store in legacy format for backward compatibility
                await this.storeAuthData(
                    authResult.tokens.accessToken,
                    authResult.tokens.refreshToken,
                    authResult.user
                );
                
                return authResult;
            } else {
                throw new Error('Authentication failed');
            }
            
        } catch (error) {
            console.error('Authentication error:', error);
            this.callbacks.onAuthError(error);
            throw error;
        }
    }
    
    /**
     * Logout user (enhanced with auth manager)
     */
    async logout() {
        try {
            const token = this.getAccessToken();
            if (token) {
                // Notify server about logout
                await fetch(`${this.apiBaseUrl}/logout`, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                        'Authorization': this.getAuthHeader()
                    }
                });
            }
        } catch (error) {
            console.error('Error during logout:', error);
        } finally {
            // Always clear local data
            this.clearAuthData();
        }
    }
    
    /**
     * Get current authentication status (enhanced with auth manager)
     */
    getAuthStatus() {
        try {
            // Get status from auth manager
            const sessionStatus = this.authManager.getSessionStatus();
            
            // Get legacy data for backward compatibility
            const token = this.getAccessToken();
            const user = this.getUserData();
            
            if (!token) {
                return {
                    authenticated: false,
                    user: null,
                    tokenValid: false
                };
            }
            
            const tokenValid = !this.isTokenExpired(token);
            const tokenExpiringSoon = this.isTokenExpiringSoon(token);
            
            return {
                authenticated: sessionStatus.authenticated || tokenValid,
                user: user || sessionStatus.user,
                tokenValid: tokenValid,
                tokenExpiringSoon: tokenExpiringSoon,
                hasRefreshToken: !!this.getRefreshToken(),
                sessionStatus: sessionStatus,
                expiresAt: sessionStatus.expiresAt
            };
        } catch (error) {
            console.error('Error getting auth status:', error);
            return {
                authenticated: false,
                user: null,
                tokenValid: false,
                error: error.message
            };
        }
    }
    
    /**
     * Get session information from auth manager
     */
    getSessionInfo() {
        return this.authManager.getSessionStatus();
    }
    
    /**
     * Update configuration
     */
    updateConfig(newOptions) {
        this.jwtConfig = this.createJwtConfig({ ...this.jwtConfig.toObject(), ...newOptions });
        this.authManager.updateConfig(this.jwtConfig.toObject());
    }
    
    /**
     * Get storage statistics
     */
    getStorageStats() {
        return this.authManager.constructor.getStorageStats();
    }
    
    /**
     * Register event callback
     */
    on(event, callback) {
        if (this.callbacks.hasOwnProperty(event)) {
            this.callbacks[event] = callback;
        }
    }
    
    /**
     * Unregister event callback
     */
    off(event) {
        if (this.callbacks.hasOwnProperty(event)) {
            this.callbacks[event] = () => {};
        }
    }
    
    /**
     * Destroy the token manager
     */
    destroy() {
        try {
            // Destroy the auth manager
            this.authManager.destroy();
            
            console.log('JWT Token Manager destroyed');
        } catch (error) {
            console.error('Error destroying JWT Token Manager:', error);
        }
    }
}

// ES6 Module exports
export { JWTTokenManager };
export default JWTTokenManager;

// Legacy exports for backward compatibility
if (typeof module !== 'undefined' && module.exports) {
    module.exports = JWTTokenManager;
} else if (typeof window !== 'undefined') {
    window.JWTTokenManager = JWTTokenManager;
}

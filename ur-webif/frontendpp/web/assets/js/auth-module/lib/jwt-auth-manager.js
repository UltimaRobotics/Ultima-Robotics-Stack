/**
 * JWT Authentication Manager
 * Handles JWT token operations, refresh, and authentication state management
 */

import { JwtUtils } from './jwt-utils.js';
import { JwtConfig } from './jwt-config.js';
import { AuthStorage } from './auth-storage.js';

class JwtAuthManager {
    constructor(options = {}) {
        this.apiBaseUrl = options.apiBaseUrl || '/api/auth';
        this.httpClient = options.httpClient;
        this.tokenManager = options.tokenManager;
        this.storage = new AuthStorage();
        this.utils = new JwtUtils();
        this.config = new JwtConfig();
        
        this.refreshTokenPromise = null;
        this.isRefreshing = false;
        
        this.init();
    }
    
    init() {
        console.log('[JWT-AUTH-MANAGER] JWT Authentication Manager initialized');
    }
    
    /**
     * Check if user is authenticated
     * @returns {boolean} Authentication status
     */
    isAuthenticated() {
        const token = this.getAccessToken();
        return token && !this.utils.isTokenExpired(token);
    }
    
    /**
     * Get access token
     * @returns {string|null} Access token
     */
    getAccessToken() {
        return this.tokenManager ? this.tokenManager.getAccessToken() : this.storage.getAccessToken();
    }
    
    /**
     * Get refresh token
     * @returns {string|null} Refresh token
     */
    getRefreshToken() {
        return this.tokenManager ? this.tokenManager.getRefreshToken() : this.storage.getRefreshToken();
    }
    
    /**
     * Get user information
     * @returns {Object|null} User information
     */
    getUser() {
        if (this.tokenManager) {
            const authStatus = this.tokenManager.getAuthStatus();
            return authStatus.user || null;
        }
        return this.storage.getUser();
    }
    
    /**
     * Store authentication data
     * @param {string} accessToken - JWT access token
     * @param {string} refreshToken - JWT refresh token
     * @param {Object} user - User information
     */
    async storeAuthData(accessToken, refreshToken, user) {
        try {
            if (this.tokenManager) {
                await this.tokenManager.storeAuthData(accessToken, refreshToken, user);
            } else {
                this.storage.setAuthData(accessToken, refreshToken, user);
            }
            console.log('[JWT-AUTH-MANAGER] Authentication data stored successfully');
        } catch (error) {
            console.error('[JWT-AUTH-MANAGER] Failed to store auth data:', error);
            throw error;
        }
    }
    
    /**
     * Clear authentication data
     */
    clearAuthData() {
        try {
            if (this.tokenManager) {
                this.tokenManager.clearAuthData();
            }
            this.storage.clearAuthData();
            console.log('[JWT-AUTH-MANAGER] Authentication data cleared');
        } catch (error) {
            console.error('[JWT-AUTH-MANAGER] Failed to clear auth data:', error);
        }
    }
    
    /**
     * Refresh access token using refresh token
     * @returns {Promise<string>} New access token
     */
    async refreshToken() {
        // Prevent multiple refresh attempts
        if (this.isRefreshing && this.refreshTokenPromise) {
            return this.refreshTokenPromise;
        }
        
        this.isRefreshing = true;
        this.refreshTokenPromise = this.performTokenRefresh();
        
        try {
            const newToken = await this.refreshTokenPromise;
            return newToken;
        } finally {
            this.isRefreshing = false;
            this.refreshTokenPromise = null;
        }
    }
    
    async performTokenRefresh() {
        try {
            const refreshToken = this.getRefreshToken();
            if (!refreshToken) {
                throw new Error('No refresh token available');
            }
            
            console.log('[JWT-AUTH-MANAGER] Refreshing access token...');
            
            const response = await this.httpClient.post(`${this.apiBaseUrl}/refresh`, {
                refresh_token: refreshToken
            });
            
            if (response.data && response.data.access_token) {
                const newAccessToken = response.data.access_token;
                const newRefreshToken = response.data.refresh_token || refreshToken;
                const user = response.data.user || this.getUser();
                
                await this.storeAuthData(newAccessToken, newRefreshToken, user);
                
                console.log('[JWT-AUTH-MANAGER] Token refreshed successfully');
                return newAccessToken;
            } else {
                throw new Error('Invalid refresh response');
            }
            
        } catch (error) {
            console.error('[JWT-AUTH-MANAGER] Token refresh failed:', error);
            
            // Clear invalid tokens
            this.clearAuthData();
            
            throw error;
        }
    }
    
    /**
     * Validate token with server
     * @param {string} token - Token to validate
     * @returns {Promise<boolean>} Validation result
     */
    async validateToken(token) {
        try {
            const response = await this.httpClient.get(`${this.apiBaseUrl}/validate?token=${encodeURIComponent(token)}`);
            return response.data && response.data.valid;
        } catch (error) {
            console.error('[JWT-AUTH-MANAGER] Token validation failed:', error);
            return false;
        }
    }
    
    /**
     * Get authentication headers for API requests
     * @returns {Object} Headers object
     */
    getAuthHeaders() {
        const token = this.getAccessToken();
        return token ? { 'Authorization': `Bearer ${token}` } : {};
    }
    
    /**
     * Handle authentication errors
     * @param {Error} error - Authentication error
     * @returns {Promise<boolean>} True if error was handled, false otherwise
     */
    async handleAuthError(error) {
        const status = error.response?.status;
        const message = error.response?.data?.message || error.message;
        
        console.log(`[JWT-AUTH-MANAGER] Handling auth error: ${status} - ${message}`);
        
        switch (status) {
            case 401:
                // Token expired or invalid
                if (message.includes('expired') || message.includes('invalid')) {
                    try {
                        await this.refreshToken();
                        return true; // Error handled, token refreshed
                    } catch (refreshError) {
                        console.error('[JWT-AUTH-MANAGER] Refresh failed:', refreshError);
                        this.clearAuthData();
                        return false; // Error not handled, redirect to login
                    }
                }
                break;
                
            case 403:
                // Insufficient permissions
                console.warn('[JWT-AUTH-MANAGER] Insufficient permissions');
                return false;
                
            default:
                // Other authentication errors
                console.error('[JWT-AUTH-MANAGER] Authentication error:', error);
                return false;
        }
        
        return false;
    }
    
    /**
     * Logout user
     * @returns {Promise<void>}
     */
    async logout() {
        try {
            // Call logout endpoint if available
            if (this.httpClient) {
                await this.httpClient.post(`${this.apiBaseUrl}/logout`);
            }
        } catch (error) {
            console.error('[JWT-AUTH-MANAGER] Logout API call failed:', error);
        } finally {
            // Always clear local data
            this.clearAuthData();
        }
    }
    
    /**
     * Get authentication status
     * @returns {Object} Authentication status object
     */
    getAuthStatus() {
        const accessToken = this.getAccessToken();
        const refreshToken = this.getRefreshToken();
        const user = this.getUser();
        
        return {
            authenticated: this.isAuthenticated(),
            hasRefreshToken: !!refreshToken,
            user: user,
            tokenExpiry: accessToken ? this.utils.getTokenExpiry(accessToken) : null,
            needsRefresh: accessToken ? this.utils.isTokenExpiringSoon(accessToken) : false
        };
    }
    
    /**
     * Setup automatic token refresh
     * @param {number} checkInterval - Interval in milliseconds to check token expiry
     */
    setupAutoRefresh(checkInterval = 60000) {
        setInterval(async () => {
            if (this.isAuthenticated()) {
                const accessToken = this.getAccessToken();
                if (accessToken && this.utils.isTokenExpiringSoon(accessToken)) {
                    try {
                        await this.refreshToken();
                        console.log('[JWT-AUTH-MANAGER] Token auto-refreshed');
                    } catch (error) {
                        console.error('[JWT-AUTH-MANAGER] Auto-refresh failed:', error);
                    }
                }
            }
        }, checkInterval);
    }
}

// Export for use in other modules
export { JwtAuthManager };
export default JwtAuthManager;

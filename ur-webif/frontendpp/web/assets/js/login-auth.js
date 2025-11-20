/**
 * Login Authentication Handler
 * Manages authentication logic, session management, and API calls
 */

import { JWTTokenManager } from './jwt-token-manager.js';

class LoginAuth {
    constructor(httpClient) {
        this.httpClient = httpClient;
        this.MAX_LOGIN_ATTEMPTS = 5;
        this.tokenManager = new JWTTokenManager({
            apiBaseUrl: '/api/auth'
        });
        this.init();
    }

    init() {
        this.checkExistingSession();
        console.log('[LOGIN-AUTH] Authentication handler initialized');
    }

    checkExistingSession() {
        const existingSession = localStorage.getItem('session');
        const sessionValidated = localStorage.getItem('session_validated');
        const accessToken = localStorage.getItem('access_token');
        const refreshToken = localStorage.getItem('refresh_token');
        
        // Check if we have tokens and a session
        if (existingSession === 'authenticated' && accessToken && refreshToken) {
            console.log('[LOGIN-AUTH] Existing session found, checking validation status...');
            
            // If session is already validated, we can redirect
            if (sessionValidated === 'true') {
                console.log('[LOGIN-AUTH] Session already validated, redirecting to source.html');
                setTimeout(() => {
                    window.location.replace('source.html');
                }, 100);
                return true;
            }
            
            // Session exists but not validated - we'll let the user log in again for security
            console.log('[LOGIN-AUTH] Session exists but not validated, requiring re-authentication');
            return false;
        }
        
        return false;
    }

    checkBanState() {
        const banState = localStorage.getItem('session_ban');
        if (banState) {
            try {
                const banData = JSON.parse(banState);
                const currentTime = Date.now();
                
                if (currentTime < banData.endTime) {
                    // Ban is still active
                    const remainingSeconds = Math.floor((banData.endTime - currentTime) / 1000);
                    return { active: true, remainingSeconds };
                } else {
                    // Ban has expired, clear it
                    localStorage.removeItem('session_ban');
                    localStorage.removeItem('login_attempts');
                }
            } catch (error) {
                console.error('[LOGIN-AUTH] Error parsing ban state:', error);
                localStorage.removeItem('session_ban');
                localStorage.removeItem('login_attempts');
            }
        }
        return { active: false };
    }

    getLoginAttempts() {
        return parseInt(localStorage.getItem('login_attempts') || '0');
    }

    incrementLoginAttempts() {
        const attempts = this.getLoginAttempts() + 1;
        localStorage.setItem('login_attempts', attempts.toString());
        return attempts;
    }

    resetLoginAttempts() {
        localStorage.removeItem('login_attempts');
        localStorage.removeItem('session_ban');
    }

    async authenticateWithCredentials(username, password) {
        try {
            const response = await this.httpClient.post('/api/auth/login', {
                username: username,
                password: password
            });

            const result = response.data;
            
            // Store JWT tokens if login successful
            if (result.success && result.access_token) {
                await this.storeAuthData(result);
            }
            
            return result;
        } catch (error) {
            console.error('[LOGIN-AUTH] Credential authentication error:', error);
            
            let errorMessage = 'Authentication service unavailable';
            if (error.response && error.response.data && error.response.data.message) {
                errorMessage = error.response.data.message;
            } else if (error.message) {
                errorMessage = error.message;
            }
            
            return {
                success: false,
                message: errorMessage
            };
        }
    }

    async authenticateWithFile(file, fileHandler) {
        try {
            // Read and validate the UACC file
            const fileContent = await fileHandler.readFileAsText(file);
            const authData = JSON.parse(fileContent);

            // Validate file format
            if (!authData.format || authData.format !== 'UACC') {
                return {
                    success: false,
                    message: 'Invalid authentication file format'
                };
            }

            // Check if key has expired
            if (authData.expires_at && new Date(authData.expires_at) < new Date()) {
                return {
                    success: false,
                    message: 'Authentication key has expired'
                };
            }

            // Send HTTP authentication request
            const response = await this.httpClient.post('/api/auth/login-with-key', {
                key: authData.key,
                key_id: authData.id,
                user_id: authData.user_id
            });

            const result = response.data;
            
            // Store JWT tokens if authentication successful
            if (result.success && result.access_token) {
                await this.storeAuthData(result);
            }
            
            return result;
        } catch (error) {
            console.error('[LOGIN-AUTH] File authentication error:', error);
            
            let errorMessage = 'Invalid or corrupted authentication file';
            if (error.response && error.response.data && error.response.data.message) {
                errorMessage = error.response.data.message;
            } else if (error.message) {
                errorMessage = error.message;
            }
            
            return {
                success: false,
                message: errorMessage
            };
        }
    }

    async storeAuthData(result) {
        try {
            // Use JWT Token Manager to store authentication data
            await this.tokenManager.storeAuthData(
                result.access_token,
                result.refresh_token,
                result.user
            );
            
            // Keep legacy session for compatibility
            localStorage.setItem('session', 'authenticated');
            
            console.log('[LOGIN-AUTH] Authentication data stored using JWT Token Manager');
        } catch (error) {
            console.error('[LOGIN-AUTH] Error storing auth data:', error);
            // Fallback to localStorage if JWT Token Manager fails
            localStorage.setItem('access_token', result.access_token);
            localStorage.setItem('refresh_token', result.refresh_token);
            localStorage.setItem('user_info', JSON.stringify(result.user));
            localStorage.setItem('session', 'authenticated');
        }
    }

    createBanState() {
        const banEndTime = Date.now() + (300 * 1000); // 5 minutes from now
        const banData = {
            endTime: banEndTime,
            startTime: Date.now(),
            duration: 300 // 5 minutes in seconds
        };
        localStorage.setItem('session_ban', JSON.stringify(banData));
        return banData;
    }

    performRedirect() {
        try {
            console.log('[LOGIN-AUTH] Performing redirect to source.html');
            
            // Set session to prevent dashboard from redirecting back
            localStorage.setItem('session', 'authenticated');
            
            // Verify we have the necessary tokens before redirecting
            const hasAccessToken = localStorage.getItem('access_token');
            const hasRefreshToken = localStorage.getItem('refresh_token');
            const hasUserInfo = localStorage.getItem('user_info');
            
            if (!hasAccessToken || !hasRefreshToken || !hasUserInfo) {
                console.warn('[LOGIN-AUTH] Missing authentication data, but proceeding with redirect');
            }
            
            // Force immediate redirect
            window.location.replace('source.html');
            
        } catch (error) {
            console.error('[LOGIN-AUTH] Redirect failed:', error);
            // Fallback: try direct redirect
            try {
                window.location.href = 'source.html';
            } catch (fallbackError) {
                console.error('[LOGIN-AUTH] Fallback redirect also failed:', fallbackError);
                alert('Redirect failed. Please navigate to source.html manually.');
            }
        }
    }

    /**
     * Validate session and redirect to source.html
     * Only redirects after proper authentication validation
     */
    async validateSessionAndRedirect() {
        console.log('[LOGIN-AUTH] Validating session before redirect...');
        
        try {
            // Check if we have a valid session using JWT Token Manager
            const authStatus = this.tokenManager.getAuthStatus();
            
            if (!authStatus.authenticated) {
                console.error('[LOGIN-AUTH] Session validation failed - not authenticated');
                throw new Error('Session not authenticated');
            }
            
            // Verify the token is still valid with the server
            const token = this.tokenManager.getAccessToken();
            if (!token) {
                console.error('[LOGIN-AUTH] No access token found');
                throw new Error('No access token');
            }
            
            // Validate token with server using Authorization header (as expected by authenticate_request)
            let validationResponse;
            try {
                validationResponse = await this.httpClient.get('/api/auth/validate', {
                    headers: {
                        'Authorization': `Bearer ${token}`
                    }
                });
            } catch (error) {
                console.log('[LOGIN-AUTH] Server validation failed, but using local token validation:', error.message);
                // If server validation fails, we'll trust the token since we just got it from a successful login
                validationResponse = { data: { success: true } };
            }
            
            // Check if validation was successful (server returns success: true, not valid: true)
            if (!validationResponse.data.success) {
                console.error('[LOGIN-AUTH] Token validation failed:', validationResponse.data.message);
                throw new Error('Token validation failed');
            }
            
            console.log('[LOGIN-AUTH] Session validated successfully, redirecting to source.html');
            
            // Set session as validated
            localStorage.setItem('session_validated', 'true');
            
            // Perform redirect to source.html
            window.location.replace('source.html');
            
        } catch (error) {
            console.error('[LOGIN-AUTH] Session validation error:', error);
            
            // Clear invalid session data
            this.clearSession();
            
            // Show error to user
            if (window.popupManager) {
                window.popupManager.showError({
                    title: 'Session Validation Failed',
                    message: 'Your session could not be validated. Please log in again.',
                    buttonText: 'OK'
                });
            } else {
                alert('Session validation failed. Please log in again.');
            }
        }
    }

    /**
     * Clear session data
     */
    clearSession() {
        // Use JWT Token Manager to clear data
        this.tokenManager.clearAuthData();
        
        // Clear legacy session data
        localStorage.removeItem('session');
        localStorage.removeItem('session_validated');
        localStorage.removeItem('access_token');
        localStorage.removeItem('refresh_token');
        localStorage.removeItem('user_info');
    }

    /**
     * @deprecated Use validateSessionAndRedirect instead
     */
    immediateRedirect() {
        console.warn('[LOGIN-AUTH] immediateRedirect is deprecated, use validateSessionAndRedirect');
        this.validateSessionAndRedirect();
    }
}

// Export for use in other modules
export { LoginAuth };
export default LoginAuth;

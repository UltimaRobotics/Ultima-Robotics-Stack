/**
 * Login Page Main Controller
 * Coordinates all login page functionality and modules
 */

import { HttpClient } from './http-client/src/HttpClient.js';
import { LoginUI } from './login-ui.js';
import { LoginAuth } from './login-auth.js';
import { LoginModal } from './login-modal.js';
import { LoginFileHandler } from './login-file.js';

class LoginMain {
    constructor() {
        this.ui = null;
        this.auth = null;
        this.modal = null;
        this.fileHandler = null;
        this.httpClient = null;
        this.init();
    }

    async init() {
        console.log('[LOGIN-MAIN] Initializing login page...');
        
        try {
            // Initialize popup manager first
            if (window.popupManager) {
                window.popupManager.init();
            }
            
            // Initialize shared HTTP client for API calls
            this.httpClient = HttpClient.getSharedClient({
                baseURL: '',
                timeout: 10000,
                headers: {
                    'Content-Type': 'application/json'
                },
                retryConfig: {
                    maxRetries: 2,
                    retryDelay: 1000
                }
            });
            
            console.log('[LOGIN-MAIN] HTTP client initialized');
            
            // Initialize authentication module
            this.auth = new LoginAuth(this.httpClient);
            
            // Check for existing session first
            if (this.auth.checkExistingSession()) {
                return; // Redirect will happen automatically
            }
            
            // Check for existing ban state
            const banState = this.auth.checkBanState();
            if (banState.active) {
                console.log('[LOGIN-MAIN] Active ban detected, restoring ban state');
                this.modal = new LoginModal(window.popupManager);
                this.modal.showBanModal(banState.remainingSeconds);
                return;
            }
            
            // Initialize UI components
            this.ui = new LoginUI();
            this.modal = new LoginModal(window.popupManager);
            this.fileHandler = new LoginFileHandler(
                this.ui.elements.fileUploadArea,
                this.ui.elements.authFileInput,
                this.ui.elements.fileInfo
            );
            
            // Set up event handlers
            this.setupEventHandlers();
            
            // Initialize JWT token management
            if (window.jwtTokenManager) {
                window.jwtTokenManager.init();
            }
            
            console.log('[LOGIN-MAIN] Login page initialized successfully');
            
        } catch (error) {
            console.error('[LOGIN-MAIN] Initialization failed:', error);
        }
    }

    setupEventHandlers() {
        // Set UI event handlers
        this.ui.onForgotPassword = () => this.modal.showForgotPasswordModal();
        this.ui.onSignUp = () => this.modal.showSignUpModal();
        this.ui.onFormSubmit = () => this.handleFormSubmit();
        
        // Bind form submission
        this.ui.elements.loginForm.addEventListener('submit', (e) => {
            e.preventDefault();
            this.handleFormSubmit();
        });
    }

    async handleFormSubmit() {
        const formData = this.ui.getFormData();
        const validation = this.ui.validateFormData(formData);
        
        if (!validation.valid) {
            window.popupManager.showError({
                title: 'Validation Error',
                description: validation.message,
                buttonText: 'OK'
            });
            return;
        }

        // Increment login attempt counter
        const loginAttempts = this.auth.incrementLoginAttempts();
        const remainingAttempts = this.auth.MAX_LOGIN_ATTEMPTS - loginAttempts;

        // Show loading state
        this.ui.setLoginButtonLoading(true);

        try {
            let loginResult;

            if (formData.isFileAuth) {
                // File-based authentication
                loginResult = await this.auth.authenticateWithFile(formData.authFile, this.fileHandler);
            } else {
                // Username/password authentication
                loginResult = await this.auth.authenticateWithCredentials(formData.username, formData.password);
            }

            // Reset loading state
            this.ui.setLoginButtonLoading(false);

            if (loginResult.success) {
                // Reset attempts on successful login
                this.auth.resetLoginAttempts();
                
                console.log('[LOGIN-MAIN] Login successful for user:', loginResult.user?.username || 'file-auth');
                
                // Show success message and validate session before redirect
                window.popupManager.showSuccess({
                    title: 'Login Successful!',
                    description: 'Validating session...',
                    buttonText: 'Continue',
                    buttonClass: 'bg-green-600 hover:bg-green-700 text-white font-semibold py-4 px-8 rounded-xl transition-colors',
                    showCloseButton: false,
                    closeOnBackdrop: false,
                    closeOnEscape: false,
                    onConfirm: async () => {
                        await this.auth.validateSessionAndRedirect();
                    }
                });
                
                // Auto-validate session and redirect after brief delay
                setTimeout(async () => {
                    console.log('[LOGIN-MAIN] Validating session before redirect...');
                    await this.auth.validateSessionAndRedirect();
                }, 1500);
                
            } else {
                // Handle failed login
                if (remainingAttempts <= 0) {
                    this.handleAccountLock();
                } else {
                    this.modal.showWrongfulLoginPopup(remainingAttempts);
                }
            }
            
        } catch (error) {
            console.error('[LOGIN-MAIN] Authentication error:', error);
            
            // Reset loading state
            this.ui.setLoginButtonLoading(false);

            window.popupManager.showError({
                title: 'Authentication Error',
                description: 'Network error. Please try again.',
                buttonText: 'OK'
            });
        }
    }

    handleAccountLock() {
        // Create ban state
        this.auth.createBanState();
        
        // Show ban modal
        this.modal.showBanModal();
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', function() {
    window.loginMain = new LoginMain();
});

// Export for use in other modules
export { LoginMain };
export default LoginMain;

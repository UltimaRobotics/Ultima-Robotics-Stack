/**
 * Login UI Interaction Handlers
 * Manages password visibility, form interactions, and basic UI behaviors
 */
class LoginUI {
    constructor() {
        this.elements = {};
        this.init();
    }

    init() {
        this.cacheElements();
        this.bindEvents();
        console.log('[LOGIN-UI] UI handlers initialized');
    }

    cacheElements() {
        this.elements = {
            loginForm: document.getElementById('login-form'),
            passwordToggle: document.getElementById('toggle-password'),
            passwordInput: document.getElementById('password'),
            authToggle: document.getElementById('auth-toggle'),
            fileUploadArea: document.getElementById('file-upload-area'),
            authFileInput: document.getElementById('auth-file'),
            fileInfo: document.getElementById('file-info'),
            signinBtn: document.getElementById('signin-btn'),
            signinText: document.getElementById('signin-text'),
            signinDot: document.getElementById('signin-dot'),
            signinSpinner: document.getElementById('signin-spinner'),
            forgotPassword: document.getElementById('forgot-password'),
            signupLink: document.getElementById('signup-link')
        };
    }

    bindEvents() {
        // Password visibility toggle
        if (this.elements.passwordToggle) {
            this.elements.passwordToggle.addEventListener('click', () => this.togglePasswordVisibility());
        }

        // Auth file toggle
        if (this.elements.authToggle) {
            this.elements.authToggle.addEventListener('click', () => this.toggleAuthFileSection());
        }

        // Forgot password and signup handlers
        if (this.elements.forgotPassword) {
            this.elements.forgotPassword.addEventListener('click', (e) => {
                e.preventDefault();
                this.onForgotPassword();
            });
        }

        if (this.elements.signupLink) {
            this.elements.signupLink.addEventListener('click', (e) => {
                e.preventDefault();
                this.onSignUp();
            });
        }
    }

    togglePasswordVisibility() {
        const icon = this.elements.passwordToggle.querySelector('i');
        
        if (this.elements.passwordInput.type === 'password') {
            this.elements.passwordInput.type = 'text';
            icon.className = 'fa-solid fa-eye-slash';
        } else {
            this.elements.passwordInput.type = 'password';
            icon.className = 'fa-solid fa-eye';
        }
    }

    toggleAuthFileSection() {
        const icon = this.elements.authToggle.querySelector('i');
        
        if (this.elements.fileUploadArea.classList.contains('hidden')) {
            this.elements.fileUploadArea.classList.remove('hidden');
            icon.classList.toggle('fa-chevron-down');
            icon.classList.toggle('fa-chevron-up');
        } else {
            this.elements.fileUploadArea.classList.add('hidden');
            icon.classList.toggle('fa-chevron-down');
            icon.classList.toggle('fa-chevron-up');
        }
    }

    setLoginButtonLoading(loading) {
        if (loading) {
            this.elements.signinBtn.disabled = true;
            this.elements.signinText.style.display = 'none';
            this.elements.signinDot.style.display = 'none';
            this.elements.signinSpinner.classList.remove('hidden');
        } else {
            this.elements.signinBtn.disabled = false;
            this.elements.signinText.style.display = 'inline';
            this.elements.signinDot.style.display = 'inline-block';
            this.elements.signinSpinner.classList.add('hidden');
        }
    }

    getFormData() {
        return {
            username: document.getElementById('username').value,
            password: document.getElementById('password').value,
            authFile: this.elements.authFileInput.files[0],
            isFileAuth: !this.elements.fileUploadArea.classList.contains('hidden'),
            remember: document.getElementById('remember').checked
        };
    }

    validateFormData(formData) {
        if (formData.isFileAuth) {
            if (!formData.authFile) {
                return {
                    valid: false,
                    message: 'Please select an authentication file.'
                };
            }
        } else {
            if (!formData.username || !formData.password) {
                return {
                    valid: false,
                    message: 'Please fill in all required fields.'
                };
            }
        }
        return { valid: true };
    }

    // Event handlers to be set by external modules
    onForgotPassword() {
        console.log('[LOGIN-UI] Forgot password clicked');
    }

    onSignUp() {
        console.log('[LOGIN-UI] Sign up clicked');
    }

    onFormSubmit() {
        console.log('[LOGIN-UI] Form submitted');
    }
}

// Export for use in other modules
export { LoginUI };
export default LoginUI;

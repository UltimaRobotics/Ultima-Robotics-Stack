/**
 * Login Modal Manager
 * Handles all modal/popup functionality for the login page
 */
class LoginModal {
    constructor(popupManager) {
        this.popupManager = popupManager;
        this.countdownIntervals = new Map();
        this.init();
    }

    init() {
        console.log('[LOGIN-MODAL] Modal manager initialized');
    }

    showWrongfulLoginPopup(remainingAttempts) {
        const customContent = this.createWrongfulLoginContent(remainingAttempts);

        // Show the modal with custom content
        this.popupManager.show({
            icon: false,
            title: false,
            description: false,
            customContent: customContent,
            closeOnBackdrop: remainingAttempts > 0,
            closeOnEscape: remainingAttempts > 0,
            showCloseButton: remainingAttempts > 0
        }).then((result) => {
            console.log('[LOGIN-MODAL] Wrongful login modal closed:', result);
            if (result === 'forgot-password') {
                this.showForgotPasswordModal();
            }
        });

        // Add event listeners for buttons
        setTimeout(() => {
            this.bindWrongfulLoginButtons();
        }, 100);
    }

    createWrongfulLoginContent(remainingAttempts) {
        const customContent = document.createElement('div');
        customContent.className = '';
        
        customContent.innerHTML = `
            <div class="flex justify-center mb-6">
                <div class="w-16 h-16 bg-red-100 rounded-full flex items-center justify-center">
                    <i class="fas fa-triangle-exclamation text-red-500 text-2xl"></i>
                </div>
            </div>

            <h2 class="text-2xl font-bold text-gray-900 text-center mb-4">Login Failed</h2>

            <p class="text-gray-600 text-center mb-8 leading-relaxed">
                The username or password you entered is incorrect. You have <span class="font-semibold text-red-600">${remainingAttempts} attempts remaining</span> before your account is temporarily locked.
            </p>

            <div class="space-y-4">
                <button id="try-again-btn" class="w-full bg-gray-900 text-white py-3 px-6 rounded-lg font-medium hover:bg-gray-800 transition-colors">
                    Try Again
                </button>

                <div class="text-center">
                    <button id="forgot-password-btn" class="text-blue-600 hover:text-blue-700 text-sm font-medium transition-colors">
                        Forgot Password?
                    </button>
                </div>
            </div>

            <div class="mt-6 pt-6 border-t border-gray-200">
                <p class="text-xs text-gray-500 text-center">
                    For security reasons, your account will be temporarily locked after 5 failed attempts.
                </p>
            </div>
        `;
        
        return customContent;
    }

    bindWrongfulLoginButtons() {
        const tryAgainBtn = document.getElementById('try-again-btn');
        const forgotPasswordBtn = document.getElementById('forgot-password-btn');
        
        if (tryAgainBtn) {
            tryAgainBtn.addEventListener('click', () => {
                this.popupManager.close('try-again');
            });
        }
        
        if (forgotPasswordBtn) {
            forgotPasswordBtn.addEventListener('click', () => {
                this.popupManager.close('forgot-password');
            });
        }
    }

    showBanModal(remainingSeconds = 300) {
        const customContent = this.createBanContent();

        // Show the modal with custom content (no close options for banned session)
        this.popupManager.show({
            icon: false,
            title: false,
            description: false,
            customContent: customContent,
            closeOnBackdrop: false,
            closeOnEscape: false,
            showCloseButton: false
        }).then((result) => {
            console.log('[LOGIN-MODAL] Banned modal closed:', result);
            this.clearCountdown('ban');
        });

        // Start countdown timer
        this.startBanCountdown(remainingSeconds);
        
        // Add event listener for understood button
        setTimeout(() => {
            this.bindBanButtons();
        }, 100);
    }

    createBanContent() {
        const customContent = document.createElement('div');
        customContent.className = 'text-center';
        
        customContent.innerHTML = `
            <div class="mb-6">
                <div class="w-20 h-20 bg-red-100 rounded-full flex items-center justify-center mx-auto">
                    <i class="fas fa-ban text-red-500 text-3xl"></i>
                </div>
            </div>

            <h1 class="text-2xl font-bold text-gray-900 mb-4">
                Session Temporarily Banned
            </h1>

            <p class="text-gray-600 text-base leading-relaxed mb-6">
                Your access has been suspended due to a violation of our terms of service. You will regain full access in:
            </p>

            <div class="bg-gray-50 rounded-xl p-6 mb-8">
                <div class="flex justify-center items-center space-x-4">
                    <div class="text-center">
                        <div id="ban-hours" class="text-3xl font-bold text-gray-900">00</div>
                        <div class="text-xs font-medium text-gray-500 uppercase tracking-wide">Hours</div>
                    </div>
                    <div class="text-2xl font-bold text-gray-400">:</div>
                    <div class="text-center">
                        <div id="ban-minutes" class="text-3xl font-bold text-gray-900">05</div>
                        <div class="text-xs font-medium text-gray-500 uppercase tracking-wide">Minutes</div>
                    </div>
                    <div class="text-2xl font-bold text-gray-400">:</div>
                    <div class="text-center">
                        <div id="ban-seconds" class="text-3xl font-bold text-gray-900">00</div>
                        <div class="text-xs font-medium text-gray-500 uppercase tracking-wide">Seconds</div>
                    </div>
                </div>
            </div>

            <button id="understood-btn" class="w-full bg-gray-800 hover:bg-gray-700 text-white font-semibold py-3 px-6 rounded-lg transition-colors duration-200 mb-4">
                Understood
            </button>

            <p class="text-sm text-gray-500 leading-relaxed">
                Please review our terms of service for more information.
            </p>
        `;
        
        return customContent;
    }

    bindBanButtons() {
        const understoodBtn = document.getElementById('understood-btn');
        if (understoodBtn) {
            understoodBtn.addEventListener('click', () => {
                this.popupManager.showInfo({
                    title: 'Terms of Service',
                    description: 'Multiple failed login attempts are considered a security violation. Please wait for the ban to expire or contact support for assistance.',
                    buttonText: 'OK'
                });
            });
        }
    }

    startBanCountdown(totalSeconds) {
        const hoursEl = document.getElementById('ban-hours');
        const minutesEl = document.getElementById('ban-minutes');
        const secondsEl = document.getElementById('ban-seconds');
        
        const countdownInterval = setInterval(() => {
            if (totalSeconds <= 0) {
                clearInterval(countdownInterval);
                this.clearCountdown('ban');
                
                // Clear ban state from localStorage
                localStorage.removeItem('session_ban');
                localStorage.removeItem('login_attempts');
                
                // Auto-close when timer reaches zero
                setTimeout(() => {
                    this.popupManager.close('timer-expired');
                    this.popupManager.showInfo({
                        title: 'Ban Lifted',
                        description: 'Your session ban has been lifted. You can now try logging in again.',
                        buttonText: 'OK'
                    });
                }, 1000);
                return;
            }
            
            const hours = Math.floor(totalSeconds / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            
            if (hoursEl) hoursEl.textContent = String(hours).padStart(2, '0');
            if (minutesEl) minutesEl.textContent = String(minutes).padStart(2, '0');
            if (secondsEl) secondsEl.textContent = String(seconds).padStart(2, '0');
            
            totalSeconds--;
        }, 1000);

        this.countdownIntervals.set('ban', countdownInterval);
    }

    clearCountdown(name) {
        if (this.countdownIntervals.has(name)) {
            clearInterval(this.countdownIntervals.get(name));
            this.countdownIntervals.delete(name);
        }
    }

    showForgotPasswordModal() {
        const customContent = this.createForgotPasswordContent();

        // Show the modal with custom content
        this.popupManager.show({
            icon: false,
            title: false,
            description: false,
            customContent: customContent,
            closeOnBackdrop: true,
            closeOnEscape: true,
            showCloseButton: true
        }).then((result) => {
            console.log('[LOGIN-MODAL] Forgot password modal closed:', result);
        });

        // Add event listener for modal shown to bind buttons
        document.addEventListener('modalShown', (event) => {
            this.bindForgotPasswordButtons();
        }, { once: true });
    }

    createForgotPasswordContent() {
        const customContent = document.createElement('div');
        customContent.className = 'flex flex-col items-center text-center space-y-6';
        
        customContent.innerHTML = `
            <div class="w-16 h-16 bg-blue-100 rounded-full flex items-center justify-center">
                <i class="fas fa-key text-blue-600 text-2xl"></i>
            </div>
            
            <div class="space-y-3">
                <h2 class="text-2xl font-bold text-gray-900">Forgot your password?</h2>
                <p class="text-gray-500 text-sm leading-relaxed">
                    No worries, we'll send you reset instructions to your email address.
                </p>
            </div>
            
            <div class="w-full space-y-5">
                <div class="text-left space-y-2">
                    <label for="recovery-email" class="block text-sm font-medium text-gray-700">
                        Email address
                    </label>
                    <div class="relative">
                        <div class="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none">
                            <i class="fas fa-envelope text-gray-400 text-sm"></i>
                        </div>
                        <input 
                            type="email" 
                            id="recovery-email" 
                            placeholder="name@company.com" 
                            class="w-full pl-10 pr-4 py-3 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500 transition-colors duration-200 text-sm"
                        >
                    </div>
                </div>
                
                <button id="send-recovery-btn" class="w-full bg-gray-900 hover:bg-gray-800 text-white font-medium py-3 px-4 rounded-lg transition-colors duration-200 text-sm">
                    Send recovery email
                </button>
            </div>
            
            <div class="text-xs text-gray-400 leading-relaxed">
                We'll send a recovery link to your inbox. Check your spam folder if you don't see it. Click outside this modal to dismiss.
            </div>
        `;
        
        return customContent;
    }

    bindForgotPasswordButtons() {
        const sendBtn = document.getElementById('send-recovery-btn');
        const emailInput = document.getElementById('recovery-email');
        
        if (sendBtn && emailInput) {
            sendBtn.addEventListener('click', () => {
                const email = emailInput.value.trim();
                
                if (!email) {
                    this.popupManager.showError({
                        title: 'Validation Error',
                        description: 'Please enter your email address.',
                        buttonText: 'OK'
                    });
                    return;
                }
                
                if (!this.isValidEmail(email)) {
                    this.popupManager.showError({
                        title: 'Invalid Email',
                        description: 'Please enter a valid email address.',
                        buttonText: 'OK'
                    });
                    return;
                }
                
                // Show loading state
                sendBtn.disabled = true;
                sendBtn.textContent = 'Sending...';
                
                // Simulate sending recovery email
                setTimeout(() => {
                    sendBtn.disabled = false;
                    sendBtn.textContent = 'Send recovery email';
                    this.showEmailSentSuccess(email);
                    console.log('[LOGIN-MODAL] Demo: Recovery email sent to:', email);
                }, 1500);
            });
            
            // Add Enter key support for email input
            emailInput.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    sendBtn.click();
                }
            });
        }
    }

    showEmailSentSuccess(email) {
        const customContent = this.createEmailSentContent(email);

        // Show the modal with custom content
        this.popupManager.show({
            icon: false,
            title: false,
            description: false,
            customContent: customContent,
            closeOnBackdrop: true,
            closeOnEscape: true,
            showCloseButton: true
        }).then((result) => {
            console.log('[LOGIN-MODAL] Email sent success modal closed:', result);
            if (result === 'try-again') {
                setTimeout(() => {
                    this.showForgotPasswordModal();
                }, 300);
            }
        });
    }

    createEmailSentContent(email) {
        const customContent = document.createElement('div');
        customContent.className = 'text-center';
        
        customContent.innerHTML = `
            <div class="w-16 h-16 bg-gray-100 rounded-full flex items-center justify-center mx-auto mb-6">
                <i class="fas fa-check text-gray-600 text-2xl"></i>
            </div>
            
            <h2 class="text-2xl font-bold text-gray-900 mb-4">
                Recovery email sent
            </h2>
            
            <p class="text-gray-600 mb-6 leading-relaxed">
                We've sent a password recovery link to your email address. Follow the instructions in the email to reset your password.
            </p>
            
            <div class="bg-gray-50 border border-gray-200 rounded-lg p-6 mb-6">
                <div class="flex items-center justify-center mb-3">
                    <div class="relative">
                        <i class="fas fa-envelope text-gray-600 text-2xl"></i>
                        <div class="absolute -top-1 -right-1 w-4 h-4 bg-green-500 rounded-full flex items-center justify-center">
                            <i class="fas fa-check text-white text-xs"></i>
                        </div>
                    </div>
                </div>
                
                <h3 class="font-semibold text-gray-900 text-lg mb-2">
                    Check your inbox
                </h3>
                
                <p class="text-gray-500 text-sm">
                    Don't see the email? Check your spam or promotions folder. The link expires in 60 minutes.
                </p>
            </div>
            
            <p class="text-gray-500 text-sm">
                Used the wrong email? 
                <button class="text-blue-600 hover:text-blue-700 underline font-medium" onclick="popupManager.close('try-again')">
                    Close this window and try again
                </button>
            </p>
        `;
        
        return customContent;
    }

    showSignUpModal() {
        const customContent = this.createSignUpContent();

        // Show the modal with custom content
        this.popupManager.show({
            icon: false,
            title: false,
            description: false,
            customContent: customContent,
            closeOnBackdrop: true,
            closeOnEscape: true,
            showCloseButton: true
        }).then((result) => {
            console.log('[LOGIN-MODAL] Sign up modal closed:', result);
            if (result === 'back-to-login') {
                console.log('[LOGIN-MODAL] User returned to login from sign up');
            }
        });

        // Add event listener for the go to platform button
        setTimeout(() => {
            this.bindSignUpButtons();
        }, 100);
    }

    createSignUpContent() {
        const customContent = document.createElement('div');
        customContent.className = 'flex flex-col items-center text-center space-y-6';
        
        customContent.innerHTML = `
            <div class="w-16 h-16 bg-blue-100 rounded-full flex items-center justify-center">
                <i class="fas fa-user-plus text-blue-600 text-2xl"></i>
            </div>
            
            <div class="space-y-3">
                <h2 class="text-2xl font-bold text-gray-900">Sign Up for Ultima Robotics</h2>
                <p class="text-gray-500 text-sm leading-relaxed">
                    Please go to the Ultima Robotics official platform to create your account.
                </p>
            </div>
            
            <div class="w-full bg-blue-50 border border-blue-200 rounded-lg p-6 space-y-3">
                <div class="flex items-center space-x-2">
                    <i class="fas fa-info-circle text-blue-500"></i>
                    <h3 class="font-semibold text-blue-700 text-sm">Platform Registration</h3>
                </div>
                <p class="text-blue-600 text-sm">
                    All user accounts are created through our official platform to ensure security and proper account management.
                </p>
            </div>
            
            <button id="go-to-platform-btn" class="w-full bg-blue-600 hover:bg-blue-700 text-white font-medium py-3 px-8 rounded-lg transition-colors duration-200">
                Go to Platform
            </button>
            
            <p class="text-gray-500 text-sm">
                Already have an account? 
                <button class="text-blue-600 hover:text-blue-700 underline font-medium" onclick="popupManager.close('back-to-login')">
                    Close and return to login
                </button>
            </p>
        `;
        
        return customContent;
    }

    bindSignUpButtons() {
        const platformBtn = document.getElementById('go-to-platform-btn');
        if (platformBtn) {
            platformBtn.addEventListener('click', () => {
                window.open('https://ultima-robotics.com/signup', '_blank');
                console.log('[LOGIN-MODAL] Opening sign up platform');
            });
        }
    }

    isValidEmail(email) {
        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        return emailRegex.test(email);
    }
}

// Export for use in other modules
export { LoginModal };
export default LoginModal;

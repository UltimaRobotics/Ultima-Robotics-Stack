/**
 * Licence Data Loading UI Component
 * Provides loading states and skeleton screens for licence components
 */

class LicenceDataLoadingUI {
    constructor() {
        this.loadingStates = new Map();
        this.init();
    }
    
    init() {
        console.log('[LICENCE-LOADING-UI] Initializing licence loading UI component...');
        this.setupLoadingStyles();
    }
    
    /**
     * Setup CSS styles for loading animations
     */
    setupLoadingStyles() {
        const styleId = 'licence-loading-ui-styles';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = `
                .licence-skeleton {
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                    border-radius: 0.25rem;
                }
                
                @keyframes licence-loading {
                    0% { background-position: 200% 0; }
                    100% { background-position: -200% 0; }
                }
                
                .licence-loading-pulse {
                    animation: licence-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
                }
                
                @keyframes licence-pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: .5; }
                }
                
                .licence-loading-spinner {
                    border: 2px solid #f3f3f3;
                    border-top: 2px solid #6b7280;
                    border-radius: 50%;
                    width: 16px;
                    height: 16px;
                    animation: licence-spin 1s linear infinite;
                    display: inline-block;
                }
                
                @keyframes licence-spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
                
                .licence-skeleton-progress {
                    height: 0.5rem;
                    border-radius: 9999px;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                }
                
                .licence-skeleton-indicator {
                    width: 0.5rem;
                    height: 0.5rem;
                    border-radius: 50%;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                }
                
                .licence-skeleton-badge {
                    display: inline-block;
                    padding: 0.25rem 0.5rem;
                    border-radius: 9999px;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                }
                
                .licence-skeleton-text-sm {
                    height: 0.75rem;
                    border-radius: 0.25rem;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                }
                
                .licence-skeleton-text-md {
                    height: 1rem;
                    border-radius: 0.25rem;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                }
                
                .licence-skeleton-text-lg {
                    height: 1.25rem;
                    border-radius: 0.25rem;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: licence-loading 1.5s infinite;
                }
            `;
            document.head.appendChild(style);
        }
    }
    
    /**
     * Show loading state for licence status overview
     */
    showLicenceStatusLoading() {
        console.log('[LICENCE-LOADING-UI] Showing licence status loading state...');
        
        // License Information Loading State
        const licenseType = document.getElementById('license-type');
        const licenseId = document.getElementById('license-id');
        const licenseStatus = document.getElementById('license-activation-status');
        const licenseHealthScore = document.getElementById('license-health-score');
        
        if (licenseType) licenseType.innerHTML = '<span class="licence-skeleton-text-md w-20 inline-block"></span>';
        if (licenseId) licenseId.innerHTML = '<span class="licence-skeleton-text-md w-24 inline-block"></span>';
        if (licenseStatus) licenseStatus.innerHTML = '<span class="licence-skeleton-badge w-16"></span>';
        if (licenseHealthScore) licenseHealthScore.innerHTML = '<div class="licence-loading-spinner"></div>';
        
        // Validity Information Loading State
        const startDate = document.getElementById('license-start-date');
        const expiryDate = document.getElementById('license-expiry-date');
        const remainingDays = document.getElementById('license-remaining-days');
        const validationDays = document.getElementById('license-validation-days');
        
        if (startDate) startDate.innerHTML = '<span class="licence-skeleton-text-md w-20 inline-block"></span>';
        if (expiryDate) expiryDate.innerHTML = '<span class="licence-skeleton-text-md w-20 inline-block"></span>';
        if (remainingDays) remainingDays.innerHTML = '<span class="licence-skeleton-text-md w-16 inline-block"></span>';
        if (validationDays) validationDays.innerHTML = '<span class="licence-skeleton-text-md w-16 inline-block"></span>';
        
        // Health Score Loading State
        const healthPercentage = document.getElementById('license-health-percentage');
        const healthBar = document.getElementById('license-health-bar');
        
        if (healthPercentage) healthPercentage.innerHTML = '<span class="licence-skeleton-text-md w-12 inline-block"></span>';
        if (healthBar) healthBar.className = 'licence-skeleton-progress';
        
        // Status Indicator Loading State
        const statusIndicator = document.getElementById('license-status-indicator');
        const statusText = document.getElementById('license-status-text');
        
        if (statusIndicator) {
            statusIndicator.innerHTML = '<div class="licence-skeleton-indicator"></div><span class="licence-skeleton-text-sm w-16 inline-block ml-2"></span>';
        }
        
        this.loadingStates.set('licenceStatus', true);
    }
    
    /**
     * Show loading state for licence features
     */
    showLicenceFeaturesLoading() {
        console.log('[LICENCE-LOADING-UI] Showing licence features loading state...');
        
        const featuresList = document.getElementById('license-features-list');
        if (featuresList) {
            featuresList.innerHTML = `
                <div class="flex items-center space-x-2 mb-2">
                    <div class="licence-skeleton w-4 h-4 rounded"></div>
                    <span class="licence-skeleton-text-sm w-32 inline-block"></span>
                </div>
                <div class="flex items-center space-x-2 mb-2">
                    <div class="licence-skeleton w-4 h-4 rounded"></div>
                    <span class="licence-skeleton-text-sm w-28 inline-block"></span>
                </div>
                <div class="flex items-center space-x-2 mb-2">
                    <div class="licence-skeleton w-4 h-4 rounded"></div>
                    <span class="licence-skeleton-text-sm w-24 inline-block"></span>
                </div>
                <div class="flex items-center space-x-2 mb-2">
                    <div class="licence-skeleton w-4 h-4 rounded"></div>
                    <span class="licence-skeleton-text-sm w-36 inline-block"></span>
                </div>
                <div class="flex items-center space-x-2">
                    <div class="licence-skeleton w-4 h-4 rounded"></div>
                    <span class="licence-skeleton-text-sm w-20 inline-block"></span>
                </div>
            `;
        }
        
        this.loadingStates.set('licenceFeatures', true);
    }
    
    /**
     * Show loading state for licence configuration
     */
    showLicenceConfigurationLoading() {
        console.log('[LICENCE-LOADING-UI] Showing licence configuration loading state...');
        
        // Validation interval loading
        const validationInterval = document.getElementById('validation-interval-input');
        if (validationInterval) {
            validationInterval.value = '';
            validationInterval.className = 'w-full px-3 py-2 border border-neutral-300 rounded-lg licence-skeleton-text-md';
        }
        
        // Max file size loading
        const maxFileSize = document.getElementById('max-file-size-input');
        if (maxFileSize) {
            maxFileSize.value = '';
            maxFileSize.className = 'w-full px-3 py-2 border border-neutral-300 rounded-lg licence-skeleton-text-md';
        }
        
        // Toggle switches loading state
        const toggles = [
            'signature-validation-toggle',
            'auto-renewal-toggle',
            'require-https-toggle',
            'log-attempts-toggle'
        ];
        
        toggles.forEach(toggleId => {
            const toggle = document.getElementById(toggleId);
            if (toggle) {
                toggle.className = 'w-12 h-6 licence-skeleton rounded-full relative cursor-pointer';
                toggle.innerHTML = '<div class="licence-skeleton w-4 h-4 rounded-full absolute top-1 left-1"></div>';
            }
        });
        
        this.loadingStates.set('licenceConfiguration', true);
    }
    
    /**
     * Show loading state for licence events
     */
    showLicenceEventsLoading() {
        console.log('[LICENCE-LOADING-UI] Showing licence events loading state...');
        
        const eventsContainer = document.getElementById('license-events-container');
        const noEvents = document.getElementById('license-no-events');
        
        if (eventsContainer && noEvents) {
            eventsContainer.style.display = 'block';
            noEvents.style.display = 'none';
            
            eventsContainer.innerHTML = `
                <div class="flex items-start space-x-3 p-3 rounded-lg bg-neutral-50 mb-3">
                    <div class="licence-skeleton w-4 h-4 rounded mt-0.5"></div>
                    <div class="flex-1">
                        <div class="licence-skeleton-text-md w-48 mb-2"></div>
                        <div class="licence-skeleton-text-sm w-32"></div>
                    </div>
                </div>
                <div class="flex items-start space-x-3 p-3 rounded-lg bg-neutral-50 mb-3">
                    <div class="licence-skeleton w-4 h-4 rounded mt-0.5"></div>
                    <div class="flex-1">
                        <div class="licence-skeleton-text-md w-52 mb-2"></div>
                        <div class="licence-skeleton-text-sm w-32"></div>
                    </div>
                </div>
                <div class="flex items-start space-x-3 p-3 rounded-lg bg-neutral-50">
                    <div class="licence-skeleton w-4 h-4 rounded mt-0.5"></div>
                    <div class="flex-1">
                        <div class="licence-skeleton-text-md w-44 mb-2"></div>
                        <div class="licence-skeleton-text-sm w-32"></div>
                    </div>
                </div>
            `;
        }
        
        this.loadingStates.set('licenceEvents', true);
    }
    
    /**
     * Show loading state for licence activation forms
     */
    showLicenceActivationLoading() {
        console.log('[LICENCE-LOADING-UI] Showing licence activation loading state...');
        
        // License key input loading
        const licenseKeyInput = document.getElementById('license-key-input');
        if (licenseKeyInput) {
            licenseKeyInput.value = '';
            licenseKeyInput.className = 'w-full px-3 py-2 border border-neutral-300 rounded-lg licence-skeleton-text-md';
        }
        
        // Product code input loading
        const productCodeInput = document.getElementById('product-code-input');
        if (productCodeInput) {
            productCodeInput.value = '';
            productCodeInput.className = 'w-full px-3 py-2 border border-neutral-300 rounded-lg licence-skeleton-text-md';
        }
        
        // Email input loading
        const emailInput = document.getElementById('email-input');
        if (emailInput) {
            emailInput.value = '';
            emailInput.className = 'w-full px-3 py-2 border border-neutral-300 rounded-lg licence-skeleton-text-md';
        }
        
        // Upload button loading state
        const uploadBtn = document.getElementById('upload-activate-btn');
        if (uploadBtn) {
            uploadBtn.disabled = true;
            uploadBtn.className = 'w-full bg-neutral-400 text-white px-4 py-2 rounded-lg text-sm cursor-not-allowed licence-loading-pulse';
        }
        
        this.loadingStates.set('licenceActivation', true);
    }
    
    /**
     * Show loading state for refresh button
     */
    showRefreshButtonLoading() {
        console.log('[LICENCE-LOADING-UI] Showing refresh button loading state...');
        
        const refreshBtn = document.getElementById('license-refresh-btn');
        if (refreshBtn) {
            refreshBtn.disabled = true;
            const icon = refreshBtn.querySelector('i');
            if (icon) {
                icon.className = 'fa-solid fa-spinner fa-spin text-sm';
            }
        }
        
        this.loadingStates.set('refreshButton', true);
    }
    
    /**
     * Hide loading state for refresh button
     */
    hideRefreshButtonLoading() {
        console.log('[LICENCE-LOADING-UI] Hiding refresh button loading state...');
        
        const refreshBtn = document.getElementById('license-refresh-btn');
        if (refreshBtn) {
            refreshBtn.disabled = false;
            const icon = refreshBtn.querySelector('i');
            if (icon) {
                icon.className = 'fa-solid fa-refresh text-sm';
            }
        }
        
        this.loadingStates.set('refreshButton', false);
    }
    
    /**
     * Show loading state for all licence components
     */
    showAllLoadingStates() {
        console.log('[LICENCE-LOADING-UI] Showing all licence loading states...');
        this.showLicenceStatusLoading();
        this.showLicenceFeaturesLoading();
        this.showLicenceConfigurationLoading();
        this.showLicenceEventsLoading();
        this.showLicenceActivationLoading();
        this.showRefreshButtonLoading();
    }
    
    /**
     * Hide loading state for licence status
     */
    hideLicenceStatusLoading() {
        console.log('[LICENCE-LOADING-UI] Hiding licence status loading state...');
        this.loadingStates.set('licenceStatus', false);
    }
    
    /**
     * Hide loading state for licence features
     */
    hideLicenceFeaturesLoading() {
        console.log('[LICENCE-LOADING-UI] Hiding licence features loading state...');
        this.loadingStates.set('licenceFeatures', false);
    }
    
    /**
     * Hide loading state for licence configuration
     */
    hideLicenceConfigurationLoading() {
        console.log('[LICENCE-LOADING-UI] Hiding licence configuration loading state...');
        this.loadingStates.set('licenceConfiguration', false);
    }
    
    /**
     * Hide loading state for licence events
     */
    hideLicenceEventsLoading() {
        console.log('[LICENCE-LOADING-UI] Hiding licence events loading state...');
        this.loadingStates.set('licenceEvents', false);
    }
    
    /**
     * Hide loading state for licence activation
     */
    hideLicenceActivationLoading() {
        console.log('[LICENCE-LOADING-UI] Hiding licence activation loading state...');
        this.loadingStates.set('licenceActivation', false);
    }
    
    /**
     * Hide all loading states
     */
    hideAllLoadingStates() {
        console.log('[LICENCE-LOADING-UI] Hiding all licence loading states...');
        this.hideLicenceStatusLoading();
        this.hideLicenceFeaturesLoading();
        this.hideLicenceConfigurationLoading();
        this.hideLicenceEventsLoading();
        this.hideLicenceActivationLoading();
        this.hideRefreshButtonLoading();
    }
    
    /**
     * Check if a specific component is loading
     */
    isLoading(component) {
        return this.loadingStates.get(component) || false;
    }
    
    /**
     * Check if any component is loading
     */
    isAnyLoading() {
        return Array.from(this.loadingStates.values()).some(loading => loading);
    }
    
    /**
     * Show error state for licence components
     */
    showErrorState(component, message = 'Failed to load licence data') {
        console.log(`[LICENCE-LOADING-UI] Showing error state for ${component}: ${message}`);
        
        switch (component) {
            case 'licenceStatus':
                this.showLicenceStatusError(message);
                break;
            case 'licenceFeatures':
                this.showLicenceFeaturesError(message);
                break;
            case 'licenceConfiguration':
                this.showLicenceConfigurationError(message);
                break;
            case 'licenceEvents':
                this.showLicenceEventsError(message);
                break;
            default:
                console.warn(`[LICENCE-LOADING-UI] Unknown component: ${component}`);
        }
    }
    
    /**
     * Show error state for licence status
     */
    showLicenceStatusError(message) {
        const statusOverview = document.getElementById('license-status-overview');
        if (statusOverview) {
            statusOverview.innerHTML = `
                <div class="bg-red-50 border border-red-200 rounded-lg p-6">
                    <div class="flex items-center">
                        <i class="fa-solid fa-exclamation-triangle text-red-500 mr-3"></i>
                        <div>
                            <h3 class="text-lg font-medium text-red-800">Error Loading License Status</h3>
                            <p class="text-red-600 mt-1">${message}</p>
                            <button onclick="window.licenceLoadingUI.showLicenceStatusLoading()" class="mt-3 px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                                <i class="fa-solid fa-refresh mr-2"></i>Retry
                            </button>
                        </div>
                    </div>
                </div>
            `;
        }
    }
    
    /**
     * Show error state for licence features
     */
    showLicenceFeaturesError(message) {
        const featuresList = document.getElementById('license-features-list');
        if (featuresList) {
            featuresList.innerHTML = `
                <div class="bg-red-50 border border-red-200 rounded-lg p-4">
                    <div class="flex items-center">
                        <i class="fa-solid fa-exclamation-triangle text-red-500 mr-2"></i>
                        <div>
                            <p class="text-red-600 text-sm">${message}</p>
                            <button onclick="window.licenceLoadingUI.showLicenceFeaturesLoading()" class="mt-2 px-3 py-1 bg-red-600 text-white rounded text-sm hover:bg-red-700">
                                <i class="fa-solid fa-refresh mr-1"></i>Retry
                            </button>
                        </div>
                    </div>
                </div>
            `;
        }
    }
    
    /**
     * Show error state for licence configuration
     */
    showLicenceConfigurationError(message) {
        const configContent = document.getElementById('license-configuration-content');
        if (configContent) {
            configContent.innerHTML = `
                <div class="bg-red-50 border border-red-200 rounded-lg p-6">
                    <div class="flex items-center">
                        <i class="fa-solid fa-exclamation-triangle text-red-500 mr-3"></i>
                        <div>
                            <h3 class="text-lg font-medium text-red-800">Error Loading License Configuration</h3>
                            <p class="text-red-600 mt-1">${message}</p>
                            <button onclick="window.licenceLoadingUI.showLicenceConfigurationLoading()" class="mt-3 px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                                <i class="fa-solid fa-refresh mr-2"></i>Retry
                            </button>
                        </div>
                    </div>
                </div>
            `;
        }
    }
    
    /**
     * Show error state for licence events
     */
    showLicenceEventsError(message) {
        const eventsContainer = document.getElementById('license-events-container');
        if (eventsContainer) {
            eventsContainer.innerHTML = `
                <div class="bg-red-50 border border-red-200 rounded-lg p-4">
                    <div class="flex items-center">
                        <i class="fa-solid fa-exclamation-triangle text-red-500 mr-2"></i>
                        <div>
                            <p class="text-red-600 text-sm">${message}</p>
                            <button onclick="window.licenceLoadingUI.showLicenceEventsLoading()" class="mt-2 px-3 py-1 bg-red-600 text-white rounded text-sm hover:bg-red-700">
                                <i class="fa-solid fa-refresh mr-1"></i>Retry
                            </button>
                        </div>
                    </div>
                </div>
            `;
        }
    }
    
    /**
     * Retry loading licence data
     */
    retryLicenceData() {
        console.log('[LICENCE-LOADING-UI] Retrying licence data load...');
        this.showAllLoadingStates();
        // Trigger reload via licence UI manager
        if (window.licenceUI) {
            window.licenceUI.refreshAllData();
        }
    }
    
    /**
     * Initialize the loading UI
     */
    initialize() {
        if (!this.initialized) {
            this.init();
            this.initialized = true;
        }
    }
    
    /**
     * Cleanup method
     */
    destroy() {
        this.loadingStates.clear();
        this.initialized = false;
        
        // Remove styles
        const styleElement = document.getElementById('licence-loading-ui-styles');
        if (styleElement) {
            styleElement.remove();
        }
    }
}

// Export for use in other modules
export { LicenceDataLoadingUI };

// Auto-initialize when DOM is ready and make globally available
document.addEventListener('DOMContentLoaded', () => {
    window.licenceLoadingUI = new LicenceDataLoadingUI();
});

/**
 * Firmware Update Data Loading UI Component
 * Provides loading states and skeleton screens for firmware upgrade components
 */

class FirmwareUpdateDataLoadingUI {
    constructor() {
        this.loadingStates = new Map();
        this.init();
    }
    
    init() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Initializing firmware update loading UI component...');
        this.setupLoadingStyles();
    }
    
    /**
     * Setup CSS styles for loading animations
     */
    setupLoadingStyles() {
        const styleId = 'firmware-update-loading-ui-styles';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = `
                .firmware-skeleton {
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: firmware-loading 1.5s infinite;
                    border-radius: 0.25rem;
                }
                
                @keyframes firmware-loading {
                    0% { background-position: 200% 0; }
                    100% { background-position: -200% 0; }
                }
                
                .firmware-loading-pulse {
                    animation: firmware-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
                }
                
                @keyframes firmware-pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: .5; }
                }
                
                .firmware-loading-spinner {
                    border: 2px solid #f3f3f3;
                    border-top: 2px solid #6b7280;
                    border-radius: 50%;
                    width: 16px;
                    height: 16px;
                    animation: firmware-spin 1s linear infinite;
                    display: inline-block;
                }
                
                @keyframes firmware-spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
                
                .firmware-skeleton-text {
                    height: 1.5rem;
                    border-radius: 0.25rem;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: firmware-loading 1.5s infinite;
                    display: inline-block;
                }
                
                .firmware-skeleton-text-sm {
                    height: 1rem;
                    border-radius: 0.25rem;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: firmware-loading 1.5s infinite;
                    display: inline-block;
                }
                
                .firmware-skeleton-badge {
                    height: 1.25rem;
                    width: 4rem;
                    border-radius: 9999px;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: firmware-loading 1.5s infinite;
                }
                
                .firmware-loading-overlay {
                    position: absolute;
                    top: 0;
                    left: 0;
                    right: 0;
                    bottom: 0;
                    background: rgba(255, 255, 255, 0.8);
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    z-index: 10;
                    border-radius: 0.5rem;
                }
            `;
            document.head.appendChild(style);
        }
    }
    
    /**
     * Show loading state for current firmware section
     */
    showCurrentFirmwareLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Showing current firmware loading state (persistent until data arrives)...');
        
        // Current version loading state
        const currentVersionElement = document.getElementById('current-version');
        if (currentVersionElement) {
            currentVersionElement.innerHTML = '<div class="firmware-skeleton-text w-32 h-6"></div>';
        }
        
        // Build number loading state
        const buildNumberElement = document.getElementById('build-number');
        if (buildNumberElement) {
            buildNumberElement.innerHTML = '<div class="firmware-skeleton-text-sm w-24 h-4"></div>';
        }
        
        // Device model loading state
        const deviceModelElement = document.getElementById('device-model');
        if (deviceModelElement) {
            deviceModelElement.innerHTML = '<div class="firmware-skeleton-text w-28 h-5"></div>';
        }
        
        // Hardware revision loading state
        const hardwareRevisionElement = document.getElementById('hardware-revision');
        if (hardwareRevisionElement) {
            hardwareRevisionElement.innerHTML = '<div class="firmware-skeleton-text-sm w-20 h-4"></div>';
        }
        
        // Build date loading state
        const buildDateElement = document.getElementById('build-date');
        if (buildDateElement) {
            buildDateElement.innerHTML = '<div class="firmware-skeleton-text w-24 h-5"></div>';
        }
        
        // Build time loading state
        const buildTimeElement = document.getElementById('build-time');
        if (buildTimeElement) {
            buildTimeElement.innerHTML = '<div class="firmware-skeleton-text-sm w-20 h-4"></div>';
        }
        
        // Uptime loading state
        const uptimeElement = document.getElementById('uptime');
        if (uptimeElement) {
            uptimeElement.innerHTML = '<div class="firmware-skeleton-text-sm w-16 h-4"></div>';
        }
        
        // Store loading state as persistent
        this.loadingStates.set('current-firmware', 'persistent');
    }
    
    /**
     * Hide loading state for current firmware section
     */
    hideCurrentFirmwareLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Hiding current firmware loading state (data is now available)...');
        
        // Clear loading states
        const loadingElements = document.querySelectorAll('.firmware-skeleton, .firmware-skeleton-text, .firmware-skeleton-text-sm, .firmware-skeleton-badge');
        loadingElements.forEach(element => {
            // Only remove if it's a loading skeleton (not actual content)
            if (element.classList.contains('firmware-skeleton') || 
                element.classList.contains('firmware-skeleton-text') || 
                element.classList.contains('firmware-skeleton-text-sm') || 
                element.classList.contains('firmware-skeleton-badge')) {
                element.remove();
            }
        });
        
        // Remove persistent loading state
        this.loadingStates.set('current-firmware', false);
    }
    
    /**
     * Show loading state for firmware file upload
     */
    showFileUploadLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Showing file upload loading state...');
        
        const uploadArea = document.getElementById('file-upload-area');
        if (uploadArea) {
            uploadArea.classList.add('firmware-loading-pulse');
            uploadArea.style.opacity = '0.6';
        }
        
        this.loadingStates.set('file-upload', true);
    }
    
    /**
     * Hide loading state for firmware file upload
     */
    hideFileUploadLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Hiding file upload loading state...');
        
        const uploadArea = document.getElementById('file-upload-area');
        if (uploadArea) {
            uploadArea.classList.remove('firmware-loading-pulse');
            uploadArea.style.opacity = '1';
        }
        
        this.loadingStates.set('file-upload', false);
    }
    
    /**
     * Show loading state for online updates
     */
    showOnlineUpdateLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Showing online update loading state...');
        
        const checkBtn = document.getElementById('check-updates-btn');
        if (checkBtn) {
            checkBtn.disabled = true;
            checkBtn.innerHTML = '<div class="firmware-loading-spinner"></div> Checking...';
        }
        
        const updatesList = document.getElementById('available-updates');
        if (updatesList) {
            updatesList.innerHTML = `
                <div class="space-y-3">
                    <div class="firmware-skeleton-text w-full"></div>
                    <div class="firmware-skeleton-text w-3/4"></div>
                    <div class="firmware-skeleton-text w-5/6"></div>
                </div>
            `;
        }
        
        this.loadingStates.set('online-update', true);
    }
    
    /**
     * Hide loading state for online updates
     */
    hideOnlineUpdateLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Hiding online update loading state...');
        
        const checkBtn = document.getElementById('check-updates-btn');
        if (checkBtn) {
            checkBtn.disabled = false;
            checkBtn.innerHTML = '<i class="fa-solid fa-search mr-2"></i>Check for Updates';
        }
        
        this.loadingStates.set('online-update', false);
    }
    
    /**
     * Show loading state for TFTP connection test
     */
    showTftpTestLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Showing TFTP test loading state...');
        
        const testBtn = document.getElementById('test-tftp-btn');
        if (testBtn) {
            testBtn.disabled = true;
            testBtn.innerHTML = '<div class="firmware-loading-spinner"></div> Testing...';
        }
        
        this.loadingStates.set('tftp-test', true);
    }
    
    /**
     * Hide loading state for TFTP connection test
     */
    hideTftpTestLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Hiding TFTP test loading state...');
        
        const testBtn = document.getElementById('test-tftp-btn');
        if (testBtn) {
            testBtn.disabled = false;
            testBtn.innerHTML = '<i class="fa-solid fa-network-wired mr-2"></i>Test Connection';
        }
        
        this.loadingStates.set('tftp-test', false);
    }
    
    /**
     * Show loading state for upgrade process
     */
    showUpgradeLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Showing upgrade loading state...');
        
        const startBtn = document.getElementById('start-upgrade-btn');
        if (startBtn) {
            startBtn.disabled = true;
            startBtn.innerHTML = '<div class="firmware-loading-spinner"></div> Upgrading...';
        }
        
        // Disable all upgrade source controls
        const sourceRadios = document.querySelectorAll('input[name="upgrade-source"]');
        sourceRadios.forEach(radio => radio.disabled = true);
        
        this.loadingStates.set('upgrade', true);
    }
    
    /**
     * Hide loading state for upgrade process
     */
    hideUpgradeLoading() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Hiding upgrade loading state...');
        
        const startBtn = document.getElementById('start-upgrade-btn');
        if (startBtn) {
            startBtn.disabled = false;
            startBtn.innerHTML = 'Start Upgrade';
        }
        
        // Enable all upgrade source controls
        const sourceRadios = document.querySelectorAll('input[name="upgrade-source"]');
        sourceRadios.forEach(radio => radio.disabled = false);
        
        this.loadingStates.set('upgrade', false);
    }
    
    /**
     * Show loading overlay for a specific container
     */
    showLoadingOverlay(containerId, message = 'Loading...') {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        // Add relative position to container if not already set
        if (container.style.position !== 'relative') {
            container.style.position = 'relative';
        }
        
        // Create overlay
        const overlay = document.createElement('div');
        overlay.className = 'firmware-loading-overlay';
        overlay.id = `${containerId}-loading-overlay`;
        overlay.innerHTML = `
            <div class="flex flex-col items-center">
                <div class="firmware-loading-spinner mb-2" style="width: 24px; height: 24px; border-width: 3px;"></div>
                <span class="text-sm text-neutral-600">${message}</span>
            </div>
        `;
        
        container.appendChild(overlay);
    }
    
    /**
     * Hide loading overlay for a specific container
     */
    hideLoadingOverlay(containerId) {
        const overlay = document.getElementById(`${containerId}-loading-overlay`);
        if (overlay) {
            overlay.remove();
        }
    }
    
    /**
     * Check if a specific component is loading
     */
    isLoading(component) {
        const state = this.loadingStates.get(component);
        return state === true || state === 'persistent';
    }
    
    /**
     * Show loading state for refresh button
     */
    showRefreshLoading() {
        const refreshBtn = document.getElementById('refresh-firmware-btn');
        if (refreshBtn) {
            const icon = refreshBtn.querySelector('i');
            if (icon) {
                icon.classList.add('fa-spin');
            }
            refreshBtn.disabled = true;
        }
    }
    
    /**
     * Hide loading state for refresh button
     */
    hideRefreshLoading() {
        const refreshBtn = document.getElementById('refresh-firmware-btn');
        if (refreshBtn) {
            const icon = refreshBtn.querySelector('i');
            if (icon) {
                icon.classList.remove('fa-spin');
            }
            refreshBtn.disabled = false;
        }
    }
    
    /**
     * Clear all loading states
     */
    clearAllLoadingStates() {
        console.log('[FIRMWARE-UPDATE-LOADING-UI] Clearing all loading states including persistent ones...');
        
        this.hideCurrentFirmwareLoading();
        this.hideFileUploadLoading();
        this.hideOnlineUpdateLoading();
        this.hideTftpTestLoading();
        this.hideUpgradeLoading();
        this.hideRefreshLoading();
        
        // Remove all loading overlays
        const overlays = document.querySelectorAll('.firmware-loading-overlay');
        overlays.forEach(overlay => overlay.remove());
        
        // Clear loading states map completely
        this.loadingStates.clear();
    }
}

export { FirmwareUpdateDataLoadingUI };

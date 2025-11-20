/**
 * Licence Page Main Controller
 * Manages license operations and business logic without HTTP requests
 */

class LicenceManager {
    constructor(sourceManager = null) {
        this.sourceManager = sourceManager;
        this.initialized = false;
        this.licenceData = null;
        this.configData = {
            signature_validation: true,
            auto_renewal: false,
            validation_interval: 7,
            require_https: true,
            log_activation_attempts: true,
            max_file_size: 5
        };
        this.events = [];
        
        this.init();
    }
    
    async init() {
        try {
            console.log('[LICENCE-MANAGER] Initializing licence page...');
            
            // Use the source manager's authentication if available
            if (this.sourceManager) {
                this.tokenManager = this.sourceManager.tokenManager;
                this.authManager = this.sourceManager.authManager;
                this.currentUser = this.sourceManager.currentUser;
            }
            
            // Initialize with loading states - no HTTP requests
            this.initializeDefaultData();
            
            this.initialized = true;
            console.log('[LICENCE-MANAGER] Licence manager initialized successfully');
            
        } catch (error) {
            console.error('[LICENCE-MANAGER] Initialization failed:', error);
            this.handleInitializationError(error);
        }
    }
    
    /**
     * Initialize default data without HTTP requests
     */
    initializeDefaultData() {
        // Set default licence data (will be shown as loading state)
        this.licenceData = null; // Let loading UI handle the display
        
        // Set default configuration
        this.configData = {
            signature_validation: true,
            auto_renewal: false,
            validation_interval: 7,
            require_https: true,
            log_activation_attempts: true,
            max_file_size: 5
        };
        
        // Set default events (empty)
        this.events = [];
    }
    
    // Licence activation methods (simulated without HTTP requests)
    async activateWithKey(licenseKey) {
        try {
            console.log('[LICENCE-MANAGER] Simulating license key activation for:', licenseKey);
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 1500));
            
            // Add event
            this.addEvent('info', `License key activation attempted: ${licenseKey.substring(0, 8)}...`);
            
            // Return simulated success
            return { success: true, data: { message: 'License activation simulated successfully' } };
        } catch (error) {
            this.addEvent('error', `License activation failed: ${error.message}`);
            return { success: false, error: error.message };
        }
    }
    
    async activateWithFile(file) {
        try {
            console.log('[LICENCE-MANAGER] Simulating license file activation for:', file.name);
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            // Add event
            this.addEvent('info', `License file upload attempted: ${file.name}`);
            
            // Return simulated success
            return { success: true, data: { message: 'License file activation simulated successfully' } };
        } catch (error) {
            this.addEvent('error', `License file activation failed: ${error.message}`);
            return { success: false, error: error.message };
        }
    }
    
    async activateOnline(productCode, email) {
        try {
            console.log('[LICENCE-MANAGER] Simulating online activation for:', productCode, email);
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 3000));
            
            // Add event
            this.addEvent('info', `Online activation attempted for product: ${productCode}`);
            
            // Return simulated success
            return { success: true, data: { message: 'Online activation simulated successfully' } };
        } catch (error) {
            this.addEvent('error', `Online activation failed: ${error.message}`);
            return { success: false, error: error.message };
        }
    }
    
    // Configuration management (simulated without HTTP requests)
    async updateConfiguration(config) {
        try {
            console.log('[LICENCE-MANAGER] Simulating configuration update:', config);
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Update local configuration
            this.configData = { ...this.configData, ...config };
            this.addEvent('info', 'License configuration updated');
            
            return { success: true, data: this.configData };
        } catch (error) {
            this.addEvent('error', `Configuration update failed: ${error.message}`);
            return { success: false, error: error.message };
        }
    }
    
    async resetConfiguration() {
        try {
            console.log('[LICENCE-MANAGER] Simulating configuration reset');
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Reset to defaults
            this.configData = {
                signature_validation: true,
                auto_renewal: false,
                validation_interval: 7,
                require_https: true,
                log_activation_attempts: true,
                max_file_size: 5
            };
            this.addEvent('info', 'License configuration reset to defaults');
            
            return { success: true, data: this.configData };
        } catch (error) {
            this.addEvent('error', `Configuration reset failed: ${error.message}`);
            return { success: false, error: error.message };
        }
    }
    
    // Event management
    addEvent(type, message) {
        const event = {
            id: Date.now(),
            type: type,
            message: message,
            timestamp: new Date().toISOString()
        };
        
        this.events.unshift(event);
        
        // Keep only last 50 events
        if (this.events.length > 50) {
            this.events = this.events.slice(0, 50);
        }
        
        // Notify UI of new event
        this.notifyEventAdded(event);
    }
    
    async clearEvents() {
        try {
            console.log('[LICENCE-MANAGER] Simulating clearing events');
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 500));
            
            this.events = [];
            this.addEvent('info', 'License events cleared');
            
            return { success: true };
        } catch (error) {
            return { success: false, error: error.message };
        }
    }
    
    // Utility methods (simulated without HTTP requests)
    async refreshLicenceStatus() {
        try {
            console.log('[LICENCE-MANAGER] Simulating licence status refresh');
            
            // Simulate processing time
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            this.addEvent('info', 'License status refresh simulated');
            
            return { success: true, data: { message: 'License status refreshed' } };
        } catch (error) {
            this.addEvent('error', `Refresh failed: ${error.message}`);
            return { success: false, error: error.message };
        }
    }
    
    getLicenceData() {
        // Return null to indicate loading state should be shown
        return this.licenceData;
    }
    
    getConfiguration() {
        return this.configData;
    }
    
    getEvents() {
        return this.events;
    }
    
    isInitialized() {
        return this.initialized;
    }
    
    // Event notification system
    notifyEventAdded(event) {
        const customEvent = new CustomEvent('licenceEventAdded', {
            detail: { event: event }
        });
        document.dispatchEvent(customEvent);
    }
    
    notifyLicenceDataUpdated() {
        const customEvent = new CustomEvent('licenceDataUpdated', {
            detail: { data: this.licenceData }
        });
        document.dispatchEvent(customEvent);
    }
    
    notifyConfigurationUpdated() {
        const customEvent = new CustomEvent('licenceConfigurationUpdated', {
            detail: { config: this.configData }
        });
        document.dispatchEvent(customEvent);
    }
    
    handleInitializationError(error) {
        console.error('[LICENCE-MANAGER] Initialization error:', error);
        
        // Show error message to user
        const errorDiv = document.createElement('div');
        errorDiv.className = 'fixed top-4 right-4 bg-red-500 text-white px-6 py-3 rounded-lg shadow-lg z-50';
        errorDiv.innerHTML = `
            <div class="flex items-center">
                <i class="fa-solid fa-exclamation-triangle mr-2"></i>
                <span>Failed to initialize licence manager: ${error.message}</span>
            </div>
        `;
        document.body.appendChild(errorDiv);
        
        // Remove error message after 5 seconds
        setTimeout(() => {
            if (errorDiv.parentNode) {
                errorDiv.parentNode.removeChild(errorDiv);
            }
        }, 5000);
    }
    
    // Cleanup method
    destroy() {
        this.initialized = false;
        this.licenceData = null;
        this.events = [];
    }
}

// Export for use in other modules
export { LicenceManager };

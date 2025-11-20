/**
 * Data Loading UI Component for Network Priority
 * Provides loading states and skeleton screens for network priority components
 */

class DataLoadingUI {
    constructor() {
        this.loadingStates = new Map();
        this.init();
    }
    
    init() {
        console.log('[DATA-LOADING-UI] Initializing data loading UI component...');
        this.setupLoadingStyles();
    }
    
    /**
     * Setup CSS styles for loading animations
     */
    setupLoadingStyles() {
        const styleId = 'data-loading-ui-styles';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = `
                .skeleton {
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: loading 1.5s infinite;
                }
                
                @keyframes loading {
                    0% { background-position: 200% 0; }
                    100% { background-position: -200% 0; }
                }
                
                .loading-pulse {
                    animation: pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
                }
                
                @keyframes pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: .5; }
                }
                
                .loading-spinner {
                    border: 2px solid #f3f3f3;
                    border-top: 2px solid #3b82f6;
                    border-radius: 50%;
                    width: 20px;
                    height: 20px;
                    animation: spin 1s linear infinite;
                }
                
                @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
            `;
            document.head.appendChild(style);
        }
    }
    
    /**
     * Show loading state for interfaces container
     */
    showInterfacesLoading() {
        const container = document.getElementById('interfaces-container');
        if (!container) return;
        
        console.log('[DATA-LOADING-UI] Showing interfaces loading state...');
        
        container.innerHTML = `
            <div class="space-y-4">
                ${this.createInterfaceSkeleton()}
                ${this.createInterfaceSkeleton()}
                ${this.createInterfaceSkeleton()}
            </div>
        `;
        
        this.loadingStates.set('interfaces', true);
    }
    
    /**
     * Create skeleton HTML for interface card
     */
    createInterfaceSkeleton() {
        return `
            <div class="bg-white border border-neutral-200 rounded-lg p-4">
                <div class="flex items-center justify-between">
                    <div class="flex items-center space-x-4">
                        <div class="skeleton w-10 h-10 rounded-full"></div>
                        <div class="space-y-2">
                            <div class="skeleton h-4 w-24 rounded"></div>
                            <div class="skeleton h-3 w-32 rounded"></div>
                            <div class="skeleton h-3 w-28 rounded"></div>
                        </div>
                    </div>
                    <div class="text-right space-y-2">
                        <div class="skeleton h-4 w-16 rounded"></div>
                        <div class="skeleton h-3 w-24 rounded"></div>
                        <div class="skeleton h-3 w-20 rounded"></div>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Show loading state for routing rules table
     */
    showRoutingRulesLoading() {
        const tbody = document.getElementById('routing-rules-tbody');
        if (!tbody) return;
        
        console.log('[DATA-LOADING-UI] Showing routing rules loading state...');
        
        tbody.innerHTML = `
            ${this.createRoutingRuleSkeleton()}
            ${this.createRoutingRuleSkeleton()}
            ${this.createRoutingRuleSkeleton()}
            ${this.createRoutingRuleSkeleton()}
        `;
        
        this.loadingStates.set('routingRules', true);
    }
    
    /**
     * Create skeleton HTML for routing rule row
     */
    createRoutingRuleSkeleton() {
        return `
            <tr class="hover:bg-neutral-50">
                <td class="px-4 py-3">
                    <div class="skeleton w-4 h-4 rounded"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="skeleton h-4 w-8 rounded"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="skeleton h-4 w-24 rounded"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="skeleton h-4 w-20 rounded"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="skeleton h-4 w-16 rounded"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="skeleton h-4 w-8 rounded"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="skeleton h-6 w-16 rounded-full"></div>
                </td>
                <td class="px-4 py-3">
                    <div class="flex space-x-2">
                        <div class="skeleton w-6 h-6 rounded"></div>
                        <div class="skeleton w-6 h-6 rounded"></div>
                        <div class="skeleton w-6 h-6 rounded"></div>
                    </div>
                </td>
            </tr>
        `;
    }
    
    /**
     * Show loading state for statistics
     */
    showStatisticsLoading() {
        const stats = ['total', 'online', 'offline'];
        stats.forEach(stat => {
            const element = document.querySelector(`[data-stat="${stat}"]`);
            if (element) {
                element.innerHTML = '<div class="loading-spinner mx-auto"></div>';
            }
        });
        
        this.loadingStates.set('statistics', true);
    }
    
    /**
     * Show connection status loading
     */
    showConnectionStatusLoading() {
        const statusIndicator = document.getElementById('priority-connection-status');
        if (statusIndicator) {
            // Use setAttribute instead of className to avoid read-only property issues
            try {
                statusIndicator.setAttribute('class', 'px-3 py-1 rounded-full text-sm font-medium bg-neutral-100 text-neutral-500 loading-pulse');
                statusIndicator.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Connecting...';
            } catch (error) {
                console.warn('[DATA-LOADING-UI] Could not set connection status loading state:', error);
                // Fallback: just update the text content
                statusIndicator.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Connecting...';
            }
        }
        
        this.loadingStates.set('connectionStatus', true);
    }
    
    /**
     * Hide loading state for specific component
     */
    hideLoading(component) {
        this.loadingStates.delete(component);
        console.log(`[DATA-LOADING-UI] Hidden loading state for: ${component}`);
    }
    
    /**
     * Check if any components are still loading
     */
    isLoading() {
        return this.loadingStates.size > 0;
    }
    
    /**
     * Show loading state for all network priority components
     */
    showAllLoadingStates() {
        console.log('[DATA-LOADING-UI] Showing all loading states...');
        this.showInterfacesLoading();
        this.showRoutingRulesLoading();
        this.showStatisticsLoading();
        this.showConnectionStatusLoading();
    }
    
    /**
     * Hide all loading states
     */
    hideAllLoadingStates() {
        console.log('[DATA-LOADING-UI] Hiding all loading states...');
        this.loadingStates.clear();
    }
    
    /**
     * Show empty state for interfaces
     */
    showInterfacesEmpty() {
        const container = document.getElementById('interfaces-container');
        if (!container) return;
        
        container.innerHTML = `
            <div class="text-center py-12">
                <i class="fa-solid fa-network-wired text-4xl text-neutral-300 mb-4"></i>
                <h3 class="text-lg font-medium text-neutral-900 mb-2">No Interfaces Available</h3>
                <p class="text-neutral-500">Network interfaces will appear here once connected.</p>
            </div>
        `;
    }
    
    /**
     * Show empty state for routing rules
     */
    showRoutingRulesEmpty() {
        const tbody = document.getElementById('routing-rules-tbody');
        if (!tbody) return;
        
        tbody.innerHTML = `
            <tr>
                <td colspan="8" class="px-4 py-12 text-center">
                    <i class="fa-solid fa-route text-3xl text-neutral-300 mb-3 block"></i>
                    <h3 class="text-lg font-medium text-neutral-900 mb-2">No Routing Rules Configured</h3>
                    <p class="text-neutral-500">Click "Add Rule" to create your first routing rule.</p>
                </td>
            </tr>
        `;
    }
    
    /**
     * Show error state for a component
     */
    showError(component, message = 'Failed to load data') {
        console.error(`[DATA-LOADING-UI] Showing error state for ${component}:`, message);
        
        switch (component) {
            case 'interfaces':
                const container = document.getElementById('interfaces-container');
                if (container) {
                    container.innerHTML = `
                        <div class="text-center py-12">
                            <i class="fa-solid fa-exclamation-triangle text-4xl text-red-300 mb-4"></i>
                            <h3 class="text-lg font-medium text-neutral-900 mb-2">Failed to Load Interfaces</h3>
                            <p class="text-neutral-500 mb-4">${message}</p>
                            <button class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 transition-colors" onclick="window.dataLoadingUI.retryInterfaces()">
                                <i class="fa-solid fa-refresh mr-2"></i>Retry
                            </button>
                        </div>
                    `;
                }
                break;
                
            case 'routingRules':
                const tbody = document.getElementById('routing-rules-tbody');
                if (tbody) {
                    tbody.innerHTML = `
                        <tr>
                            <td colspan="8" class="px-4 py-12 text-center">
                                <i class="fa-solid fa-exclamation-triangle text-3xl text-red-300 mb-3 block"></i>
                                <h3 class="text-lg font-medium text-neutral-900 mb-2">Failed to Load Routing Rules</h3>
                                <p class="text-neutral-500 mb-4">${message}</p>
                                <button class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 transition-colors" onclick="window.dataLoadingUI.retryRoutingRules()">
                                    <i class="fa-solid fa-refresh mr-2"></i>Retry
                                </button>
                            </td>
                        </tr>
                    `;
                }
                break;
        }
    }
    
    /**
     * Retry loading interfaces
     */
    retryInterfaces() {
        console.log('[DATA-LOADING-UI] Retrying interface load...');
        this.showInterfacesLoading();
        // Trigger reload via network priority manager
        if (window.networkPriorityManager) {
            window.networkPriorityManager.loadInterfacesData();
        }
    }
    
    /**
     * Retry loading routing rules
     */
    retryRoutingRules() {
        console.log('[DATA-LOADING-UI] Retrying routing rules load...');
        this.showRoutingRulesLoading();
        // Trigger reload via network priority manager
        if (window.networkPriorityManager) {
            window.networkPriorityManager.loadRoutingRules();
        }
    }
}

// Export for use in other modules
export { DataLoadingUI };

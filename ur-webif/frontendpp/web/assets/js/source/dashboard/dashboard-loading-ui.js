/**
 * Dashboard Data Loading UI Component
 * Provides loading states and skeleton screens for dashboard components
 */

class DashboardLoadingUI {
    constructor() {
        this.loadingStates = new Map();
        this.init();
    }
    
    init() {
        console.log('[DASHBOARD-LOADING-UI] Initializing dashboard loading UI component...');
        this.setupLoadingStyles();
    }
    
    /**
     * Setup CSS styles for loading animations
     */
    setupLoadingStyles() {
        const styleId = 'dashboard-loading-ui-styles';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = `
                .dashboard-skeleton {
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: dashboard-loading 1.5s infinite;
                    border-radius: 0.25rem;
                }
                
                @keyframes dashboard-loading {
                    0% { background-position: 200% 0; }
                    100% { background-position: -200% 0; }
                }
                
                .dashboard-loading-pulse {
                    animation: dashboard-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
                }
                
                @keyframes dashboard-pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: .5; }
                }
                
                .dashboard-loading-spinner {
                    border: 2px solid #f3f3f3;
                    border-top: 2px solid #6b7280;
                    border-radius: 50%;
                    width: 16px;
                    height: 16px;
                    animation: dashboard-spin 1s linear infinite;
                    display: inline-block;
                }
                
                @keyframes dashboard-spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
                
                .dashboard-skeleton-progress {
                    height: 0.75rem;
                    border-radius: 9999px;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: dashboard-loading 1.5s infinite;
                }
                
                .dashboard-skeleton-indicator {
                    width: 0.5rem;
                    height: 0.5rem;
                    border-radius: 50%;
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: dashboard-loading 1.5s infinite;
                }
            `;
            document.head.appendChild(style);
        }
    }
    
    /**
     * Show loading state for system stats (CPU, RAM, Swap)
     */
    showSystemStatsLoading() {
        console.log('[DASHBOARD-LOADING-UI] Showing system stats loading state...');
        
        // CPU Loading State
        const cpuUsage = document.querySelector('[data-cpu-usage]');
        const cpuProgress = document.querySelector('[data-cpu-progress]');
        const cpuCores = document.querySelector('[data-cpu-cores]');
        const cpuTemp = document.querySelector('[data-cpu-temp]');
        const cpuBaseClock = document.querySelector('[data-cpu-base-clock]');
        const cpuBoostClock = document.querySelector('[data-cpu-boost-clock]');
        
        if (cpuUsage) cpuUsage.innerHTML = '<div class="dashboard-loading-spinner"></div>';
        if (cpuProgress) cpuProgress.className = 'dashboard-skeleton-progress';
        if (cpuCores) cpuCores.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (cpuTemp) cpuTemp.innerHTML = '<span class="dashboard-skeleton inline-block w-8 h-3"></span>';
        if (cpuBaseClock) cpuBaseClock.innerHTML = '<span class="dashboard-skeleton inline-block w-10 h-3"></span>';
        if (cpuBoostClock) cpuBoostClock.innerHTML = '<span class="dashboard-skeleton inline-block w-10 h-3"></span>';
        
        // RAM Loading State
        const ramUsage = document.querySelector('[data-ram-usage]');
        const ramProgress = document.querySelector('[data-ram-progress]');
        const ramUsed = document.querySelector('[data-ram-used]');
        const ramTotal = document.querySelector('[data-ram-total]');
        const ramAvailable = document.querySelector('[data-ram-available]');
        const ramType = document.querySelector('[data-ram-type]');
        
        if (ramUsage) ramUsage.innerHTML = '<div class="dashboard-loading-spinner"></div>';
        if (ramProgress) ramProgress.className = 'dashboard-skeleton-progress';
        if (ramUsed) ramUsed.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (ramTotal) ramTotal.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (ramAvailable) ramAvailable.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (ramType) ramType.innerHTML = '<span class="dashboard-skeleton inline-block w-8 h-3"></span>';
        
        // Swap Loading State
        const swapUsage = document.querySelector('[data-swap-usage]');
        const swapProgress = document.querySelector('[data-swap-progress]');
        const swapUsed = document.querySelector('[data-swap-used]');
        const swapTotal = document.querySelector('[data-swap-total]');
        const swapAvailable = document.querySelector('[data-swap-available]');
        const swapPriority = document.querySelector('[data-swap-priority]');
        
        if (swapUsage) swapUsage.innerHTML = '<div class="dashboard-loading-spinner"></div>';
        if (swapProgress) swapProgress.className = 'dashboard-skeleton-progress';
        if (swapUsed) swapUsed.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (swapTotal) swapTotal.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (swapAvailable) swapAvailable.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (swapPriority) swapPriority.innerHTML = '<span class="dashboard-skeleton inline-block w-10 h-3"></span>';
        
        this.loadingStates.set('systemStats', true);
    }
    
    /**
     * Show loading state for network status
     */
    showNetworkStatusLoading() {
        console.log('[DASHBOARD-LOADING-UI] Showing network status loading state...');
        
        // Internet Loading State
        const internetIndicator = document.querySelector('[data-internet-indicator]');
        const internetStatus = document.querySelector('[data-internet-status]');
        const externalIp = document.querySelector('[data-external-ip]');
        const dnsPrimary = document.querySelector('[data-dns-primary]');
        const dnsSecondary = document.querySelector('[data-dns-secondary]');
        const latency = document.querySelector('[data-latency]');
        const bandwidth = document.querySelector('[data-bandwidth]');
        
        if (internetIndicator) internetIndicator.className = 'dashboard-skeleton-indicator';
        if (internetStatus) internetStatus.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (externalIp) externalIp.innerHTML = '<span class="dashboard-skeleton inline-block w-20 h-3"></span>';
        if (dnsPrimary) dnsPrimary.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (dnsSecondary) dnsSecondary.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (latency) latency.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (bandwidth) bandwidth.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        
        // Ultima Server Loading State
        const serverIndicator = document.querySelector('[data-server-indicator]');
        const serverStatus = document.querySelector('[data-server-status]');
        const serverHostname = document.querySelector('[data-server-hostname]');
        const serverPort = document.querySelector('[data-server-port]');
        const serverProtocol = document.querySelector('[data-server-protocol]');
        const lastPing = document.querySelector('[data-last-ping]');
        const sessionDuration = document.querySelector('[data-session-duration]');
        
        if (serverIndicator) serverIndicator.className = 'dashboard-skeleton-indicator';
        if (serverStatus) serverStatus.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (serverHostname) serverHostname.innerHTML = '<span class="dashboard-skeleton inline-block w-24 h-3"></span>';
        if (serverPort) serverPort.innerHTML = '<span class="dashboard-skeleton inline-block w-8 h-3"></span>';
        if (serverProtocol) serverProtocol.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (lastPing) lastPing.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (sessionDuration) sessionDuration.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        
        // Connection Type Loading State
        const connectionType = document.querySelector('[data-connection-type]');
        const interfaceName = document.querySelector('[data-interface-name]');
        const macAddress = document.querySelector('[data-mac-address]');
        const connectionSpeed = document.querySelector('[data-connection-speed]');
        const connectionUptime = document.querySelector('[data-connection-uptime]');
        
        if (connectionType) connectionType.innerHTML = '<span class="dashboard-skeleton inline-block w-20 h-3"></span>';
        if (interfaceName) interfaceName.innerHTML = '<span class="dashboard-skeleton inline-block w-20 h-3"></span>';
        if (macAddress) macAddress.innerHTML = '<span class="dashboard-skeleton inline-block w-24 h-3"></span>';
        if (connectionSpeed) connectionSpeed.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (connectionUptime) connectionUptime.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        
        this.loadingStates.set('networkStatus', true);
    }
    
    /**
     * Show loading state for cellular status
     */
    showCellularStatusLoading() {
        console.log('[DASHBOARD-LOADING-UI] Showing cellular status loading state...');
        
        // Signal Loading State
        const signalStrength = document.querySelector('[data-signal-strength]');
        const signalType = document.querySelector('[data-signal-type]');
        const signalQuality = document.querySelector('[data-signal-quality]');
        const signalDbm = document.querySelector('[data-signal-dbm]');
        
        if (signalStrength) signalStrength.innerHTML = '<div class="dashboard-loading-spinner"></div>';
        if (signalType) signalType.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (signalQuality) signalQuality.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        if (signalDbm) signalDbm.innerHTML = '<span class="dashboard-skeleton inline-block w-12 h-3"></span>';
        
        // Cellular Connection Loading State
        const cellularStatus = document.querySelector('[data-cellular-status]');
        const cellularProvider = document.querySelector('[data-cellular-provider]');
        const cellularTechnology = document.querySelector('[data-cellular-technology]');
        const cellularApn = document.querySelector('[data-cellular-apn]');
        const cellularIccid = document.querySelector('[data-cellular-iccid]');
        const cellularImei = document.querySelector('[data-cellular-imei]');
        
        if (cellularStatus) cellularStatus.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (cellularProvider) cellularProvider.innerHTML = '<span class="dashboard-skeleton inline-block w-20 h-3"></span>';
        if (cellularTechnology) cellularTechnology.innerHTML = '<span class="dashboard-skeleton inline-block w-16 h-3"></span>';
        if (cellularApn) cellularApn.innerHTML = '<span class="dashboard-skeleton inline-block w-24 h-3"></span>';
        if (cellularIccid) cellularIccid.innerHTML = '<span class="dashboard-skeleton inline-block w-28 h-3"></span>';
        if (cellularImei) cellularImei.innerHTML = '<span class="dashboard-skeleton inline-block w-28 h-3"></span>';
        
        this.loadingStates.set('cellularStatus', true);
    }
    
    /**
     * Show loading state for update indicators
     */
    showUpdateIndicatorsLoading() {
        console.log('[DASHBOARD-LOADING-UI] Showing update indicators loading state...');
        
        const lastUpdate = document.querySelector('[data-last-update]');
        const lastLogin = document.querySelector('[data-last-login]');
        const statusDot = document.getElementById('status-dot');
        
        if (lastUpdate) lastUpdate.innerHTML = '<span class="dashboard-loading-spinner"></span>';
        if (lastLogin) lastLogin.innerHTML = '<span class="dashboard-skeleton inline-block w-20 h-3"></span>';
        if (statusDot) statusDot.className = 'w-2 h-2 dashboard-skeleton-indicator animate-pulse';
        
        this.loadingStates.set('updateIndicators', true);
    }
    
    /**
     * Hide loading state for specific component
     */
    hideLoading(component) {
        this.loadingStates.delete(component);
        console.log(`[DASHBOARD-LOADING-UI] Hidden loading state for: ${component}`);
    }
    
    /**
     * Check if any components are still loading
     */
    isLoading(component) {
        return this.loadingStates.has(component);
    }
    
    /**
     * Show loading state for all dashboard components
     */
    showAllLoadingStates() {
        console.log('[DASHBOARD-LOADING-UI] Showing all dashboard loading states...');
        this.showSystemStatsLoading();
        this.showNetworkStatusLoading();
        this.showCellularStatusLoading();
        this.showUpdateIndicatorsLoading();
    }
    
    /**
     * Hide all loading states
     */
    hideAllLoadingStates() {
        console.log('[DASHBOARD-LOADING-UI] Hiding all dashboard loading states...');
        this.loadingStates.clear();
    }
    
    /**
     * Show error state for a component
     */
    showError(component, message = 'Failed to load data') {
        console.error(`[DASHBOARD-LOADING-UI] Showing error state for ${component}:`, message);
        
        switch (component) {
            case 'systemStats':
                const cpuUsage = document.querySelector('[data-cpu-usage]');
                const ramUsage = document.querySelector('[data-ram-usage]');
                const swapUsage = document.querySelector('[data-swap-usage]');
                
                if (cpuUsage) cpuUsage.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                if (ramUsage) ramUsage.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                if (swapUsage) swapUsage.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                break;
                
            case 'networkStatus':
                const internetStatus = document.querySelector('[data-internet-status]');
                const serverStatus = document.querySelector('[data-server-status]');
                const connectionType = document.querySelector('[data-connection-type]');
                
                if (internetStatus) internetStatus.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                if (serverStatus) serverStatus.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                if (connectionType) connectionType.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                break;
                
            case 'cellularStatus':
                const signalStrength = document.querySelector('[data-signal-strength]');
                const cellularStatus = document.querySelector('[data-cellular-status]');
                
                if (signalStrength) signalStrength.innerHTML = '<span class="text-red-500">--</span>';
                if (cellularStatus) cellularStatus.innerHTML = '<span class="text-red-500 text-sm">Error</span>';
                break;
        }
    }
    
    /**
     * Retry loading dashboard data
     */
    retryDashboardData() {
        console.log('[DASHBOARD-LOADING-UI] Retrying dashboard data load...');
        this.showAllLoadingStates();
        // Trigger reload via dashboard manager
        if (window.dashboard) {
            window.dashboard.refreshData();
        }
    }
}

// Export for use in other modules
export { DashboardLoadingUI };

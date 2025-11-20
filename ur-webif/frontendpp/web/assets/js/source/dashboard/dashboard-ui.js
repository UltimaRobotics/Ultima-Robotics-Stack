/**
 * Dashboard UI Module
 * Defines the UI elements and layout for the main dashboard
 * Integrates with the main-content div in source UI
 */

import { DashboardLoadingUI } from './dashboard-loading-ui.js';

class DashboardUI {
    constructor() {
        this.container = null;
        this.currentView = 'dashboard';
        this.loadingUI = null;
        this.init();
    }

    init() {
        console.log('[DASHBOARD-UI] Initializing dashboard UI...');
        this.loadingUI = new DashboardLoadingUI();
        this.createDashboardHTML();
        // Show loading states immediately after creating HTML
        setTimeout(() => {
            this.showLoadingStates();
        }, 100);
    }
    
    /**
     * Show loading states for all dashboard components
     */
    showLoadingStates() {
        console.log('[DASHBOARD-UI] Showing dashboard loading states...');
        if (this.loadingUI) {
            this.loadingUI.showAllLoadingStates();
        }
    }

    /**
     * Create the complete dashboard HTML structure based on dashboard-ui.html
     */
    createDashboardHTML() {
        return `
            <div class="flex items-center justify-between mb-8">
                <h1 class="text-2xl text-neutral-800">Welcome to Ultima Robotics</h1>
                <div class="flex items-center space-x-4">
                    <button id="auto-refresh-toggle" class="text-sm text-neutral-500 hover:text-neutral-700 flex items-center space-x-1 px-3 py-1 rounded border border-neutral-300 hover:bg-neutral-50 transition-colors" title="Toggle automatic refresh">
                        <i class="fa-solid fa-clock" id="auto-refresh-icon"></i>
                        <span id="auto-refresh-text">Auto</span>
                    </button>
                    <button id="refresh-data-btn" class="text-sm text-neutral-500 hover:text-neutral-700 flex items-center space-x-1 px-3 py-1 rounded border border-neutral-300 hover:bg-neutral-50 transition-colors">
                        <i class="fa-solid fa-refresh" id="refresh-icon"></i>
                        <span>Refresh</span>
                    </button>
                    <span class="text-sm text-neutral-500" data-last-login></span>
                    <div id="update-indicator" class="flex items-center space-x-1 text-xs text-neutral-400">
                        <div class="w-2 h-2 bg-green-500 rounded-full animate-pulse" id="status-dot"></div>
                        <span data-last-update></span>
                    </div>
                </div>
            </div>

            <!-- System Stats Section -->
            <div class="system p-6">
                <div class="grid grid-cols-1 md:grid-cols-3 gap-6 mb-6">
                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="cpu flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-gauge-high"></i>
                                <span class="text-neutral-800">CPU Usage</span>
                            </div>
                            <span class="text-2xl text-neutral-600" data-cpu-usage>0%</span>
                        </div>
                        <div class="w-full bg-neutral-200 rounded-full h-3 mb-2">
                            <div class="bg-neutral-500 h-3 rounded-full" data-cpu-progress style="width: 0%"></div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span data-cpu-cores>0 Cores</span>
                                <span data-cpu-temp>0°C</span>
                            </div>
                            <div class="flex justify-between">
                                <span data-cpu-base-clock>0 GHz</span>
                                <span data-cpu-boost-clock>0 GHz</span>
                            </div>
                        </div>
                    </div>

                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-memory"></i>
                                <span class="text-neutral-800">RAM Usage</span>
                            </div>
                            <span class="text-2xl text-neutral-600" data-ram-usage>0%</span>
                        </div>
                        <div class="w-full bg-neutral-200 rounded-full h-3 mb-2">
                            <div class="bg-neutral-500 h-3 rounded-full" data-ram-progress style="width: 0%"></div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span data-ram-used>0 GB</span>
                                <span data-ram-total>0 GB</span>
                            </div>
                            <div class="flex justify-between">
                                <span data-ram-available>0 GB</span>
                                <span data-ram-type>N/A</span>
                            </div>
                        </div>
                    </div>

                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-hard-drive"></i>
                                <span class="text-neutral-800">Swap Usage</span>
                            </div>
                            <span class="text-2xl text-neutral-600" data-swap-usage>0%</span>
                        </div>
                        <div class="w-full bg-neutral-200 rounded-full h-3 mb-2">
                            <div class="bg-neutral-500 h-3 rounded-full" data-swap-progress style="width: 0%"></div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span data-swap-used>0 MB</span>
                                <span data-swap-total>0 GB</span>
                            </div>
                            <div class="flex justify-between">
                                <span data-swap-available>0 GB</span>
                                <span data-swap-priority>Normal</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Network Status Section -->
            <div class="p-6">
                <div class="grid grid-cols-1 md:grid-cols-3 gap-6 mb-6">
                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-globe"></i>
                                <span class="text-neutral-800">Internet</span>
                            </div>
                            <div class="flex items-center space-x-1">
                                <div class="w-2 h-2 bg-neutral-500 rounded-full" data-internet-indicator></div>
                                <span class="text-sm text-neutral-600" data-internet-status>Unknown</span>
                            </div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span>External IP:</span>
                                <span data-external-ip>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>DNS Primary:</span>
                                <span data-dns-primary>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>DNS Secondary:</span>
                                <span data-dns-secondary>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Latency:</span>
                                <span class="text-neutral-600" data-latency>0 ms</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Bandwidth:</span>
                                <span data-bandwidth>N/A</span>
                            </div>
                        </div>
                    </div>

                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-server"></i>
                                <span class="text-neutral-800">Ultima Server</span>
                            </div>
                            <div class="flex items-center space-x-1">
                                <div class="w-2 h-2 bg-neutral-500 rounded-full" data-server-indicator></div>
                                <span class="text-sm text-neutral-600" data-server-status>Unknown</span>
                            </div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span>Server:</span>
                                <span data-server-hostname>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Port:</span>
                                <span data-server-port>0</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Protocol:</span>
                                <span data-server-protocol>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Last Ping:</span>
                                <span class="text-neutral-600" data-last-ping>0 ms</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Session:</span>
                                <span data-session-duration>N/A</span>
                            </div>
                        </div>
                    </div>

                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-ethernet"></i>
                                <span class="text-neutral-800">Connection Type</span>
                            </div>
                            <div class="text-sm text-neutral-600" data-connection-type>Unknown</div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span>Interface:</span>
                                <span data-interface-name>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>MAC Address:</span>
                                <span data-mac-address>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Local IP:</span>
                                <span data-local-ip>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Gateway:</span>
                                <span data-gateway-ip>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Speed:</span>
                                <span data-connection-speed>N/A</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Mobile/Cellular Status Section -->
            <div class="p-6">
                <div class="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-signal"></i>
                                <span class="text-neutral-800">Signal Strength</span>
                            </div>
                            <div class="flex items-center space-x-2">
                                <div class="flex items-center space-x-1" data-signal-bars>
                                    <div class="w-1 h-3 bg-neutral-300 rounded"></div>
                                    <div class="w-1 h-4 bg-neutral-300 rounded"></div>
                                    <div class="w-1 h-5 bg-neutral-300 rounded"></div>
                                    <div class="w-1 h-6 bg-neutral-300 rounded"></div>
                                </div>
                                <span class="text-sm text-neutral-500" data-signal-status>No Signal</span>
                            </div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span>RSSI:</span>
                                <span class="text-neutral-600" data-rssi>0 dBm</span>
                            </div>
                            <div class="flex justify-between">
                                <span>RSRP:</span>
                                <span class="text-neutral-600" data-rsrp>0 dBm</span>
                            </div>
                            <div class="flex justify-between">
                                <span>RSRQ:</span>
                                <span class="text-neutral-600" data-rsrq>0 dB</span>
                            </div>
                            <div class="flex justify-between">
                                <span>SINR:</span>
                                <span class="text-neutral-600" data-sinr>0 dB</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Cell ID:</span>
                                <span class="text-neutral-500" data-cell-id>N/A</span>
                            </div>
                        </div>
                    </div>

                    <div class="bg-gradient-to-br from-neutral-50 to-neutral-100 border border-neutral-200 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-3">
                            <div class="flex items-center space-x-2">
                                <i class="text-neutral-600 fa-solid fa-mobile-screen-button"></i>
                                <span class="text-neutral-800">Connection Status</span>
                            </div>
                            <div class="flex items-center space-x-1">
                                <div class="w-2 h-2 bg-neutral-400 rounded-full" data-cellular-indicator></div>
                                <span class="text-sm text-neutral-600" data-cellular-status>Disconnected</span>
                            </div>
                        </div>
                        <div class="text-xs text-neutral-600 space-y-1">
                            <div class="flex justify-between">
                                <span>Network:</span>
                                <span class="text-neutral-500" data-network-name>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Technology:</span>
                                <span class="text-neutral-500" data-technology>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Band:</span>
                                <span class="text-neutral-500" data-band>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>APN:</span>
                                <span class="text-neutral-500" data-apn>N/A</span>
                            </div>
                            <div class="flex justify-between">
                                <span>Data Usage:</span>
                                <span class="text-neutral-500" data-data-usage>0 MB</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Quick Actions Section -->
            <div id="quick-actions" class="mb-8">
                <h2 class="text-lg text-neutral-800 mb-4">Quick Actions</h2>
                <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
                    <div id="cellular-cnfg-btn" class="action-card bg-white rounded-lg border border-neutral-200 p-5 hover:shadow-md transition-shadow cursor-pointer">
                        <div class="flex items-center justify-between mb-4">
                            <div class="w-10 h-10 rounded-full bg-neutral-100 flex items-center justify-center">
                                <i class="fa-solid fa-plus text-neutral-600"></i>
                            </div>
                            <i class="fa-solid fa-arrow-right text-neutral-400"></i>
                        </div>
                        <h3 class="text-sm text-neutral-800">Configure Cellular</h3>
                        <p class="text-xs text-neutral-500 mt-1">Manage 5G/LTE configuration</p>
                    </div>

                    <div id="network-scan-btn" class="action-card bg-white rounded-lg border border-neutral-200 p-5 hover:shadow-md transition-shadow cursor-pointer">
                        <div class="flex items-center justify-between mb-4">
                            <div class="w-10 h-10 rounded-full bg-neutral-100 flex items-center justify-center">
                                <i class="fa-solid fa-wifi text-neutral-600"></i>
                            </div>
                            <i class="fa-solid fa-arrow-right text-neutral-400"></i>
                        </div>
                        <h3 class="text-sm text-neutral-800">Network Scan</h3>
                        <p class="text-xs text-neutral-500 mt-1">Discover devices on your network</p>
                    </div>

                    <div id="update-firmware-btn" class="action-card bg-white rounded-lg border border-neutral-200 p-5 hover:shadow-md transition-shadow cursor-pointer">
                        <div class="flex items-center justify-between mb-4">
                            <div class="w-10 h-10 rounded-full bg-neutral-100 flex items-center justify-center">
                                <i class="fa-solid fa-download text-neutral-600"></i>
                            </div>
                            <i class="fa-solid fa-arrow-right text-neutral-400"></i>
                        </div>
                        <h3 class="text-sm text-neutral-800">Update Firmware</h3>
                        <p class="text-xs text-neutral-500 mt-1">Keep your devices up to date</p>
                    </div>

                    <div id="backup-config-btn" class="action-card bg-white rounded-lg border border-neutral-200 p-5 hover:shadow-md transition-shadow cursor-pointer">
                        <div class="flex items-center justify-between mb-4">
                            <div class="w-10 h-10 rounded-full bg-neutral-100 flex items-center justify-center">
                                <i class="fa-solid fa-shield text-neutral-600"></i>
                            </div>
                            <i class="fa-solid fa-arrow-right text-neutral-400"></i>
                        </div>
                        <h3 class="text-sm text-neutral-800">Backup Config</h3>
                        <p class="text-xs text-neutral-500 mt-1">Save your current configuration</p>
                    </div>
                </div>
            </div>
        `;
    }

    /**
     * Render the dashboard in the specified container
     */
    render(containerId = 'main-content') {
        this.container = document.getElementById(containerId);
        if (!this.container) {
            console.error('[DASHBOARD-UI] Container not found:', containerId);
            return false;
        }

        console.log('[DASHBOARD-UI] Rendering dashboard in container:', containerId);
        this.container.innerHTML = this.createDashboardHTML();
        this.attachEventListeners();
        return true;
    }

    /**
     * Attach event listeners to dashboard elements
     */
    attachEventListeners() {
        // Auto refresh toggle
        const autoRefreshToggle = this.container.querySelector('#auto-refresh-toggle');
        if (autoRefreshToggle) {
            autoRefreshToggle.addEventListener('click', () => this.handleAutoRefreshToggle());
        }

        // Refresh data button
        const refreshBtn = this.container.querySelector('#refresh-data-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.handleRefreshData());
        }

        // Quick action buttons
        const cellularConfigBtn = this.container.querySelector('#cellular-cnfg-btn');
        if (cellularConfigBtn) {
            cellularConfigBtn.addEventListener('click', () => this.handleCellularConfig());
        }

        const networkScanBtn = this.container.querySelector('#network-scan-btn');
        if (networkScanBtn) {
            networkScanBtn.addEventListener('click', () => this.handleNetworkScan());
        }

        const updateFirmwareBtn = this.container.querySelector('#update-firmware-btn');
        if (updateFirmwareBtn) {
            updateFirmwareBtn.addEventListener('click', () => this.handleUpdateFirmware());
        }

        const backupConfigBtn = this.container.querySelector('#backup-config-btn');
        if (backupConfigBtn) {
            backupConfigBtn.addEventListener('click', () => this.handleBackupConfig());
        }
    }

    /**
     * Handle auto refresh toggle
     */
    handleAutoRefreshToggle() {
        console.log('[DASHBOARD-UI] Toggling auto refresh...');
        const icon = this.container.querySelector('#auto-refresh-icon');
        const text = this.container.querySelector('#auto-refresh-text');
        
        if (icon && text) {
            const isActive = icon.classList.contains('text-green-600');
            if (isActive) {
                icon.classList.remove('text-green-600');
                icon.classList.add('text-neutral-500');
                text.textContent = 'Auto';
            } else {
                icon.classList.remove('text-neutral-500');
                icon.classList.add('text-green-600');
                text.textContent = 'Auto ON';
            }
        }
        
        document.dispatchEvent(new CustomEvent('dashboard:toggleAutoRefresh'));
    }

    /**
     * Handle refresh data button
     */
    handleRefreshData() {
        console.log('[DASHBOARD-UI] Refreshing data...');
        const icon = this.container.querySelector('#refresh-icon');
        if (icon) {
            icon.classList.add('animate-spin');
            setTimeout(() => {
                icon.classList.remove('animate-spin');
            }, 1000);
        }
        document.dispatchEvent(new CustomEvent('dashboard:refreshData'));
    }

    /**
     * Handle cellular configuration action
     */
    handleCellularConfig() {
        console.log('[DASHBOARD-UI] Opening cellular configuration...');
        document.dispatchEvent(new CustomEvent('dashboard:cellularConfig'));
    }

    /**
     * Handle network scan action
     */
    handleNetworkScan() {
        console.log('[DASHBOARD-UI] Starting network scan...');
        document.dispatchEvent(new CustomEvent('dashboard:networkScan'));
    }

    /**
     * Handle firmware update action
     */
    handleUpdateFirmware() {
        console.log('[DASHBOARD-UI] Checking for firmware updates...');
        document.dispatchEvent(new CustomEvent('dashboard:updateFirmware'));
    }

    /**
     * Handle backup configuration action
     */
    handleBackupConfig() {
        console.log('[DASHBOARD-UI] Creating configuration backup...');
        document.dispatchEvent(new CustomEvent('dashboard:backupConfig'));
    }

    /**
     * Update dashboard data with system monitoring information
     */
    updateData(data) {
        console.log('[DASHBOARD-UI] Updating dashboard data:', data);
        
        if (!this.container) {
            console.error('[DASHBOARD-UI] Container not found for data update');
            return;
        }
        
        if (!data) {
            console.error('[DASHBOARD-UI] No data provided for update');
            return;
        }
        
        try {
            // Update system stats
            if (data.system) {
                console.log('[DASHBOARD-UI] Updating system stats:', data.system);
                this.updateSystemStats(data.system);
                // Hide loading state for system stats
                if (this.loadingUI) {
                    this.loadingUI.hideLoading('systemStats');
                }
            } else {
                console.warn('[DASHBOARD-UI] No system data provided');
            }
            
            // Update network status
            if (data.network) {
                console.log('[DASHBOARD-UI] Updating network status:', data.network);
                this.updateNetworkStatus(data.network);
                // Hide loading state for network status
                if (this.loadingUI) {
                    this.loadingUI.hideLoading('networkStatus');
                }
            } else {
                console.warn('[DASHBOARD-UI] No network data provided');
            }
            
            // Update cellular status
            if (data.cellular) {
                console.log('[DASHBOARD-UI] Updating cellular status:', data.cellular);
                this.updateCellularStatus(data.cellular);
                // Hide loading state for cellular status
                if (this.loadingUI) {
                    this.loadingUI.hideLoading('cellularStatus');
                }
            } else {
                console.warn('[DASHBOARD-UI] No cellular data provided');
            }
            
            // Update last login and update times
            if (data.lastLogin) {
                const lastLoginElement = this.container.querySelector('[data-last-login]');
                if (lastLoginElement) {
                    lastLoginElement.textContent = data.lastLogin;
                } else {
                    console.warn('[DASHBOARD-UI] Last login element not found');
                }
            }
            
            if (data.lastUpdate) {
                const lastUpdateElement = this.container.querySelector('[data-last-update]');
                if (lastUpdateElement) {
                    lastUpdateElement.textContent = data.lastUpdate;
                    // Hide loading state for update indicators
                    if (this.loadingUI) {
                        this.loadingUI.hideLoading('updateIndicators');
                    }
                } else {
                    console.warn('[DASHBOARD-UI] Last update element not found');
                }
            }
            
            console.log('[DASHBOARD-UI] Dashboard data update completed successfully');
            
        } catch (error) {
            console.error('[DASHBOARD-UI] Error updating dashboard data:', error);
            // Show error states if loading fails
            if (this.loadingUI) {
                this.loadingUI.showError('systemStats', error.message);
                this.loadingUI.showError('networkStatus', error.message);
                this.loadingUI.showError('cellularStatus', error.message);
            }
        }
    }

    /**
     * Update system statistics
     */
    updateSystemStats(system) {
        console.log('[DASHBOARD-UI] Updating system stats:', system);
        
        try {
            // CPU
            if (system.cpu) {
                console.log('[DASHBOARD-UI] Updating CPU data:', system.cpu);
                const cpuUsage = this.container.querySelector('[data-cpu-usage]');
                const cpuProgress = this.container.querySelector('[data-cpu-progress]');
                const cpuCores = this.container.querySelector('[data-cpu-cores]');
                const cpuTemp = this.container.querySelector('[data-cpu-temp]');
                const cpuBaseClock = this.container.querySelector('[data-cpu-base-clock]');
                const cpuBoostClock = this.container.querySelector('[data-cpu-boost-clock]');
                
                if (cpuUsage) {
                    cpuUsage.textContent = system.cpu.usage || '0%';
                } else {
                    console.warn('[DASHBOARD-UI] CPU usage element not found');
                }
                
                if (cpuProgress) {
                    const usage = system.cpu.usage || '0%';
                    cpuProgress.style.width = usage;
                    // Add color coding based on usage
                    if (usage.includes('%')) {
                        const numericUsage = parseInt(usage);
                        if (numericUsage > 80) {
                            cpuProgress.className = 'bg-red-500 h-2 rounded-full transition-all duration-300';
                        } else if (numericUsage > 60) {
                            cpuProgress.className = 'bg-yellow-500 h-2 rounded-full transition-all duration-300';
                        } else {
                            cpuProgress.className = 'bg-green-500 h-2 rounded-full transition-all duration-300';
                        }
                    }
                } else {
                    console.warn('[DASHBOARD-UI] CPU progress element not found');
                }
                
                if (cpuCores) cpuCores.textContent = system.cpu.cores || '0 Cores';
                if (cpuTemp) cpuTemp.textContent = system.cpu.temperature || '0°C';
                if (cpuBaseClock) cpuBaseClock.textContent = system.cpu.baseClock || '0 GHz';
                if (cpuBoostClock) cpuBoostClock.textContent = system.cpu.boostClock || '0 GHz';
            } else {
                console.warn('[DASHBOARD-UI] No CPU data provided');
            }
            
            // RAM
            if (system.ram) {
                console.log('[DASHBOARD-UI] Updating RAM data:', system.ram);
                const ramUsage = this.container.querySelector('[data-ram-usage]');
                const ramProgress = this.container.querySelector('[data-ram-progress]');
                const ramUsed = this.container.querySelector('[data-ram-used]');
                const ramTotal = this.container.querySelector('[data-ram-total]');
                const ramAvailable = this.container.querySelector('[data-ram-available]');
                const ramType = this.container.querySelector('[data-ram-type]');
                
                if (ramUsage) {
                    ramUsage.textContent = system.ram.usage || '0%';
                } else {
                    console.warn('[DASHBOARD-UI] RAM usage element not found');
                }
                
                if (ramProgress) {
                    const usage = system.ram.usage || '0%';
                    ramProgress.style.width = usage;
                    // Add color coding based on usage
                    if (usage.includes('%')) {
                        const numericUsage = parseInt(usage);
                        if (numericUsage > 80) {
                            ramProgress.className = 'bg-red-500 h-2 rounded-full transition-all duration-300';
                        } else if (numericUsage > 60) {
                            ramProgress.className = 'bg-yellow-500 h-2 rounded-full transition-all duration-300';
                        } else {
                            ramProgress.className = 'bg-green-500 h-2 rounded-full transition-all duration-300';
                        }
                    }
                } else {
                    console.warn('[DASHBOARD-UI] RAM progress element not found');
                }
                
                if (ramUsed) ramUsed.textContent = system.ram.used || '0 GB';
                if (ramTotal) ramTotal.textContent = system.ram.total || '0 GB';
                if (ramAvailable) ramAvailable.textContent = system.ram.available || '0 GB';
                if (ramType) ramType.textContent = system.ram.type || 'N/A';
            } else {
                console.warn('[DASHBOARD-UI] No RAM data provided');
            }
            
            // Swap
            if (system.swap) {
                console.log('[DASHBOARD-UI] Updating Swap data:', system.swap);
                const swapUsage = this.container.querySelector('[data-swap-usage]');
                const swapProgress = this.container.querySelector('[data-swap-progress]');
                const swapUsed = this.container.querySelector('[data-swap-used]');
                const swapTotal = this.container.querySelector('[data-swap-total]');
                const swapAvailable = this.container.querySelector('[data-swap-available]');
                const swapPriority = this.container.querySelector('[data-swap-priority]');
                
                if (swapUsage) {
                    swapUsage.textContent = system.swap.usage || '0%';
                } else {
                    console.warn('[DASHBOARD-UI] Swap usage element not found');
                }
                
                if (swapProgress) {
                    const usage = system.swap.usage || '0%';
                    swapProgress.style.width = usage;
                    // Add color coding based on usage
                    if (usage.includes('%')) {
                        const numericUsage = parseInt(usage);
                        if (numericUsage > 50) {
                            swapProgress.className = 'bg-red-500 h-2 rounded-full transition-all duration-300';
                        } else if (numericUsage > 25) {
                            swapProgress.className = 'bg-yellow-500 h-2 rounded-full transition-all duration-300';
                        } else {
                            swapProgress.className = 'bg-green-500 h-2 rounded-full transition-all duration-300';
                        }
                    }
                } else {
                    console.warn('[DASHBOARD-UI] Swap progress element not found');
                }
                
                if (swapUsed) swapUsed.textContent = system.swap.used || '0 MB';
                if (swapTotal) swapTotal.textContent = system.swap.total || '0 GB';
                if (swapAvailable) swapAvailable.textContent = system.swap.available || '0 GB';
                if (swapPriority) swapPriority.textContent = system.swap.priority || 'Normal';
            } else {
                console.warn('[DASHBOARD-UI] No Swap data provided');
            }
            
        } catch (error) {
            console.error('[DASHBOARD-UI] Error updating system stats:', error);
        }
    }

    /**
     * Update network status
     */
    updateNetworkStatus(network) {
        console.log('[DASHBOARD-UI] Updating network status:', network);
        
        try {
            // Internet
            if (network.internet) {
                console.log('[DASHBOARD-UI] Updating internet data:', network.internet);
                const internetIndicator = this.container.querySelector('[data-internet-indicator]');
                const internetStatus = this.container.querySelector('[data-internet-status]');
                const externalIp = this.container.querySelector('[data-external-ip]');
                const dnsPrimary = this.container.querySelector('[data-dns-primary]');
                const dnsSecondary = this.container.querySelector('[data-dns-secondary]');
                const latency = this.container.querySelector('[data-latency]');
                const bandwidth = this.container.querySelector('[data-bandwidth]');
                
                if (internetIndicator && network.internet.status) {
                    const statusClass = network.internet.status === 'Connected' ? 'bg-green-500' : 
                                       network.internet.status === 'Connecting' ? 'bg-yellow-500' : 'bg-red-500';
                    internetIndicator.className = `w-2 h-2 rounded-full ${statusClass}`;
                } else if (internetIndicator) {
                    console.warn('[DASHBOARD-UI] Internet indicator found but no status data');
                }
                
                if (internetStatus) internetStatus.textContent = network.internet.status || 'Unknown';
                if (externalIp) externalIp.textContent = network.internet.externalIp || 'N/A';
                if (dnsPrimary) dnsPrimary.textContent = network.internet.dnsPrimary || 'N/A';
                if (dnsSecondary) dnsSecondary.textContent = network.internet.dnsSecondary || 'N/A';
                if (latency) latency.textContent = network.internet.latency || '0 ms';
                if (bandwidth) bandwidth.textContent = network.internet.bandwidth || 'N/A';
            } else {
                console.warn('[DASHBOARD-UI] No internet data provided');
            }
            
            // Server
            if (network.server) {
                console.log('[DASHBOARD-UI] Updating server data:', network.server);
                const serverIndicator = this.container.querySelector('[data-server-indicator]');
                const serverStatus = this.container.querySelector('[data-server-status]');
                const serverHostname = this.container.querySelector('[data-server-hostname]');
                const serverPort = this.container.querySelector('[data-server-port]');
                const serverProtocol = this.container.querySelector('[data-server-protocol]');
                const lastPing = this.container.querySelector('[data-last-ping]');
                const sessionDuration = this.container.querySelector('[data-session-duration]');
                
                if (serverIndicator && network.server.status) {
                    const statusClass = network.server.status === 'Connected' ? 'bg-green-500' : 
                                       network.server.status === 'Connecting' ? 'bg-yellow-500' : 'bg-red-500';
                    serverIndicator.className = `w-2 h-2 rounded-full ${statusClass}`;
                } else if (serverIndicator) {
                    console.warn('[DASHBOARD-UI] Server indicator found but no status data');
                }
                
                if (serverStatus) serverStatus.textContent = network.server.status || 'Unknown';
                if (serverHostname) serverHostname.textContent = network.server.hostname || 'N/A';
                if (serverPort) serverPort.textContent = network.server.port || '0';
                if (serverProtocol) serverProtocol.textContent = network.server.protocol || 'N/A';
                if (lastPing) lastPing.textContent = network.server.lastPing || '0 ms';
                if (sessionDuration) sessionDuration.textContent = network.server.sessionDuration || 'N/A';
            } else {
                console.warn('[DASHBOARD-UI] No server data provided');
            }
            
            // Connection Type
            if (network.connection) {
                console.log('[DASHBOARD-UI] Updating connection data:', network.connection);
                const connectionType = this.container.querySelector('[data-connection-type]');
                const interfaceName = this.container.querySelector('[data-interface-name]');
                const macAddress = this.container.querySelector('[data-mac-address]');
                const localIp = this.container.querySelector('[data-local-ip]');
                const gatewayIp = this.container.querySelector('[data-gateway-ip]');
                const connectionSpeed = this.container.querySelector('[data-connection-speed]');
                
                if (connectionType) connectionType.textContent = network.connection.type || 'Unknown';
                if (interfaceName) interfaceName.textContent = network.connection.interface || 'N/A';
                if (macAddress) macAddress.textContent = network.connection.macAddress || 'N/A';
                if (localIp) localIp.textContent = network.connection.localIp || 'N/A';
                if (gatewayIp) gatewayIp.textContent = network.connection.gateway || 'N/A';
                if (connectionSpeed) connectionSpeed.textContent = network.connection.speed || 'N/A';
            } else {
                console.warn('[DASHBOARD-UI] No connection data provided');
            }
            
        } catch (error) {
            console.error('[DASHBOARD-UI] Error updating network status:', error);
        }
    }

    /**
     * Update cellular status
     */
    updateCellularStatus(cellular) {
        console.log('[DASHBOARD-UI] Updating cellular status:', cellular);
        
        try {
            // Signal Strength
            if (cellular.signal) {
                console.log('[DASHBOARD-UI] Updating cellular signal data:', cellular.signal);
                const signalBars = this.container.querySelector('[data-signal-bars]');
                const signalStatus = this.container.querySelector('[data-signal-status]');
                const rssi = this.container.querySelector('[data-rssi]');
                const rsrp = this.container.querySelector('[data-rsrp]');
                const rsrq = this.container.querySelector('[data-rsrq]');
                const sinr = this.container.querySelector('[data-sinr]');
                const cellId = this.container.querySelector('[data-cell-id]');
                
                if (signalBars && cellular.signal.bars !== undefined) {
                    const bars = signalBars.querySelectorAll('div');
                    bars.forEach((bar, index) => {
                        if (index < cellular.signal.bars) {
                            bar.classList.remove('bg-neutral-300');
                            bar.classList.add('bg-green-500');
                        } else {
                            bar.classList.remove('bg-green-500');
                            bar.classList.add('bg-neutral-300');
                        }
                    });
                } else if (signalBars) {
                    console.warn('[DASHBOARD-UI] Signal bars element found but no bars data');
                }
                
                if (signalStatus) signalStatus.textContent = cellular.signal.status || 'No Signal';
                if (rssi) rssi.textContent = cellular.signal.rssi || '0 dBm';
                if (rsrp) rsrp.textContent = cellular.signal.rsrp || '0 dBm';
                if (rsrq) rsrq.textContent = cellular.signal.rsrq || '0 dB';
                if (sinr) sinr.textContent = cellular.signal.sinr || '0 dB';
                if (cellId) cellId.textContent = cellular.signal.cellId || 'N/A';
            } else {
                console.warn('[DASHBOARD-UI] No cellular signal data provided');
            }
            
            // Connection Status
            if (cellular.connection) {
                console.log('[DASHBOARD-UI] Updating cellular connection data:', cellular.connection);
                const cellularIndicator = this.container.querySelector('[data-cellular-indicator]');
                const cellularStatus = this.container.querySelector('[data-cellular-status]');
                const networkName = this.container.querySelector('[data-network-name]');
                const technology = this.container.querySelector('[data-technology]');
                const band = this.container.querySelector('[data-band]');
                const apn = this.container.querySelector('[data-apn]');
                const dataUsage = this.container.querySelector('[data-data-usage]');
                
                if (cellularIndicator && cellular.connection.status) {
                    const statusClass = cellular.connection.status === 'Connected' ? 'bg-green-500' : 
                                       cellular.connection.status === 'Connecting' ? 'bg-yellow-500' : 'bg-neutral-400';
                    cellularIndicator.className = `w-2 h-2 rounded-full ${statusClass}`;
                } else if (cellularIndicator) {
                    console.warn('[DASHBOARD-UI] Cellular indicator found but no status data');
                }
                
                if (cellularStatus) cellularStatus.textContent = cellular.connection.status || 'Disconnected';
                if (networkName) networkName.textContent = cellular.connection.network || 'N/A';
                if (technology) technology.textContent = cellular.connection.technology || 'N/A';
                if (band) band.textContent = cellular.connection.band || 'N/A';
                if (apn) apn.textContent = cellular.connection.apn || 'N/A';
                if (dataUsage) dataUsage.textContent = cellular.connection.dataUsage || '0 MB';
            } else {
                console.warn('[DASHBOARD-UI] No cellular connection data provided');
            }
            
        } catch (error) {
            console.error('[DASHBOARD-UI] Error updating cellular status:', error);
        }
    }

    /**
     * Show loading state
     */
    showLoading() {
        if (this.container) {
            this.container.innerHTML = `
                <div class="flex items-center justify-center h-64">
                    <div class="text-center">
                        <div class="w-8 h-8 border-2 border-blue-600 border-t-transparent rounded-full animate-spin mx-auto mb-4"></div>
                        <p class="text-neutral-600">Loading dashboard...</p>
                    </div>
                </div>
            `;
        }
    }

    /**
     * Show error state
     */
    showError(message = 'Failed to load dashboard') {
        if (this.container) {
            this.container.innerHTML = `
                <div class="flex items-center justify-center h-64">
                    <div class="text-center">
                        <i class="fas fa-exclamation-triangle text-4xl text-red-500 mb-4"></i>
                        <p class="text-neutral-600">${message}</p>
                        <button class="mt-4 px-4 py-2 text-sm text-white bg-blue-600 rounded hover:bg-blue-700" onclick="location.reload()">
                            Try Again
                        </button>
                    </div>
                </div>
            `;
        }
    }

    /**
     * Destroy dashboard instance
     */
    destroy() {
        if (this.container) {
            this.container.innerHTML = '';
        }
        this.container = null;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = DashboardUI;
} else if (typeof window !== 'undefined') {
    window.DashboardUI = DashboardUI;
}

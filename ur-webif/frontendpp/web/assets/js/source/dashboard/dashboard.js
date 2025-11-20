/**
 * Dashboard Controller
 * Main dashboard logic and data management
 * Integrates with source UI and handles dashboard functionality
 */

class Dashboard {
    constructor(sourceUI) {
        this.sourceUI = sourceUI;
        this.dashboardUI = null;
        this.data = {
            system: {
                cpu: { usage: '0%', cores: '0 Cores', temperature: '0°C', baseClock: '0 GHz', boostClock: '0 GHz' },
                ram: { usage: '0%', used: '0 GB', total: '0 GB', available: '0 GB', type: 'N/A' },
                swap: { usage: '0%', used: '0 MB', total: '0 GB', available: '0 GB', priority: 'Normal' }
            },
            network: {
                internet: { status: 'Unknown', externalIp: 'N/A', dnsPrimary: 'N/A', dnsSecondary: 'N/A', latency: '0 ms', bandwidth: 'N/A' },
                server: { status: 'Unknown', hostname: 'N/A', port: '0', protocol: 'N/A', lastPing: '0 ms', sessionDuration: 'N/A' },
                connection: { type: 'Unknown', interface: 'N/A', macAddress: 'N/A', localIp: 'N/A', gateway: 'N/A', speed: 'N/A' }
            },
            cellular: {
                signal: { bars: 0, status: 'No Signal', rssi: '0 dBm', rsrp: '0 dBm', rsrq: '0 dB', sinr: '0 dB', cellId: 'N/A' },
                connection: { status: 'Disconnected', network: 'N/A', technology: 'N/A', band: 'N/A', apn: 'N/A', dataUsage: '0 MB' }
            },
            lastUpdate: new Date().toLocaleString(),
            lastLogin: 'Last login: ' + new Date().toLocaleString()
        };
        this.isLoading = false;
        this.init();
    }

    /**
     * Initialize dashboard
     */
    async init() {
        console.log('[DASHBOARD] Initializing dashboard...');
        
        // Create UI instance
        this.dashboardUI = new DashboardUI();
        
        // Set up event listeners
        this.setupEventListeners();
        
        // Load initial data
        await this.loadDashboardData();
        
        // Render dashboard
        this.render();
    }

    /**
     * Set up event listeners for dashboard interactions
     */
    setupEventListeners() {
        // Listen for dashboard UI events (system monitoring actions)
        document.addEventListener('dashboard:toggleAutoRefresh', () => {
            this.handleAutoRefreshToggle();
        });

        document.addEventListener('dashboard:refreshData', () => {
            this.handleRefreshData();
        });

        document.addEventListener('dashboard:cellularConfig', () => {
            this.handleCellularConfig();
        });

        document.addEventListener('dashboard:networkScan', () => {
            this.handleNetworkScan();
        });

        document.addEventListener('dashboard:updateFirmware', () => {
            this.handleUpdateFirmware();
        });

        document.addEventListener('dashboard:backupConfig', () => {
            this.handleBackupConfig();
        });

        // Listen for WebSocket messages that might update dashboard
        document.addEventListener('websocketMessage', (e) => {
            this.handleWebSocketMessage(e.detail);
        });
        
        // Listen for specific dashboard data updates from WebSocket
        document.addEventListener('dashboardDataUpdate', (e) => {
            this.handleDashboardDataUpdate(e.detail);
        });
    }

    /**
     * Load dashboard data from backend
     */
    async loadDashboardData() {
        this.isLoading = true;
        
        try {
            console.log('[DASHBOARD] Loading dashboard data...');
            
            // Load system monitoring data via HTTP API as fallback
            await this.loadSystemData();
            await this.loadNetworkData();
            await this.loadCellularData();
            
            console.log('[DASHBOARD] Dashboard data loaded successfully');
            
        } catch (error) {
            console.error('[DASHBOARD] Failed to load dashboard data:', error);
            // Keep default values on error
        } finally {
            this.isLoading = false;
        }
    }

    /**
     * Load system monitoring data
     */
    async loadSystemData() {
        try {
            // This would be replaced with actual API call
            // For now, keep default values - WebSocket will provide real data
            console.log('[DASHBOARD] System data will be provided by WebSocket');
        } catch (error) {
            console.error('[DASHBOARD] Failed to load system data:', error);
        }
    }

    /**
     * Load network data
     */
    async loadNetworkData() {
        try {
            // This would be replaced with actual API call
            // For now, keep default values - WebSocket will provide real data
            console.log('[DASHBOARD] Network data will be provided by WebSocket');
        } catch (error) {
            console.error('[DASHBOARD] Failed to load network data:', error);
        }
    }

    /**
     * Load cellular data
     */
    async loadCellularData() {
        try {
            // This would be replaced with actual API call
            // For now, keep default values - WebSocket will provide real data
            console.log('[DASHBOARD] Cellular data will be provided by WebSocket');
        } catch (error) {
            console.error('[DASHBOARD] Failed to load cellular data:', error);
        }
    }

    /**
     * Load projects data
     */
    async loadProjects() {
        try {
            // Mock data - replace with actual API call
            this.data.projects = [
                {
                    id: 1,
                    name: 'Web Application',
                    type: 'web',
                    status: 'active',
                    lastUpdated: '2 hours ago',
                    icon: 'fa-code',
                    color: 'blue'
                },
                {
                    id: 2,
                    name: 'Mobile App',
                    type: 'mobile',
                    status: 'in-progress',
                    lastUpdated: '5 hours ago',
                    icon: 'fa-mobile-alt',
                    color: 'green'
                },
                {
                    id: 3,
                    name: 'Analytics Dashboard',
                    type: 'analytics',
                    status: 'active',
                    lastUpdated: 'yesterday',
                    icon: 'fa-chart-line',
                    color: 'purple'
                }
            ];
        } catch (error) {
            console.error('[DASHBOARD] Failed to load projects:', error);
            this.data.projects = [];
        }
    }

    /**
     * Load statistics data
     */
    async loadStats() {
        try {
            // Mock data - replace with actual API call
            this.data.stats = {
                totalProjects: 12,
                activeTasks: 24,
                teamMembers: 8,
                storageUsed: '2.4GB'
            };
        } catch (error) {
            console.error('[DASHBOARD] Failed to load stats:', error);
            this.data.stats = {
                totalProjects: 0,
                activeTasks: 0,
                teamMembers: 0,
                storageUsed: '0GB'
            };
        }
    }

    /**
     * Load recent activity data
     */
    async loadActivity() {
        try {
            // Mock data - replace with actual API call
            this.data.activity = [
                {
                    id: 1,
                    type: 'file',
                    message: 'New file uploaded',
                    timestamp: '2 hours ago',
                    icon: 'fa-file',
                    color: 'blue'
                },
                {
                    id: 2,
                    type: 'task',
                    message: 'Task completed',
                    timestamp: '5 hours ago',
                    icon: 'fa-check',
                    color: 'green'
                },
                {
                    id: 3,
                    type: 'user',
                    message: 'Team member joined',
                    timestamp: 'Yesterday',
                    icon: 'fa-user',
                    color: 'purple'
                }
            ];
        } catch (error) {
            console.error('[DASHBOARD] Failed to load activity:', error);
            this.data.activity = [];
        }
    }

    /**
     * Safely update dashboard UI data
     */
    safeUpdateData(data = null) {
        if (this.dashboardUI && this.dashboardUI.updateData) {
            this.dashboardUI.updateData(data || this.data);
        } else {
            console.warn('[DASHBOARD] Dashboard UI not available for data update');
        }
    }

    /**
     * Render the dashboard
     */
    render() {
        console.log('[DASHBOARD] Rendering dashboard...');
        
        try {
            // Check if dashboardUI is initialized
            if (!this.dashboardUI) {
                console.error('[DASHBOARD] Dashboard UI not initialized');
                if (this.sourceUI && this.sourceUI.showError) {
                    this.sourceUI.showError('Dashboard UI failed to initialize');
                }
                return;
            }
            
            if (this.isLoading) {
                this.dashboardUI.showLoading();
                return;
            }

            // Render dashboard UI
            const success = this.dashboardUI.render('main-content');
            
            if (success) {
                // Update UI with data
                this.safeUpdateData();
                console.log('[DASHBOARD] Dashboard rendered successfully');
            } else {
                throw new Error('Failed to render dashboard UI');
            }
            
        } catch (error) {
            console.error('[DASHBOARD] Failed to render dashboard:', error);
            if (this.dashboardUI && this.dashboardUI.showError) {
                this.dashboardUI.showError('Failed to load dashboard');
            } else {
                console.error('[DASHBOARD] Dashboard UI not available for error display');
            }
        }
    }

    /**
        
    } catch (error) {
        console.error('[DASHBOARD] Failed to load dashboard data:', error);
        // Keep default values on error
    } finally {
        this.isLoading = false;
                }
        }
    }

    /**
     * Handle WebSocket messages
     */
    handleWebSocketMessage(message) {
        console.log('[DASHBOARD] Received WebSocket message:', message);
        
        // Handle backend dashboard_update messages with categories
        if (message.type === 'dashboard_update' && message.category) {
            const category = message.category;
            const data = message.data;
            
            console.log('[DASHBOARD] Processing backend category update:', category, data);
            
            switch (category) {
                case 'system':
                    // System category from backend contains ONLY CPU data
                    console.log('[DASHBOARD] Processing SYSTEM category (CPU data only)');
                    this.handleSystemUpdate({ cpu: data });
                    break;
                case 'cpu':
                    console.log('[DASHBOARD] Processing CPU category');
                    this.handleSystemUpdate({ cpu: data });
                    break;
                case 'ram':
                    console.log('[DASHBOARD] Processing RAM category');
                    this.handleSystemUpdate({ ram: data });
                    break;
                case 'swap':
                    console.log('[DASHBOARD] Processing SWAP category');
                    this.handleSystemUpdate({ swap: data });
                    break;
                case 'network':
                    console.log('[DASHBOARD] Processing NETWORK category');
                    this.handleNetworkUpdate(data);
                    break;
                case 'ultima_server':
                    console.log('[DASHBOARD] Processing ULTIMA_SERVER category');
                    this.handleNetworkUpdate({ server: data });
                    break;
                case 'signal':
                    console.log('[DASHBOARD] Processing SIGNAL category');
                    this.handleCellularUpdate(data);
                    break;
                default:
                    console.warn('[DASHBOARD] Unknown backend category:', category);
            }
            return;
        }
        
        // Handle other message types (legacy)
        switch (message.type) {
            case 'system_stats':
            case 'system_update':
                this.handleSystemUpdate(message.data);
                break;
            case 'network_status':
                this.handleNetworkUpdate(message.data);
                break;
            case 'cellular_status':
                this.handleCellularUpdate(message.data);
                break;
            case 'dashboard_update':
            case 'performance_data':
                this.handleFullDashboardUpdate(message.data);
                break;
            default:
                if (message.system || message.network || message.cellular || 
                    message.cpu || message.ram || message.swap || 
                    message.internet || message.server || message.connection || 
                    message.signal) {
                    this.handleFullDashboardUpdate(message);
                }
        }
    }

    /**
     * Handle dashboard data updates from WebSocket
     */
    handleDashboardDataUpdate(detail) {
        console.log('[DASHBOARD] Received dashboard data update:', detail);
        
        const { data, source } = detail;
        
        if (source === 'websocket') {
            this.handleFullDashboardUpdate(data);
        }
    }

    /**
     * Handle system updates
     */
    handleSystemUpdate(systemData) {
        console.log('[DASHBOARD] Handling system update:', systemData);
        
        // Update system data
        if (systemData.cpu) this.data.system.cpu = { ...this.data.system.cpu, ...systemData.cpu };
        if (systemData.ram) this.data.system.ram = { ...this.data.system.ram, ...systemData.ram };
        if (systemData.swap) this.data.system.swap = { ...this.data.system.swap, ...systemData.swap };
        
        // Update timestamp
        this.data.lastUpdate = new Date().toLocaleString();
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle network updates
     */
    handleNetworkUpdate(networkData) {
        console.log('[DASHBOARD] Handling network update:', networkData);
        
        // Update network data
        if (networkData.internet) this.data.network.internet = { ...this.data.network.internet, ...networkData.internet };
        if (networkData.server) this.data.network.server = { ...this.data.network.server, ...networkData.server };
        if (networkData.connection) this.data.network.connection = { ...this.data.network.connection, ...networkData.connection };
        
        // Update timestamp
        this.data.lastUpdate = new Date().toLocaleString();
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle cellular updates
     */
    handleCellularUpdate(cellularData) {
        console.log('[DASHBOARD] Handling cellular update:', cellularData);
        
        // Update cellular data
        if (cellularData.signal) this.data.cellular.signal = { ...this.data.cellular.signal, ...cellularData.signal };
        if (cellularData.connection) this.data.cellular.connection = { ...this.data.cellular.connection, ...cellularData.connection };
        
        // Update timestamp
        this.data.lastUpdate = new Date().toLocaleString();
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle full dashboard update
     */
    handleFullDashboardUpdate(data) {
        console.log('[DASHBOARD] Handling full dashboard update:', data);
        
        // Update all data sections
        if (data.system) {
            if (data.system.cpu) this.data.system.cpu = { ...this.data.system.cpu, ...data.system.cpu };
            if (data.system.ram) this.data.system.ram = { ...this.data.system.ram, ...data.system.ram };
            if (data.system.swap) this.data.system.swap = { ...this.data.system.swap, ...data.system.swap };
        }
        
        if (data.network) {
            if (data.network.internet) this.data.network.internet = { ...this.data.network.internet, ...data.network.internet };
            if (data.network.server) this.data.network.server = { ...this.data.network.server, ...data.network.server };
            if (data.network.connection) this.data.network.connection = { ...this.data.network.connection, ...data.network.connection };
        }
        
        if (data.cellular) {
            if (data.cellular.signal) this.data.cellular.signal = { ...this.data.cellular.signal, ...data.cellular.signal };
            if (data.cellular.connection) this.data.cellular.connection = { ...this.data.cellular.connection, ...data.cellular.connection };
        }
        
        // Handle direct data fields
        if (data.cpu) this.data.system.cpu = { ...this.data.system.cpu, ...data.cpu };
        if (data.ram) this.data.system.ram = { ...this.data.system.ram, ...data.ram };
        if (data.swap) this.data.system.swap = { ...this.data.system.swap, ...data.swap };
        if (data.internet) this.data.network.internet = { ...this.data.network.internet, ...data.internet };
        if (data.server) this.data.network.server = { ...this.data.network.server, ...data.server };
        if (data.connection) this.data.network.connection = { ...this.data.network.connection, ...data.connection };
        if (data.signal) this.data.cellular.signal = { ...this.data.cellular.signal, ...data.signal };
        
        // Update timestamp
        this.data.lastUpdate = data.lastUpdate || new Date().toLocaleString();
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle project updates
     */
    handleProjectUpdate(projectData) {
        console.log('[DASHBOARD] Handling project update:', projectData);
        
        // Update projects list
        const projectIndex = this.data.projects.findIndex(p => p.id === projectData.id);
        if (projectIndex !== -1) {
            this.data.projects[projectIndex] = { ...this.data.projects[projectIndex], ...projectData };
        } else {
            this.data.projects.unshift(projectData);
        }
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle task updates
     */
    handleTaskUpdate(taskData) {
        console.log('[DASHBOARD] Handling task update:', taskData);
        
        // Update stats
        this.data.stats.activeTasks = taskData.activeTasks || this.data.stats.activeTasks;
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle team updates
     */
    handleTeamUpdate(teamData) {
        console.log('[DASHBOARD] Handling team update:', teamData);
        
        // Update team member count
        this.data.stats.teamMembers = teamData.memberCount || this.data.stats.teamMembers;
        
        // Add to activity
        if (teamData.newMember) {
            this.data.activity.unshift({
                id: Date.now(),
                type: 'user',
                message: 'Team member joined',
                timestamp: 'Just now',
                icon: 'fa-user',
                color: 'purple'
            });
        }
        
        // Refresh UI
        this.safeUpdateData();
    }

    /**
     * Handle auto refresh toggle action
     */
    handleAutoRefreshToggle() {
        console.log('[DASHBOARD] Toggling auto refresh...');
        // This could be implemented to start/stop periodic data requests
        // For now, the UI handles the visual feedback
    }

    /**
     * Handle refresh data action
     */
    async handleRefreshData() {
        console.log('[DASHBOARD] Refreshing dashboard data...');
        try {
            await this.loadDashboardData();
            this.safeUpdateData();
        } catch (error) {
            console.error('[DASHBOARD] Failed to refresh data:', error);
        }
    }

    /**
     * Handle cellular configuration action
     */
    handleCellularConfig() {
        console.log('[DASHBOARD] Opening cellular configuration...');
        
        if (window.popupManager) {
            window.popupManager.showCustom({
                title: '',
                content: `
                    <div class="bg-white rounded-lg shadow-2xl w-[500px] max-w-[90vw]">
                        <div class="flex items-center justify-between px-6 py-4 border-b border-neutral-200">
                            <h2 class="text-xl text-neutral-900">Cellular Configuration</h2>
                            <button class="p-1 hover:bg-neutral-100 rounded-md transition-colors" onclick="window.popupManager.close()">
                                <i class="text-neutral-500 text-lg">✕</i>
                            </button>
                        </div>
                        <div class="px-6 py-6">
                            <form id="cellular-config-form" class="space-y-4">
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">APN Settings</label>
                                    <input type="text" name="apn" placeholder="Access Point Name" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500">
                                </div>
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Network Mode</label>
                                    <select name="mode" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500">
                                        <option value="auto">Automatic</option>
                                        <option value="5g">5G Only</option>
                                        <option value="4g">4G/LTE Only</option>
                                        <option value="3g">3G Only</option>
                                    </select>
                                </div>
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Roaming</label>
                                    <div class="flex items-center space-x-2">
                                        <input type="checkbox" name="roaming" class="rounded border-neutral-300">
                                        <span class="text-sm text-neutral-600">Enable data roaming</span>
                                    </div>
                                </div>
                            </form>
                        </div>
                        <div class="flex justify-end gap-3 px-6 py-4 border-t border-neutral-200">
                            <button class="px-4 py-2 text-sm text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50" onclick="window.popupManager.close()">
                                Cancel
                            </button>
                            <button class="px-4 py-2 text-sm text-white bg-blue-600 rounded-md hover:bg-blue-700" onclick="window.dashboard.submitCellularConfig()">
                                Save Configuration
                            </button>
                        </div>
                    </div>
                `,
                buttons: [],
                closeOnBackdrop: true,
                closeOnEscape: true
            });
        }
    }

    /**
     * Submit cellular configuration
     */
    submitCellularConfig() {
        const form = document.getElementById('cellular-config-form');
        if (form) {
            const formData = new FormData(form);
            const config = {
                apn: formData.get('apn'),
                mode: formData.get('mode'),
                roaming: formData.get('roaming') === 'on'
            };
            
            console.log('[DASHBOARD] Saving cellular config:', config);
            
            // Close popup
            if (window.popupManager) {
                window.popupManager.close();
            }
            
            // Show success message
            if (window.popupManager) {
                window.popupManager.showSuccess({
                    title: 'Configuration Saved',
                    message: 'Cellular configuration has been saved successfully.'
                });
            }
        }
    }

    /**
     * Handle network scan action
     */
    handleNetworkScan() {
        console.log('[DASHBOARD] Starting network scan...');
        
        if (window.popupManager) {
            window.popupManager.showInfo({
                title: 'Network Scan',
                message: 'Scanning network for devices...'
            });
            
            // Simulate network scan
            setTimeout(() => {
                if (window.popupManager) {
                    window.popupManager.showCustom({
                        title: '',
                        content: `
                            <div class="bg-white rounded-lg shadow-2xl w-[400px] max-w-[90vw]">
                                <div class="flex items-center justify-between px-6 py-4 border-b border-neutral-200">
                                    <h2 class="text-xl text-neutral-900">Network Scan Results</h2>
                                    <button class="p-1 hover:bg-neutral-100 rounded-md transition-colors" onclick="window.popupManager.close()">
                                        <i class="text-neutral-500 text-lg">✕</i>
                                    </button>
                                </div>
                                <div class="px-6 py-6">
                                    <div class="space-y-3">
                                        <div class="flex items-center justify-between p-3 bg-neutral-50 rounded">
                                            <div class="flex items-center space-x-3">
                                                <i class="fa-solid fa-desktop text-neutral-600"></i>
                                                <div>
                                                    <div class="text-sm font-medium text-neutral-900">Workstation-01</div>
                                                    <div class="text-xs text-neutral-500">192.168.1.100</div>
                                                </div>
                                            </div>
                                            <span class="px-2 py-1 text-xs text-green-700 bg-green-100 rounded">Online</span>
                                        </div>
                                        <div class="flex items-center justify-between p-3 bg-neutral-50 rounded">
                                            <div class="flex items-center space-x-3">
                                                <i class="fa-solid fa-mobile-alt text-neutral-600"></i>
                                                <div>
                                                    <div class="text-sm font-medium text-neutral-900">Mobile Device</div>
                                                    <div class="text-xs text-neutral-500">192.168.1.101</div>
                                                </div>
                                            </div>
                                            <span class="px-2 py-1 text-xs text-green-700 bg-green-100 rounded">Online</span>
                                        </div>
                                        <div class="flex items-center justify-between p-3 bg-neutral-50 rounded">
                                            <div class="flex items-center space-x-3">
                                                <i class="fa-solid fa-print text-neutral-600"></i>
                                                <div>
                                                    <div class="text-sm font-medium text-neutral-900">Office Printer</div>
                                                    <div class="text-xs text-neutral-500">192.168.1.102</div>
                                                </div>
                                            </div>
                                            <span class="px-2 py-1 text-xs text-yellow-700 bg-yellow-100 rounded">Idle</span>
                                        </div>
                                    </div>
                                </div>
                                <div class="flex justify-end px-6 py-4 border-t border-neutral-200">
                                    <button class="px-4 py-2 text-sm text-white bg-blue-600 rounded-md hover:bg-blue-700" onclick="window.popupManager.close()">
                                        Close
                                    </button>
                                </div>
                            </div>
                        `,
                        buttons: [],
                        closeOnBackdrop: true,
                        closeOnEscape: true
                    });
                }
            }, 2000);
        }
    }

    /**
     * Handle firmware update action
     */
    handleUpdateFirmware() {
        console.log('[DASHBOARD] Checking for firmware updates...');
        
        if (window.popupManager) {
            window.popupManager.showCustom({
                title: '',
                content: `
                    <div class="bg-white rounded-lg shadow-2xl w-[400px] max-w-[90vw]">
                        <div class="flex items-center justify-between px-6 py-4 border-b border-neutral-200">
                            <h2 class="text-xl text-neutral-900">Firmware Update</h2>
                            <button class="p-1 hover:bg-neutral-100 rounded-md transition-colors" onclick="window.popupManager.close()">
                                <i class="text-neutral-500 text-lg">✕</i>
                            </button>
                        </div>
                        <div class="px-6 py-6">
                            <div class="text-center">
                                <div class="w-12 h-12 bg-blue-100 rounded-full flex items-center justify-center mx-auto mb-4">
                                    <i class="fa-solid fa-download text-blue-600"></i>
                                </div>
                                <h3 class="text-lg text-neutral-900 mb-2">Check for Updates</h3>
                                <p class="text-sm text-neutral-600 mb-4">
                                    Current version: v2.1.0<br>
                                    Checking for available updates...
                                </p>
                                <div class="w-full bg-neutral-200 rounded-full h-2 mb-4">
                                    <div class="bg-blue-600 h-2 rounded-full" style="width: 75%"></div>
                                </div>
                            </div>
                        </div>
                        <div class="flex justify-end gap-3 px-6 py-4 border-t border-neutral-200">
                            <button class="px-4 py-2 text-sm text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50" onclick="window.popupManager.close()">
                                Cancel
                            </button>
                            <button class="px-4 py-2 text-sm text-white bg-blue-600 rounded-md hover:bg-blue-700" onclick="window.dashboard.simulateFirmwareUpdate()">
                                Update Now
                            </button>
                        </div>
                    </div>
                `,
                buttons: [],
                closeOnBackdrop: true,
                closeOnEscape: true
            });
        }
    }

    /**
     * Simulate firmware update
     */
    simulateFirmwareUpdate() {
        if (window.popupManager) {
            window.popupManager.close();
            
            window.popupManager.showInfo({
                title: 'Updating Firmware',
                message: 'Downloading and installing firmware update...'
            });
            
            setTimeout(() => {
                if (window.popupManager) {
                    window.popupManager.showSuccess({
                        title: 'Update Complete',
                        message: 'Firmware has been successfully updated to v2.2.0'
                    });
                }
            }, 3000);
        }
    }

    /**
     * Handle backup configuration action
     */
    handleBackupConfig() {
        console.log('[DASHBOARD] Creating configuration backup...');
        
        if (window.popupManager) {
            window.popupManager.showInfo({
                title: 'Creating Backup',
                message: 'Generating configuration backup...'
            });
            
            setTimeout(() => {
                // Create backup data
                const backupData = {
                    timestamp: new Date().toISOString(),
                    system: this.data.system,
                    network: this.data.network,
                    cellular: this.data.cellular,
                    version: '2.1.0'
                };
                
                // Create and download backup file
                const blob = new Blob([JSON.stringify(backupData, null, 2)], { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `ultima-backup-${new Date().toISOString().split('T')[0]}.json`;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                
                if (window.popupManager) {
                    window.popupManager.showSuccess({
                        title: 'Backup Complete',
                        message: 'Configuration backup has been created and downloaded.'
                    });
                }
            }, 1500);
        }
    }

    /**
     * Refresh dashboard data
     */
    async refresh() {
        console.log('[DASHBOARD] Refreshing dashboard...');
        await this.loadDashboardData();
        this.render();
    }

    /**
     * Destroy dashboard instance
     */
    destroy() {
        console.log('[DASHBOARD] Destroying dashboard...');
        
        // Remove event listeners
        document.removeEventListener('dashboard:toggleAutoRefresh', this.handleAutoRefreshToggle);
        document.removeEventListener('dashboard:refreshData', this.handleRefreshData);
        document.removeEventListener('dashboard:cellularConfig', this.handleCellularConfig);
        document.removeEventListener('dashboard:networkScan', this.handleNetworkScan);
        document.removeEventListener('dashboard:updateFirmware', this.handleUpdateFirmware);
        document.removeEventListener('dashboard:backupConfig', this.handleBackupConfig);
        document.removeEventListener('websocketMessage', this.handleWebSocketMessage);
        document.removeEventListener('dashboardDataUpdate', this.handleDashboardDataUpdate);
        
        // Destroy UI
        if (this.dashboardUI) {
            this.dashboardUI.destroy();
        }
        
        // Clear references
        this.dashboardUI = null;
        this.sourceUI = null;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = Dashboard;
} else if (typeof window !== 'undefined') {
    window.Dashboard = Dashboard;
}

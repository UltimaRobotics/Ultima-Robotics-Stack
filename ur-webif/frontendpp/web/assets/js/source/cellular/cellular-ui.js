/**
 * Cellular Configuration UI Controller
 * Handles all UI rendering and interactions for cellular network configuration
 */

import { CellularManager } from './cellular.js';

export class CellularUI {
    constructor(httpClient, container) {
        this.httpClient = httpClient;
        this.container = container;
        this.cellularManager = new CellularManager(httpClient);
        this.expandedSections = new Set(['cellularConfig']);
        this.dataUpdateInterval = null;
        
        this.init();
    }
    
    init() {
        this.render();
        this.setupEventListeners();
        this.startDataUpdates();
    }
    
    /**
     * Render the complete cellular configuration UI
     */
    render() {
        const content = this.getCellularConfigHTML();
        this.container.innerHTML = content;
        
        // Initialize UI state
        this.initializeUIState();
        this.updateConnectionStatus();
        this.updateSIMStatus();
    }
    
    /**
     * Get the complete HTML for cellular configuration
     */
    getCellularConfigHTML() {
        const isCellularEnabled = this.cellularManager.isCellularInterfaceEnabled();
        const connectionStatus = this.cellularManager.getConnectionStatus();
        const config = this.cellularManager.getConfiguration();
        const simInfo = this.cellularManager.getSimInfo();
        const deviceInfo = this.cellularManager.getDeviceInfo();
        
        return `
            <div class="max-w-5xl mx-auto space-y-4">
                <!-- Cellular Interface Section -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200">
                    <div class="p-4 flex items-center justify-between">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-signal text-gray-400"></i>
                            <div>
                                <h3 class="font-medium text-gray-900">Cellular Interface</h3>
                                <p class="text-xs text-gray-500">Enable or disable Cellular functionality</p>
                            </div>
                        </div>
                        <label class="relative inline-flex items-center cursor-pointer">
                            <input type="checkbox" id="cellularToggle" class="sr-only peer" ${isCellularEnabled ? 'checked' : ''}>
                            <div class="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-blue-300 rounded-full peer peer-checked:after:translate-x-full rtl:peer-checked:after:-translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-600"></div>
                        </label>
                    </div>
                </div>

                <!-- Connection Status Section -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200">
                    <div class="p-4 border-b border-gray-200 flex items-center justify-between cursor-pointer" onclick="window.cellularUI.toggleSection('connectionStatus')">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-wifi text-gray-400"></i>
                            <div>
                                <h3 class="font-medium text-gray-900">Connection Status</h3>
                                <p id="connectionStatusText" class="text-xs text-gray-500">${this.getStatusText(connectionStatus.status)}</p>
                            </div>
                        </div>
                        <div class="flex items-center gap-3">
                            <span id="connectionBadge" class="px-2 py-1 text-xs font-medium rounded-full ${this.getStatusBadgeClass(connectionStatus.status)} flex items-center gap-1">
                                <span class="w-2 h-2 ${this.getStatusDotClass(connectionStatus.status)} rounded-full"></span>
                                ${this.getStatusText(connectionStatus.status)}
                            </span>
                            <i id="connectionStatusChevron" class="fas fa-chevron-${this.expandedSections.has('connectionStatus') ? 'up' : 'down'} text-gray-400 transition-transform"></i>
                        </div>
                    </div>
                    
                    <!-- Connection Status Content -->
                    <div id="connectionStatusContent" class="${this.expandedSections.has('connectionStatus') ? 'block' : 'hidden'} p-4">
                        ${this.getConnectionStatusContent(connectionStatus)}
                    </div>
                </div>

                <!-- Cellular Configuration Section -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200">
                    <div class="p-4 border-b border-gray-200 flex items-center justify-between cursor-pointer" onclick="window.cellularUI.toggleSection('cellularConfig')">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-cog text-gray-400"></i>
                            <h3 class="font-medium text-gray-900">Cellular Configuration</h3>
                        </div>
                        <div class="flex items-center gap-3">
                            <span class="px-2 py-1 text-xs font-medium rounded bg-blue-100 text-blue-700">Profile: Default</span>
                            <i id="cellularConfigChevron" class="fas fa-chevron-${this.expandedSections.has('cellularConfig') ? 'up' : 'down'} text-gray-400 transition-transform"></i>
                        </div>
                    </div>
                    
                    <div id="cellularConfigContent" class="${this.expandedSections.has('cellularConfig') ? 'block' : 'hidden'} p-4">
                        ${this.getConfigurationContent(config)}
                    </div>
                </div>

                <!-- SIM & Device Information Section -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200">
                    <div class="p-4 border-b border-gray-200 flex items-center justify-between cursor-pointer" onclick="window.cellularUI.toggleSection('simInfo')">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-sim-card text-gray-400"></i>
                            <h3 class="font-medium text-gray-900">SIM & Device Information</h3>
                        </div>
                        <div class="flex items-center gap-3">
                            <span id="simBadge" class="px-2 py-1 text-xs font-medium rounded-full ${simInfo.available ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'} flex items-center gap-1">
                                <span class="w-2 h-2 ${simInfo.available ? 'bg-green-600' : 'bg-red-600'} rounded-full"></span>
                                ${simInfo.available ? 'SIM Available' : 'SIM Not Available'}
                            </span>
                            <i id="simInfoChevron" class="fas fa-chevron-${this.expandedSections.has('simInfo') ? 'up' : 'down'} text-gray-400 transition-transform"></i>
                        </div>
                    </div>
                    
                    <div id="simInfoContent" class="${this.expandedSections.has('simInfo') ? 'block' : 'hidden'} p-4">
                        ${this.getSIMInfoContent(simInfo, deviceInfo)}
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get connection status content HTML
     */
    getConnectionStatusContent(connectionStatus) {
        if (connectionStatus.status === 'disconnected') {
            return `
                <div class="space-y-4">
                    <div class="grid grid-cols-4 gap-4 text-sm">
                        <div>
                            <p class="text-gray-500 mb-1">Network</p>
                            <p class="text-gray-900 font-medium">No network selected</p>
                        </div>
                        <div>
                            <p class="text-gray-500 mb-1">Signal strength</p>
                            <p class="text-gray-900 font-medium">Unavailable</p>
                        </div>
                        <div>
                            <p class="text-gray-500 mb-1">Last connection attempt</p>
                            <p class="text-gray-900 font-medium">Not attempted</p>
                        </div>
                        <div>
                            <p class="text-gray-500 mb-1">Reason</p>
                            <p class="text-gray-900 font-medium">Interface disabled or no SIM</p>
                        </div>
                    </div>
                    <div class="bg-blue-50 border border-blue-100 rounded-lg p-3 text-sm text-gray-700">
                        Enable the cellular interface and insert a SIM to establish a connection.
                    </div>
                    <div class="flex justify-end">
                        <button onclick="window.cellularUI.retryConnection()" class="px-4 py-2 bg-blue-600 text-white text-sm font-medium rounded-lg hover:bg-blue-700 transition">
                            Retry connection
                        </button>
                    </div>
                </div>
            `;
        } else if (connectionStatus.status === 'connecting') {
            return `
                <div class="space-y-4">
                    <div class="text-center py-8">
                        <div class="inline-block">
                            <i class="fas fa-spinner fa-spin text-4xl text-blue-600"></i>
                        </div>
                        <p class="text-gray-600 mt-4 loading-dots">Loading latest connection information...</p>
                    </div>
                    <div class="grid grid-cols-4 gap-4">
                        <div>
                            <p class="text-xs text-gray-500 mb-2">NETWORK</p>
                            <div class="h-4 bg-gray-200 rounded skeleton"></div>
                        </div>
                        <div>
                            <p class="text-xs text-gray-500 mb-2">SIGNAL</p>
                            <div class="h-4 bg-gray-200 rounded skeleton"></div>
                        </div>
                        <div>
                            <p class="text-xs text-gray-500 mb-2">CONNECTION</p>
                            <div class="h-4 bg-gray-200 rounded skeleton"></div>
                        </div>
                        <div>
                            <p class="text-xs text-gray-500 mb-2">DATA USAGE</p>
                            <div class="h-4 bg-gray-200 rounded skeleton"></div>
                        </div>
                    </div>
                    <p class="text-sm text-gray-500 text-center">Connection details will appear once loading is complete.</p>
                </div>
            `;
        } else {
            // Connected status
            const signal = this.cellularManager.signalStrength;
            const dataUsage = this.cellularManager.getDataUsage();
            return `
                <div class="space-y-4">
                    <div class="grid grid-cols-3 gap-4">
                        <!-- Network Information -->
                        <div class="bg-gray-50 p-4 rounded-lg">
                            <h4 class="text-xs font-semibold text-gray-700 mb-3">Network Information</h4>
                            <div class="space-y-2 text-sm">
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Network</span>
                                    <span class="font-medium text-gray-900">${connectionStatus.network?.technology} - ${connectionStatus.network?.operator}</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">IP address</span>
                                    <span class="font-medium text-gray-900">${connectionStatus.network?.ipAddress || 'N/A'}</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Roaming</span>
                                    <span class="font-medium text-gray-900">${connectionStatus.network?.roaming ? 'On' : 'Off'}</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">APN</span>
                                    <span class="font-medium text-gray-900">${connectionStatus.network?.apn || 'N/A'}</span>
                                </div>
                            </div>
                        </div>

                        <!-- Signal Quality -->
                        <div class="bg-gray-50 p-4 rounded-lg">
                            <h4 class="text-xs font-semibold text-gray-700 mb-3">Signal quality</h4>
                            <div class="flex items-center gap-2 mb-3">
                                <div class="flex items-end gap-0.5">
                                    ${this.getSignalBars(signal?.bars || 0)}
                                </div>
                            </div>
                            <div class="space-y-2 text-sm">
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Status</span>
                                    <span class="font-medium text-gray-900">${signal?.status || 'Unknown'}</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">RSSI</span>
                                    <span class="font-medium text-gray-900">${signal?.rssi ? signal.rssi + ' dBm' : 'N/A'}</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">RSRP</span>
                                    <span class="font-medium text-gray-900">${signal?.rsrp ? signal.rsrp + ' dBm' : 'N/A'}</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">RSRQ</span>
                                    <span class="font-medium text-gray-900">${signal?.rsrq ? signal.rsrq + ' dB' : 'N/A'}</span>
                                </div>
                            </div>
                        </div>

                        <!-- Connection Details -->
                        <div class="bg-gray-50 p-4 rounded-lg">
                            <h4 class="text-xs font-semibold text-gray-700 mb-3">Connection details</h4>
                            <div class="space-y-2 text-sm">
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Status</span>
                                    <span class="font-medium text-green-600">Connected</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Uptime</span>
                                    <span class="font-medium text-gray-900">00:12:34</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Last event</span>
                                    <span class="font-medium text-gray-900">Connected 2 minutes ago</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-gray-600">Data usage</span>
                                    <span class="font-medium text-gray-900">${dataUsage.session?.toFixed(1) || '0.0'} MB this session</span>
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="bg-green-50 border border-green-100 rounded-lg p-3 text-sm text-gray-700">
                        Connection is active. Use the controls to manage connection state and data.
                    </div>

                    <div class="flex justify-end gap-2">
                        <button onclick="window.cellularUI.disconnectNetwork()" class="px-4 py-2 bg-red-600 text-white text-sm font-medium rounded-lg hover:bg-red-700 transition">
                            Disconnect
                        </button>
                        <button onclick="window.cellularUI.refreshStatus()" class="px-4 py-2 bg-white border border-gray-300 text-gray-700 text-sm font-medium rounded-lg hover:bg-gray-50 transition">
                            Refresh Status
                        </button>
                        <button onclick="window.cellularUI.resetData()" class="px-4 py-2 bg-white border border-gray-300 text-gray-700 text-sm font-medium rounded-lg hover:bg-gray-50 transition">
                            Reset Data
                        </button>
                    </div>
                </div>
            `;
        }
    }
    
    /**
     * Get configuration content HTML
     */
    getConfigurationContent(config) {
        return `
            <p class="text-sm text-gray-600 mb-4">Configure network mode, APN settings, roaming, and data limits.</p>
            
            <div class="grid grid-cols-2 gap-6">
                <!-- Network Configuration -->
                <div class="bg-gray-50 p-4 rounded-lg">
                    <h4 class="text-sm font-semibold text-gray-900 mb-4">Network</h4>
                    
                    <div class="space-y-4">
                        <div>
                            <label class="block text-sm text-gray-700 mb-2">Network type</label>
                            <select id="networkType" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                <option value="auto" ${config.networkType === 'auto' ? 'selected' : ''}>Auto (5G / LTE / 3G)</option>
                                <option value="lte" ${config.networkType === 'lte' ? 'selected' : ''}>LTE only</option>
                                <option value="3g" ${config.networkType === '3g' ? 'selected' : ''}>3G only</option>
                                <option value="5g" ${config.networkType === '5g' ? 'selected' : ''}>5G only</option>
                            </select>
                            <p class="text-xs text-gray-500 mt-1">Choose the highest available technology for this SIM.</p>
                        </div>

                        <div>
                            <label class="block text-sm text-gray-700 mb-2">Network selection</label>
                            <div class="flex gap-2">
                                <select id="networkSelection" class="flex-1 px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                    <option value="automatic" ${config.networkSelection === 'automatic' ? 'selected' : ''}>Automatic</option>
                                    <option value="manual" ${config.networkSelection === 'manual' ? 'selected' : ''}>Manual</option>
                                </select>
                                <button onclick="window.cellularUI.scanNetworks()" class="px-4 py-2 bg-gray-200 text-gray-700 text-sm font-medium rounded-lg hover:bg-gray-300 transition">
                                    Scan networks
                                </button>
                            </div>
                            <p class="text-xs text-gray-500 mt-1">Switch to manual to pick a specific operator.</p>
                        </div>

                        <div id="availableNetworks" class="${config.networkSelection === 'manual' ? 'block' : 'hidden'}">
                            <p class="text-xs font-semibold text-gray-700 mb-2">Available networks</p>
                            <p class="text-xs text-gray-500 mb-2">Click "Scan networks" to find available operators.</p>
                            
                            <!-- Network List Table -->
                            <div class="mt-2 border border-gray-200 rounded-lg overflow-hidden">
                                <table class="min-w-full divide-y divide-gray-200 text-xs">
                                    <thead class="bg-gray-50">
                                        <tr>
                                            <th class="px-3 py-2 text-left font-semibold text-gray-700">Operator</th>
                                            <th class="px-3 py-2 text-left font-semibold text-gray-700">Technology</th>
                                            <th class="px-3 py-2 text-left font-semibold text-gray-700">Signal</th>
                                            <th class="px-3 py-2 text-left font-semibold text-gray-700">Status</th>
                                        </tr>
                                    </thead>
                                    <tbody class="bg-white divide-y divide-gray-200" id="networkListBody">
                                        <!-- Will be populated by JS -->
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- APN Settings & Other Options -->
                <div class="space-y-4">
                    <!-- APN Settings -->
                    <div class="bg-gray-50 p-4 rounded-lg">
                        <h4 class="text-sm font-semibold text-gray-900 mb-4">APN settings</h4>
                        
                        <div class="space-y-3">
                            <div>
                                <label class="block text-sm text-gray-700 mb-1">Access Point Name (APN)</label>
                                <input type="text" id="apnName" placeholder="Internet" value="${config.apn.name}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                <p class="text-xs text-gray-500 mt-1">Select the APN provided by your cellular operator.</p>
                            </div>

                            <div>
                                <label class="block text-sm text-gray-700 mb-1">Authentication type</label>
                                <select id="authType" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                    <option value="none" ${config.apn.authType === 'none' ? 'selected' : ''}>None</option>
                                    <option value="pap" ${config.apn.authType === 'pap' ? 'selected' : ''}>PAP</option>
                                    <option value="chap" ${config.apn.authType === 'chap' ? 'selected' : ''}>CHAP</option>
                                </select>
                                <p class="text-xs text-gray-500 mt-1">Use PAP or CHAP only if your operator requires credentials.</p>
                            </div>

                            <div class="grid grid-cols-2 gap-3">
                                <div>
                                    <label class="block text-sm text-gray-700 mb-1">Username <span class="text-gray-400">Optional</span></label>
                                    <input type="text" id="username" placeholder="Optional" value="${config.apn.username}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                </div>
                                <div>
                                    <label class="block text-sm text-gray-700 mb-1">Password <span class="text-gray-400">Optional</span></label>
                                    <input type="password" id="password" placeholder="Optional" value="${config.apn.password}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Auto Connect & Roaming -->
                    <div class="bg-gray-50 p-4 rounded-lg">
                        <h4 class="text-sm font-semibold text-gray-900 mb-4">Auto connect & roaming</h4>
                        
                        <div class="space-y-3">
                            <div class="flex items-center justify-between">
                                <div>
                                    <p class="text-sm text-gray-900">Auto connect on startup</p>
                                    <p class="text-xs text-gray-500">Automatically connect when the device powers on.</p>
                                </div>
                                <label class="relative inline-flex items-center cursor-pointer">
                                    <input type="checkbox" id="autoConnect" class="sr-only peer" ${config.autoConnect ? 'checked' : ''}>
                                    <div class="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-blue-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-600"></div>
                                </label>
                            </div>

                            <div class="flex items-center justify-between">
                                <div>
                                    <p class="text-sm text-gray-900">Data roaming</p>
                                    <p class="text-xs text-gray-500">Allow data usage when roaming outside home network.</p>
                                </div>
                                <label class="relative inline-flex items-center cursor-pointer">
                                    <input type="checkbox" id="dataRoaming" class="sr-only peer" ${config.dataRoaming ? 'checked' : ''}>
                                    <div class="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-blue-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-600"></div>
                                </label>
                            </div>
                        </div>
                    </div>

                    <!-- Data Limit -->
                    <div class="bg-gray-50 p-4 rounded-lg">
                        <h4 class="text-sm font-semibold text-gray-900 mb-4">Data limit</h4>
                        
                        <div class="space-y-3">
                            <div class="flex items-center justify-between">
                                <div>
                                    <p class="text-sm text-gray-900">Enable data limit</p>
                                    <p class="text-xs text-gray-500">Set a monthly data cap for this SIM.</p>
                                </div>
                                <label class="relative inline-flex items-center cursor-pointer">
                                    <input type="checkbox" id="dataLimitToggle" class="sr-only peer" ${config.dataLimit.enabled ? 'checked' : ''} onchange="window.cellularUI.toggleDataLimit()">
                                    <div class="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-blue-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-blue-600"></div>
                                </label>
                            </div>

                            <div id="dataLimitInput" class="${config.dataLimit.enabled ? 'block' : 'hidden'}">
                                <label class="block text-sm text-gray-700 mb-1">Data limit (MB)</label>
                                <input type="number" id="dataLimit" value="${config.dataLimit.limit}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent">
                                <p class="text-xs text-gray-500 mt-1">Enter 0 to keep data usage unlimited.</p>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Apply Configuration -->
            <div class="mt-6 pt-4 border-t border-gray-200 flex items-center justify-between">
                <p class="text-sm text-gray-600">Changes take effect on the next connection attempt.</p>
                <div class="flex gap-2">
                    <button onclick="window.cellularUI.resetToDefaults()" class="px-4 py-2 bg-white border border-gray-300 text-gray-700 text-sm font-medium rounded-lg hover:bg-gray-50 transition">
                        Reset to defaults
                    </button>
                    <button onclick="window.cellularUI.saveConfiguration()" class="px-4 py-2 bg-blue-600 text-white text-sm font-medium rounded-lg hover:bg-blue-700 transition">
                        Save & reconnect
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Get SIM info content HTML
     */
    getSIMInfoContent(simInfo, deviceInfo) {
        return `
            <p class="text-sm text-gray-600 mb-4">View SIM slots, identifiers, and device modem details.</p>
            
            <div class="grid grid-cols-2 gap-6">
                <!-- SIM Status -->
                <div class="bg-gray-50 p-4 rounded-lg">
                    <h4 class="text-xs font-semibold text-gray-700 mb-3">SIM status</h4>
                    <div class="space-y-2 text-sm">
                        <div class="flex justify-between">
                            <span class="text-gray-600">Current state</span>
                            <span class="font-medium text-gray-900">${simInfo.available ? 'SIM detected' : 'No SIM detected'}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Last SIM operator</span>
                            <span class="font-medium text-gray-900">${simInfo.operator || '—'}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Last SIM ICCID</span>
                            <span class="font-medium text-gray-900">${simInfo.iccid || '—'}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Last seen</span>
                            <span class="font-medium text-gray-900">${simInfo.lastSeen || 'Not available'}</span>
                        </div>
                    </div>
                </div>

                <!-- Device Identifiers -->
                <div class="bg-gray-50 p-4 rounded-lg">
                    <h4 class="text-xs font-semibold text-gray-700 mb-3">Device identifiers</h4>
                    <div class="space-y-2 text-sm">
                        <div class="flex justify-between">
                            <span class="text-gray-600">Device name</span>
                            <span class="font-medium text-gray-900">${deviceInfo.name}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Model</span>
                            <span class="font-medium text-gray-900">${deviceInfo.model}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Serial number</span>
                            <span class="font-medium text-gray-900">${deviceInfo.serialNumber}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">IMEI</span>
                            <span class="font-medium text-gray-900">${deviceInfo.imei || 'Not available without SIM'}</span>
                        </div>
                    </div>
                </div>
            </div>

            <!-- SIM Slots -->
            <div class="mt-6 grid grid-cols-2 gap-6">
                <div class="bg-gray-50 p-4 rounded-lg">
                    <h4 class="text-xs font-semibold text-gray-700 mb-3">SIM slots</h4>
                    <div class="space-y-3">
                        ${simInfo.slots.map(slot => `
                            <div class="flex justify-between items-center">
                                <span class="text-sm text-gray-600">Slot ${slot.id}</span>
                                <div class="flex items-center gap-2">
                                    ${slot.primary ? '<span class="px-2 py-1 text-xs font-medium rounded bg-blue-100 text-blue-700">Primary</span>' : ''}
                                    <span class="text-sm font-medium text-gray-900">${slot.status === 'occupied' ? 'Occupied' : 'Empty'}</span>
                                </div>
                            </div>
                        `).join('')}
                    </div>
                    <p class="text-xs text-gray-500 mt-3">Insert a SIM card into any slot to view its details and identifiers here.</p>
                </div>

                <div class="bg-gray-50 p-4 rounded-lg">
                    <h4 class="text-xs font-semibold text-gray-700 mb-3">Modem & firmware</h4>
                    <div class="space-y-2 text-sm">
                        <div class="flex justify-between">
                            <span class="text-gray-600">Modem chipset</span>
                            <span class="font-medium text-gray-900">${deviceInfo.modemChipset}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Firmware version</span>
                            <span class="font-medium text-gray-900">${deviceInfo.firmwareVersion}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Bootloader</span>
                            <span class="font-medium text-gray-900">${deviceInfo.bootloader}</span>
                        </div>
                        <div class="flex justify-between">
                            <span class="text-gray-600">Hardware revision</span>
                            <span class="font-medium text-gray-900">${deviceInfo.hardwareRevision}</span>
                        </div>
                    </div>
                </div>
            </div>

            <!-- PIN Management -->
            <div class="mt-6 bg-gray-50 p-4 rounded-lg">
                <div class="flex justify-between items-center mb-2">
                    <h4 class="text-sm font-semibold text-gray-900">PIN management</h4>
                    <button class="px-3 py-1 text-xs font-medium text-gray-500 border border-gray-300 rounded hover:bg-white transition" ${!simInfo.available ? 'disabled' : ''}>
                        ${simInfo.available ? 'Manage PIN' : 'Unavailable - no SIM'}
                    </button>
                </div>
                <p class="text-xs text-gray-500">Enable/disable SIM PIN and change the current PIN when a SIM is present.</p>
            </div>

            <!-- PUK Unlock -->
            <div class="mt-4 bg-gray-50 p-4 rounded-lg">
                <div class="flex justify-between items-center mb-2">
                    <h4 class="text-sm font-semibold text-gray-900">PUK unlock</h4>
                    <button class="px-3 py-1 text-xs font-medium text-gray-500 border border-gray-300 rounded hover:bg-white transition" ${!simInfo.available ? 'disabled' : ''}>
                        ${simInfo.available ? 'Unlock with PUK' : 'Unavailable - no SIM'}
                    </button>
                </div>
                <p class="text-xs text-gray-500">Use the PUK code from your operator to unlock a blocked SIM card.</p>
            </div>
        `;
    }
    
    /**
     * Get signal bars HTML
     */
    getSignalBars(bars) {
        let barsHTML = '';
        for (let i = 1; i <= 4; i++) {
            const height = 3 + (i * 1);
            const isActive = i <= bars;
            barsHTML += `<div class="w-1 h-${height} ${isActive ? 'bg-blue-600' : 'bg-gray-300'} rounded-sm signal-bar"></div>`;
        }
        return barsHTML;
    }
    
    /**
     * Get status text
     */
    getStatusText(status) {
        const statusMap = {
            'disconnected': 'Disconnected',
            'connecting': 'Connecting',
            'connected': 'Connected'
        };
        return statusMap[status] || 'Unknown';
    }
    
    /**
     * Get status badge class
     */
    getStatusBadgeClass(status) {
        const classMap = {
            'disconnected': 'bg-red-100 text-red-700',
            'connecting': 'bg-yellow-100 text-yellow-700',
            'connected': 'bg-green-100 text-green-700'
        };
        return classMap[status] || 'bg-gray-100 text-gray-700';
    }
    
    /**
     * Get status dot class
     */
    getStatusDotClass(status) {
        const classMap = {
            'disconnected': 'bg-red-600',
            'connecting': 'bg-yellow-600',
            'connected': 'bg-green-600'
        };
        return classMap[status] || 'bg-gray-600';
    }
    
    /**
     * Setup event listeners
     */
    setupEventListeners() {
        // Cellular toggle
        const cellularToggle = document.getElementById('cellularToggle');
        if (cellularToggle) {
            cellularToggle.addEventListener('change', (e) => {
                this.handleCellularToggle(e.target.checked);
            });
        }
        
        // Network selection change
        const networkSelection = document.getElementById('networkSelection');
        if (networkSelection) {
            networkSelection.addEventListener('change', (e) => {
                this.handleNetworkSelectionChange(e.target.value);
            });
        }
    }
    
    /**
     * Initialize UI state
     */
    initializeUIState() {
        // Make this instance globally available for onclick handlers
        window.cellularUI = this;
    }
    
    /**
     * Toggle section expansion
     */
    toggleSection(sectionId) {
        const content = document.getElementById(sectionId + 'Content');
        const chevron = document.getElementById(sectionId + 'Chevron');
        
        if (this.expandedSections.has(sectionId)) {
            this.expandedSections.delete(sectionId);
            content.classList.add('hidden');
            chevron.classList.remove('fa-chevron-up');
            chevron.classList.add('fa-chevron-down');
        } else {
            this.expandedSections.add(sectionId);
            content.classList.remove('hidden');
            chevron.classList.remove('fa-chevron-down');
            chevron.classList.add('fa-chevron-up');
        }
    }
    
    /**
     * Handle cellular toggle
     */
    async handleCellularToggle(enabled) {
        try {
            this.cellularManager.toggleCellular();
            this.updateConnectionStatus();
        } catch (error) {
            console.error('[CELLULAR-UI] Failed to toggle cellular:', error);
        }
    }
    
    /**
     * Handle network selection change
     */
    handleNetworkSelectionChange(selection) {
        const availableNetworks = document.getElementById('availableNetworks');
        if (selection === 'manual') {
            availableNetworks.classList.remove('hidden');
        } else {
            availableNetworks.classList.add('hidden');
        }
    }
    
    /**
     * Update connection status display
     */
    updateConnectionStatus() {
        const status = this.cellularManager.getConnectionStatus();
        const statusText = document.getElementById('connectionStatusText');
        const badge = document.getElementById('connectionBadge');
        
        if (statusText) {
            statusText.textContent = this.getStatusText(status.status);
        }
        
        if (badge) {
            badge.className = `px-2 py-1 text-xs font-medium rounded-full ${this.getStatusBadgeClass(status.status)} flex items-center gap-1`;
            badge.innerHTML = `
                <span class="w-2 h-2 ${this.getStatusDotClass(status.status)} rounded-full"></span>
                ${this.getStatusText(status.status)}
            `;
        }
    }
    
    /**
     * Update SIM status display
     */
    updateSIMStatus() {
        const simInfo = this.cellularManager.getSimInfo();
        const badge = document.getElementById('simBadge');
        
        if (badge) {
            badge.className = `px-2 py-1 text-xs font-medium rounded-full ${simInfo.available ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'} flex items-center gap-1`;
            badge.innerHTML = `
                <span class="w-2 h-2 ${simInfo.available ? 'bg-green-600' : 'bg-red-600'} rounded-full"></span>
                ${simInfo.available ? 'SIM Available' : 'SIM Not Available'}
            `;
        }
    }
    
    /**
     * Start periodic data updates
     */
    startDataUpdates() {
        this.dataUpdateInterval = setInterval(() => {
            this.cellularManager.updateDataUsage();
        }, 5000); // Update every 5 seconds
    }
    
    /**
     * UI Event Handlers
     */
    async retryConnection() {
        try {
            await this.cellularManager.connectToNetwork();
            this.render();
        } catch (error) {
            console.error('[CELLULAR-UI] Failed to connect:', error);
            alert('Failed to connect: ' + error.message);
        }
    }
    
    async disconnectNetwork() {
        try {
            this.cellularManager.disconnectNetwork();
            this.render();
        } catch (error) {
            console.error('[CELLULAR-UI] Failed to disconnect:', error);
            alert('Failed to disconnect: ' + error.message);
        }
    }
    
    refreshStatus() {
        this.render();
    }
    
    resetData() {
        this.cellularManager.resetDataUsage();
        this.render();
    }
    
    async scanNetworks() {
        try {
            const networks = await this.cellularManager.scanNetworks();
            this.populateNetworkList(networks);
        } catch (error) {
            console.error('[CELLULAR-UI] Failed to scan networks:', error);
            alert('Failed to scan networks: ' + error.message);
        }
    }
    
    populateNetworkList(networks) {
        const tbody = document.getElementById('networkListBody');
        if (tbody) {
            tbody.innerHTML = networks.map(network => `
                <tr>
                    <td class="px-3 py-2 font-medium text-gray-900">${network.operator}</td>
                    <td class="px-3 py-2 text-gray-600">${network.technology}</td>
                    <td class="px-3 py-2">
                        <div class="flex items-center gap-1">
                            ${this.getSignalBars(network.signalBars)}
                            <span class="text-gray-600">${network.signal}</span>
                        </div>
                    </td>
                    <td class="px-3 py-2">
                        <span class="px-2 py-1 text-xs font-medium rounded bg-green-100 text-green-700">
                            ${network.status}
                        </span>
                    </td>
                </tr>
            `).join('');
        }
    }
    
    toggleDataLimit() {
        const toggle = document.getElementById('dataLimitToggle');
        const input = document.getElementById('dataLimitInput');
        
        if (toggle.checked) {
            input.classList.remove('hidden');
        } else {
            input.classList.add('hidden');
        }
    }
    
    async saveConfiguration() {
        try {
            // Collect form data
            const config = {
                networkType: document.getElementById('networkType').value,
                networkSelection: document.getElementById('networkSelection').value,
                apn: {
                    name: document.getElementById('apnName').value,
                    authType: document.getElementById('authType').value,
                    username: document.getElementById('username').value,
                    password: document.getElementById('password').value
                },
                autoConnect: document.getElementById('autoConnect').checked,
                dataRoaming: document.getElementById('dataRoaming').checked,
                dataLimit: {
                    enabled: document.getElementById('dataLimitToggle').checked,
                    limit: parseInt(document.getElementById('dataLimit').value) || 0
                }
            };
            
            this.cellularManager.updateConfiguration(config);
            await this.cellularManager.saveConfiguration();
            
            alert('Configuration saved successfully!');
            
            // Reconnect if connected
            if (this.cellularManager.getConnectionStatus().status === 'connected') {
                await this.retryConnection();
            }
        } catch (error) {
            console.error('[CELLULAR-UI] Failed to save configuration:', error);
            alert('Failed to save configuration: ' + error.message);
        }
    }
    
    resetToDefaults() {
        if (confirm('Are you sure you want to reset all settings to defaults?')) {
            this.cellularManager.resetToDefaults();
            this.render();
        }
    }
    
    /**
     * Cleanup resources
     */
    destroy() {
        if (this.dataUpdateInterval) {
            clearInterval(this.dataUpdateInterval);
        }
        
        // Remove global reference
        if (window.cellularUI === this) {
            delete window.cellularUI;
        }
        
    }
}

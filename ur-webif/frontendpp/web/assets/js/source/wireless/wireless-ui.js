/**
 * Wireless Configuration UI Controller
 * Handles all UI rendering and interactions for wireless network configuration
 */

import { WirelessManager } from './wireless.js';

export class WirelessUI {
    constructor(httpClient, container) {
        this.httpClient = httpClient;
        this.container = container;
        this.wirelessManager = new WirelessManager(httpClient);
        this.expandedSections = new Set();
        
        this.init();
    }
    
    init() {
        console.log('[WIRELESS-UI] Initializing Wireless UI');
        this.render();
        this.setupEventListeners();
    }
    
    /**
     * Render the complete wireless configuration UI
     */
    render() {
        const content = this.getWirelessConfigHTML();
        this.container.innerHTML = content;
        
        // Initialize UI state
        this.initializeUIState();
        this.updateModeDisplay();
    }
    
    /**
     * Get the complete HTML for wireless configuration
     */
    getWirelessConfigHTML() {
        const wifiStatus = this.wirelessManager.getWiFiStatus();
        const apSettings = this.wirelessManager.getAccessPointSettings();
        
        return `
            <div class="max-w-4xl mx-auto space-y-4">
                <!-- Wi-Fi Radio Toggle -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200 p-4 flex items-center justify-between">
                    <div>
                        <div class="flex items-center gap-2">
                            <i class="fas fa-power-off text-gray-600"></i>
                            <h2 class="font-medium text-gray-900">Wi-Fi Radio</h2>
                        </div>
                        <p class="text-sm text-gray-500 mt-1">Enable or disable wireless functionality</p>
                    </div>
                    <label class="toggle-switch">
                        <input type="checkbox" id="wifiRadio" ${wifiStatus.enabled ? 'checked' : ''} onchange="window.wirelessUI.toggleWiFiRadio()">
                        <span class="toggle-slider"></span>
                    </label>
                </div>

                <!-- Wireless Mode -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200 p-4 ${!wifiStatus.enabled ? 'opacity-50 pointer-events-none' : ''}" id="modeSection">
                    <h3 class="font-medium text-gray-900 mb-4">Wireless Mode</h3>
                    <div class="grid grid-cols-2 gap-4">
                        <div class="mode-card p-4 rounded-lg border-2 border-gray-200 cursor-pointer transition-all hover:border-blue-400" id="clientModeCard" onclick="window.wirelessUI.switchMode('client')">
                            <div class="flex items-center gap-2 mb-2">
                                <i class="fas fa-laptop text-gray-600"></i>
                                <h4 class="font-medium text-gray-900">Client Mode</h4>
                            </div>
                            <p class="text-sm text-gray-500">Connect to existing Wi-Fi network</p>
                        </div>
                        <div class="mode-card p-4 rounded-lg border-2 border-gray-200 cursor-pointer transition-all hover:border-blue-400" id="accessPointCard" onclick="window.wirelessUI.switchMode('accessPoint')">
                            <div class="flex items-center gap-2 mb-2">
                                <i class="fas fa-wifi text-gray-600"></i>
                                <h4 class="font-medium text-gray-900">Access Point</h4>
                            </div>
                            <p class="text-sm text-gray-500">Create Wi-Fi hotspot</p>
                        </div>
                    </div>
                </div>

                ${this.getAccessPointSettingsHTML()}
                ${this.getClientModeSettingsHTML()}
            </div>
            
            <style>
                .toggle-switch {
                    position: relative;
                    display: inline-block;
                    width: 48px;
                    height: 24px;
                }
                .toggle-switch input {
                    opacity: 0;
                    width: 0;
                    height: 0;
                }
                .toggle-slider {
                    position: absolute;
                    cursor: pointer;
                    top: 0;
                    left: 0;
                    right: 0;
                    bottom: 0;
                    background-color: #cbd5e1;
                    transition: 0.3s;
                    border-radius: 24px;
                }
                .toggle-slider:before {
                    position: absolute;
                    content: "";
                    height: 18px;
                    width: 18px;
                    left: 3px;
                    bottom: 3px;
                    background-color: white;
                    transition: 0.3s;
                    border-radius: 50%;
                }
                input:checked + .toggle-slider {
                    background-color: #60a5fa;
                }
                input:checked + .toggle-slider:before {
                    transform: translateX(24px);
                }
                .mode-card.active {
                    border-color: #3b82f6 !important;
                    background: #eff6ff !important;
                }
                .network-item {
                    border-bottom: 1px solid #f3f4f6;
                }
                .network-item:last-child {
                    border-bottom: none;
                }
            </style>
        `;
    }
    
    /**
     * Get access point settings HTML
     */
    getAccessPointSettingsHTML() {
        const settings = this.wirelessManager.getAccessPointSettings();
        const wifiStatus = this.wirelessManager.getWiFiStatus();
        const statistics = this.wirelessManager.getNetworkStatistics();
        
        return `
            <!-- Access Point Settings -->
            <div id="accessPointSettings" class="bg-white rounded-lg shadow-sm border border-gray-200 p-6 ${!wifiStatus.enabled || wifiStatus.mode !== 'accessPoint' ? 'hidden' : ''} ${!wifiStatus.enabled ? 'opacity-50 pointer-events-none' : ''}">
                <div class="flex items-center justify-between mb-4">
                    <h3 class="font-medium text-gray-900">Access Point Settings</h3>
                    <div class="flex items-center gap-2 text-sm text-gray-600">
                        <span id="statusBadge">Broadcasting as ${settings.ssid} · On channel ${settings.channel === 'auto' ? 'Auto' : settings.channel}</span>
                    </div>
                </div>
                <p class="text-sm text-gray-500 mb-6">Configure the hotspot clients will connect to</p>

                <div class="grid grid-cols-2 gap-6">
                    <!-- Network Name (SSID) -->
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Network name (SSID)</label>
                        <input type="text" id="ssid" value="${settings.ssid}" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500">
                        <p class="text-xs text-gray-500 mt-1">This is how your hotspot will appear to other devices.</p>
                    </div>

                    <!-- Password -->
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Password</label>
                        <div class="relative">
                            <input type="password" id="password" value="${settings.password}" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500">
                            <button onclick="window.wirelessUI.togglePassword()" class="absolute right-3 top-2.5 text-sm text-blue-600 hover:text-blue-700">Show password</button>
                        </div>
                        <p class="text-xs text-gray-500 mt-1">Use at least 8 characters. Required for secured networks.</p>
                    </div>

                    <!-- Band -->
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Band</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500" id="band">
                            <option value="dual" ${settings.band === 'dual' ? 'selected' : ''}>2.4 GHz + 5 GHz</option>
                            <option value="2.4" ${settings.band === '2.4' ? 'selected' : ''}>2.4 GHz only</option>
                            <option value="5" ${settings.band === '5' ? 'selected' : ''}>5 GHz only</option>
                        </select>
                        <p class="text-xs text-gray-500 mt-1">Dual-band is recommended for most devices.</p>
                    </div>

                    <!-- Channel -->
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Channel</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500" id="channel">
                            <option value="auto" ${settings.channel === 'auto' ? 'selected' : ''}>Auto select</option>
                            <option value="1" ${settings.channel === '1' ? 'selected' : ''}>Channel 1</option>
                            <option value="6" ${settings.channel === '6' ? 'selected' : ''}>Channel 6</option>
                            <option value="11" ${settings.channel === '11' ? 'selected' : ''}>Channel 11</option>
                        </select>
                        <p class="text-xs text-gray-500 mt-1">Automatically picks the least congested channel.</p>
                    </div>

                    <!-- Security -->
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Security</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500" id="security">
                            <option value="wpa2-psk" ${settings.security === 'wpa2-psk' ? 'selected' : ''}>WPA2-PSK</option>
                            <option value="wpa3" ${settings.security === 'wpa3' ? 'selected' : ''}>WPA3</option>
                            <option value="open" ${settings.security === 'open' ? 'selected' : ''}>Open (No security)</option>
                        </select>
                        <p class="text-xs text-gray-500 mt-1">Recommended: WPA2 for compatibility and security.</p>
                    </div>

                    <!-- Maximum clients -->
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Maximum clients</label>
                        <input type="number" id="maxClients" value="${settings.maxClients}" min="1" max="50" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500">
                        <p class="text-xs text-gray-500 mt-1">Limit how many devices can connect at once.</p>
                    </div>
                </div>

                <!-- Options -->
                <div class="grid grid-cols-2 gap-6 mt-6">
                    <div class="flex items-center justify-between">
                        <div>
                            <div class="flex items-center gap-2">
                                <i class="fas fa-eye-slash text-gray-600"></i>
                                <span class="font-medium text-gray-900">Broadcast network</span>
                            </div>
                            <p class="text-xs text-gray-500 mt-1">Hide SSID to make the network less visible.</p>
                        </div>
                        <label class="toggle-switch">
                            <input type="checkbox" id="broadcastSSID" ${settings.broadcastSSID ? 'checked' : ''}>
                            <span class="toggle-slider"></span>
                        </label>
                    </div>

                    <div class="flex items-center justify-between">
                        <div>
                            <div class="flex items-center gap-2">
                                <i class="fas fa-user-lock text-gray-600"></i>
                                <span class="font-medium text-gray-900">Guest isolation</span>
                            </div>
                            <p class="text-xs text-gray-500 mt-1">Prevent connected devices from seeing each other.</p>
                        </div>
                        <label class="toggle-switch">
                            <input type="checkbox" id="guestIsolation" ${settings.guestIsolation ? 'checked' : ''}>
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                </div>

                <!-- Show Status Toggle -->
                <div class="mt-6 pt-6 border-t border-gray-200">
                    <div class="flex items-center justify-between">
                        <div>
                            <h4 class="font-medium text-gray-900">Show status</h4>
                            <p class="text-sm text-gray-500">View connected clients and real-time hotspot activity.</p>
                        </div>
                        <button onclick="window.wirelessUI.toggleStatus()" class="text-blue-600 hover:text-blue-700 text-sm font-medium">
                            <span id="statusToggleBtn">Show <i class="fas fa-chevron-down ml-1"></i></span>
                        </button>
                    </div>
                </div>

                <!-- Connected Devices Status (Hidden by default) -->
                <div id="connectedDevices" class="hidden mt-4">
                    ${this.getConnectedDevicesHTML(statistics)}
                </div>

                <div class="mt-6 pt-6 border-t border-gray-200">
                    <p class="text-sm text-gray-600 mb-4">Clients will connect to ${settings.ssid} using ${settings.security.toUpperCase()} on the best available channel.</p>
                    <div class="flex items-center justify-between">
                        <span class="text-xs text-gray-500">Changes apply immediately after saving.</span>
                        <div class="flex gap-2">
                            <span class="px-3 py-1 bg-green-100 text-green-700 text-xs rounded-lg font-medium">Hotspot enabled</span>
                            <span class="px-3 py-1 bg-blue-100 text-blue-700 text-xs rounded-lg font-medium">${settings.band === 'dual' ? '2.4 / 5 GHz' : settings.band + ' GHz'}</span>
                        </div>
                    </div>
                    <div class="flex gap-3 mt-4">
                        <button class="px-4 py-2 text-gray-700 border border-gray-300 rounded-lg hover:bg-gray-50" onclick="window.wirelessUI.discardAccessPointSettings()">Discard</button>
                        <button class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600" onclick="window.wirelessUI.saveAccessPointSettings()">Save access point</button>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get connected devices HTML
     */
    getConnectedDevicesHTML(statistics) {
        const devices = this.wirelessManager.getConnectedDevices();
        
        return `
            <div class="bg-blue-50 border border-blue-200 rounded-lg p-4 mb-4">
                <p class="text-sm text-gray-700">
                    <strong>${statistics.connectedDevices} devices</strong> connected to ${this.wirelessManager.getAccessPointSettings().ssid}
                    <span class="float-right">
                        Current throughput: <strong>${statistics.currentThroughput} Mbps</strong> · Peak today: <strong>${statistics.peakThroughput} Mbps</strong>
                    </span>
                </p>
            </div>

            <div class="space-y-3">
                <div class="text-xs font-medium text-gray-500 grid grid-cols-12 gap-4 px-4">
                    <div class="col-span-3">Device<br>Name and band</div>
                    <div class="col-span-2">Signal<br>Quality (dBm)</div>
                    <div class="col-span-2">Traffic<br>Today / live</div>
                    <div class="col-span-3">IP / MAC<br>Address details</div>
                    <div class="col-span-2 text-right">Limit speed</div>
                </div>

                ${devices.map(device => this.getDeviceItemHTML(device)).join('')}
            </div>

            <p class="text-xs text-gray-500 mt-4">Values update in real time while this page is open. Disconnecting a device will immediately drop its connection.</p>
        `;
    }
    
    /**
     * Get individual device item HTML
     */
    getDeviceItemHTML(device) {
        const signalQuality = this.wirelessManager.getSignalQualityText(device.signalDbm);
        const signalColor = this.wirelessManager.getSignalColorClass(device.signalDbm);
        
        return `
            <div class="bg-gray-50 rounded-lg p-4">
                <div class="grid grid-cols-12 gap-4 items-center">
                    <div class="col-span-3">
                        <div class="font-medium text-gray-900">${device.name}</div>
                        <div class="text-xs text-gray-500">${device.macAddress} · ${device.band}</div>
                    </div>
                    <div class="col-span-2">
                        <div class="font-medium ${signalColor}">${signalQuality} (${device.signalDbm} dBm)</div>
                    </div>
                    <div class="col-span-2">
                        <div class="font-medium text-gray-900">${device.trafficToday}</div>
                        <div class="text-xs text-gray-500">Now: ${device.trafficLive}</div>
                    </div>
                    <div class="col-span-3">
                        <div class="font-medium text-gray-900">${device.ipAddress}</div>
                        <div class="text-xs text-gray-500">${device.leaseType}</div>
                    </div>
                    <div class="col-span-2 text-right">
                        <button class="px-3 py-1 bg-blue-500 text-white text-xs rounded-lg hover:bg-blue-600" onclick="window.wirelessUI.disconnectDevice('${device.id}')">Disconnect</button>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get client mode settings HTML
     */
    getClientModeSettingsHTML() {
        const wifiStatus = this.wirelessManager.getWiFiStatus();
        const availableNetworks = this.wirelessManager.getAvailableNetworks();
        const savedNetworks = this.wirelessManager.getSavedNetworks();
        const clientSettings = this.wirelessManager.getClientSettings();
        
        return `
            <!-- Client Mode Settings (Hidden by default) -->
            <div id="clientModeSettings" class="space-y-4 ${!wifiStatus.enabled || wifiStatus.mode !== 'client' ? 'hidden' : ''} ${!wifiStatus.enabled ? 'opacity-50 pointer-events-none' : ''}">
                <!-- Available Networks -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
                    <div class="flex items-center justify-between mb-4">
                        <h3 class="font-medium text-gray-900">Available Networks</h3>
                        <button onclick="window.wirelessUI.scanNetworks()" class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600 flex items-center gap-2" ${clientSettings.scanning ? 'disabled' : ''}>
                            <i class="fas fa-sync-alt" id="scanIcon" ${clientSettings.scanning ? 'class="fa-spin"' : ''}></i>
                            ${clientSettings.scanning ? 'Scanning...' : 'Scan Networks'}
                        </button>
                    </div>

                    <div class="space-y-0" id="networkList">
                        ${availableNetworks.map(network => this.getNetworkItemHTML(network)).join('')}
                    </div>
                </div>

                <!-- Manual Connection -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
                    <button onclick="window.wirelessUI.toggleManualConnection()" class="w-full flex items-center justify-between">
                        <h3 class="font-medium text-gray-900">Manual Connection</h3>
                        <i class="fas fa-chevron-down" id="manualIcon"></i>
                    </button>
                    <div id="manualConnectionForm" class="hidden mt-4">
                        <p class="text-sm text-gray-500 mb-4">Enter network details to connect to a hidden or custom Wi-Fi network.</p>
                        <div class="grid grid-cols-2 gap-4">
                            <div>
                                <label class="block text-sm font-medium text-gray-700 mb-2">Network name (SSID)</label>
                                <input type="text" id="manualSSID" placeholder="e.g. MyNetwork" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500">
                            </div>
                            <div>
                                <label class="block text-sm font-medium text-gray-700 mb-2">Security</label>
                                <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500" id="manualSecurity">
                                    <option value="wpa2-personal" selected>WPA2-Personal</option>
                                    <option value="wpa3">WPA3</option>
                                    <option value="wep">WEP</option>
                                    <option value="open">Open (No security)</option>
                                </select>
                            </div>
                            <div>
                                <label class="block text-sm font-medium text-gray-700 mb-2">Band</label>
                                <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500" id="manualBand">
                                    <option value="auto" selected>Auto</option>
                                    <option value="2.4">2.4 GHz only</option>
                                    <option value="5">5 GHz only</option>
                                </select>
                            </div>
                            <div>
                                <label class="block text-sm font-medium text-gray-700 mb-2">Password</label>
                                <input type="password" id="manualPassword" placeholder="Enter network password" class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500">
                            </div>
                        </div>
                        <div class="flex items-center gap-2 mt-4">
                            <input type="checkbox" id="saveNetwork" class="w-4 h-4 text-blue-600">
                            <label for="saveNetwork" class="text-sm text-gray-700">Save this network</label>
                        </div>
                        <div class="flex gap-3 mt-4">
                            <button class="px-4 py-2 text-gray-700 border border-gray-300 rounded-lg hover:bg-gray-50" onclick="window.wirelessUI.cancelManualConnection()">Cancel</button>
                            <button class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600" onclick="window.wirelessUI.connectManualNetwork()">Connect</button>
                        </div>
                    </div>
                </div>

                <!-- Saved Networks -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
                    <button onclick="window.wirelessUI.toggleSavedNetworks()" class="w-full flex items-center justify-between">
                        <h3 class="font-medium text-gray-900">Saved Networks</h3>
                        <i class="fas fa-chevron-down" id="savedIcon"></i>
                    </button>
                    <div id="savedNetworksList" class="hidden mt-4">
                        <p class="text-sm text-gray-500 mb-4">Manage networks this device will remember and connect to automatically.</p>
                        
                        ${savedNetworks.map(network => this.getSavedNetworkItemHTML(network)).join('')}

                        <button class="mt-4 text-red-600 hover:text-red-700 text-sm font-medium" onclick="window.wirelessUI.forgetAllNetworks()">Clear all saved</button>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get network item HTML
     */
    getNetworkItemHTML(network) {
        const signalIcon = network.signal === 'strong' ? 'text-green-600' : 
                          network.signal === 'medium' ? 'text-yellow-600' : 'text-orange-600';
        
        return `
            <div class="network-item py-4 flex items-center justify-between">
                <div class="flex items-center gap-3">
                    <i class="fas fa-wifi text-2xl ${signalIcon}"></i>
                    <div>
                        <div class="font-medium text-gray-900">${network.ssid}</div>
                        <div class="text-sm text-gray-500">${network.signal.charAt(0).toUpperCase() + network.signal.slice(1)} signal · ${network.security}</div>
                    </div>
                </div>
                <div class="flex items-center gap-2">
                    ${network.security !== 'Open' ? '<i class="fas fa-lock text-gray-400"></i>' : ''}
                    ${network.connected ? 
                        '<span class="px-3 py-1 bg-green-100 text-green-700 text-xs rounded-lg font-medium">Connected</span>' : 
                        `<span class="px-3 py-1 bg-gray-200 text-gray-700 text-xs rounded-lg font-medium">${network.band}</span>`
                    }
                    ${!network.connected ? 
                        `<button class="px-4 py-1.5 text-blue-600 border border-blue-600 rounded-lg hover:bg-blue-50" onclick="window.wirelessUI.connectToNetwork('${network.id}')">Connect</button>` : 
                        ''
                    }
                </div>
            </div>
        `;
    }
    
    /**
     * Get saved network item HTML
     */
    getSavedNetworkItemHTML(network) {
        return `
            <div class="bg-gray-50 rounded-lg p-4 mb-3">
                <div class="flex items-center justify-between">
                    <div class="flex-1">
                        <div class="font-medium text-gray-900">${network.ssid}</div>
                        <div class="text-sm text-gray-500">${network.priority.charAt(0).toUpperCase() + network.priority.slice(1)} · ${network.autoConnect ? 'Auto-connect enabled' : 'Manual connect only'}</div>
                    </div>
                    <div class="flex items-center gap-2">
                        <span class="px-2 py-1 ${network.band === '2.4 / 5 GHz' ? 'bg-blue-100 text-blue-700' : 'bg-gray-200 text-gray-700'} text-xs rounded">${network.band}</span>
                        <span class="px-2 py-1 ${network.autoConnect ? 'bg-green-100 text-green-700' : 'bg-gray-200 text-gray-700'} text-xs rounded">${network.autoConnect ? 'Auto-connect' : 'Manual'}</span>
                        <button class="p-2 text-gray-600 hover:text-gray-900">
                            <i class="fas fa-ellipsis-v"></i>
                        </button>
                        <button class="text-blue-600 hover:text-blue-700 text-sm" onclick="window.wirelessUI.showNetworkPassword('${network.id}')">Show password</button>
                        <button class="text-red-600 hover:text-red-700 text-sm" onclick="window.wirelessUI.removeSavedNetwork('${network.id}')">Remove</button>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Setup event listeners
     */
    setupEventListeners() {
        // Make UI instance globally available for onclick handlers
        window.wirelessUI = this;
    }
    
    /**
     * Initialize UI state
     */
    initializeUIState() {
        const wifiStatus = this.wirelessManager.getWiFiStatus();
        
        // Set initial mode
        this.updateModeDisplay();
        
        // Update WiFi radio state
        if (!wifiStatus.enabled) {
            document.getElementById('modeSection').classList.add('opacity-50', 'pointer-events-none');
            document.getElementById('accessPointSettings').classList.add('opacity-50', 'pointer-events-none');
            document.getElementById('clientModeSettings').classList.add('opacity-50', 'pointer-events-none');
        }
    }
    
    /**
     * Update mode display
     */
    updateModeDisplay() {
        const wifiStatus = this.wirelessManager.getWiFiStatus();
        
        // Update mode cards
        const clientCard = document.getElementById('clientModeCard');
        const apCard = document.getElementById('accessPointCard');
        
        if (clientCard && apCard) {
            clientCard.classList.remove('active');
            apCard.classList.remove('active');
            
            if (wifiStatus.mode === 'client') {
                clientCard.classList.add('active');
            } else {
                apCard.classList.add('active');
            }
        }
        
        // Show/hide appropriate settings
        const apSettings = document.getElementById('accessPointSettings');
        const clientSettings = document.getElementById('clientModeSettings');
        
        if (apSettings && clientSettings) {
            if (wifiStatus.mode === 'accessPoint') {
                apSettings.classList.remove('hidden');
                clientSettings.classList.add('hidden');
            } else {
                apSettings.classList.add('hidden');
                clientSettings.classList.remove('hidden');
            }
        }
    }
    
    /**
     * Handle WiFi radio toggle
     */
    async toggleWiFiRadio() {
        const enabled = document.getElementById('wifiRadio').checked;
        const result = await this.wirelessManager.toggleWiFi(enabled);
        
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render(); // Re-render to update UI state
        } else {
            this.showNotification(result.error, 'error');
            document.getElementById('wifiRadio').checked = !enabled; // Revert toggle
        }
    }
    
    /**
     * Handle mode switch
     */
    async switchMode(mode) {
        const result = await this.wirelessManager.switchMode(mode);
        
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.updateModeDisplay();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Toggle password visibility
     */
    togglePassword() {
        const passwordInput = document.getElementById('password');
        const button = event.target;
        
        if (passwordInput.type === 'password') {
            passwordInput.type = 'text';
            button.textContent = 'Hide password';
        } else {
            passwordInput.type = 'password';
            button.textContent = 'Show password';
        }
    }
    
    /**
     * Toggle connected devices status
     */
    toggleStatus() {
        const devicesDiv = document.getElementById('connectedDevices');
        const toggleBtn = document.getElementById('statusToggleBtn');
        
        if (devicesDiv.classList.contains('hidden')) {
            devicesDiv.classList.remove('hidden');
            toggleBtn.innerHTML = 'Hide <i class="fas fa-chevron-up ml-1"></i>';
        } else {
            devicesDiv.classList.add('hidden');
            toggleBtn.innerHTML = 'Show <i class="fas fa-chevron-down ml-1"></i>';
        }
    }
    
    /**
     * Scan for networks
     */
    async scanNetworks() {
        this.showNotification('Scanning for networks...', 'info');
        const result = await this.wirelessManager.scanNetworks();
        
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render(); // Re-render to update network list
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Connect to a network
     */
    async connectToNetwork(networkId) {
        const network = this.wirelessManager.getAvailableNetworks().find(n => n.id === networkId);
        if (!network) return;
        
        // For secured networks, show password prompt (simplified for demo)
        let password = null;
        if (network.security !== 'Open') {
            password = prompt(`Enter password for ${network.ssid}:`);
            if (!password) return;
        }
        
        this.showNotification(`Connecting to ${network.ssid}...`, 'info');
        const result = await this.wirelessManager.connectToNetwork(networkId, password);
        
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render(); // Re-render to update connection status
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Toggle manual connection form
     */
    toggleManualConnection() {
        const form = document.getElementById('manualConnectionForm');
        const icon = document.getElementById('manualIcon');
        
        if (form.classList.contains('hidden')) {
            form.classList.remove('hidden');
            icon.classList.remove('fa-chevron-down');
            icon.classList.add('fa-chevron-up');
        } else {
            form.classList.add('hidden');
            icon.classList.remove('fa-chevron-up');
            icon.classList.add('fa-chevron-down');
        }
    }
    
    /**
     * Connect to manual network
     */
    async connectManualNetwork() {
        const ssid = document.getElementById('manualSSID').value;
        const security = document.getElementById('manualSecurity').value;
        const band = document.getElementById('manualBand').value;
        const password = document.getElementById('manualPassword').value;
        const saveNetwork = document.getElementById('saveNetwork').checked;
        
        if (!ssid) {
            this.showNotification('Please enter a network name', 'error');
            return;
        }
        
        if (security !== 'open' && !password) {
            this.showNotification('Please enter a password', 'error');
            return;
        }
        
        this.showNotification(`Connecting to ${ssid}...`, 'info');
        
        // Simulate manual connection
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        this.showNotification(`Connected to ${ssid}`, 'success');
        
        if (saveNetwork) {
            // Add to saved networks
            const newNetwork = {
                id: ssid.toLowerCase().replace(/\s+/g, '-'),
                ssid: ssid,
                priority: 'saved',
                autoConnect: false,
                band: band === 'auto' ? '2.4 / 5 GHz' : band + ' GHz',
                security: security.toUpperCase(),
                password: '********'
            };
            
            this.wirelessManager.savedNetworks.unshift(newNetwork);
        }
        
        // Clear form
        document.getElementById('manualSSID').value = '';
        document.getElementById('manualPassword').value = '';
        document.getElementById('saveNetwork').checked = false;
        
        this.render();
    }
    
    /**
     * Cancel manual connection
     */
    cancelManualConnection() {
        document.getElementById('manualConnectionForm').classList.add('hidden');
        document.getElementById('manualSSID').value = '';
        document.getElementById('manualPassword').value = '';
        document.getElementById('saveNetwork').checked = false;
    }
    
    /**
     * Toggle saved networks list
     */
    toggleSavedNetworks() {
        const list = document.getElementById('savedNetworksList');
        const icon = document.getElementById('savedIcon');
        
        if (list.classList.contains('hidden')) {
            list.classList.remove('hidden');
            icon.classList.remove('fa-chevron-down');
            icon.classList.add('fa-chevron-up');
        } else {
            list.classList.add('hidden');
            icon.classList.remove('fa-chevron-up');
            icon.classList.add('fa-chevron-down');
        }
    }
    
    /**
     * Show network password
     */
    showNetworkPassword(networkId) {
        const network = this.wirelessManager.getSavedNetworks().find(n => n.id === networkId);
        if (network && network.password) {
            this.showNotification(`Password for ${network.ssid}: ${network.password}`, 'info');
        }
    }
    
    /**
     * Remove saved network
     */
    async removeSavedNetwork(networkId) {
        if (confirm('Are you sure you want to remove this saved network?')) {
            const result = await this.wirelessManager.removeSavedNetwork(networkId);
            
            if (result.success) {
                this.showNotification(result.message, 'success');
                this.render();
            } else {
                this.showNotification(result.error, 'error');
            }
        }
    }
    
    /**
     * Forget all networks
     */
    async forgetAllNetworks() {
        if (confirm('Are you sure you want to forget all saved networks?')) {
            const result = await this.wirelessManager.forgetAllNetworks();
            
            if (result.success) {
                this.showNotification(result.message, 'success');
                this.render();
            } else {
                this.showNotification(result.error, 'error');
            }
        }
    }
    
    /**
     * Disconnect device from access point
     */
    async disconnectDevice(deviceId) {
        if (confirm('Are you sure you want to disconnect this device?')) {
            const result = await this.wirelessManager.disconnectDevice(deviceId);
            
            if (result.success) {
                this.showNotification(result.message, 'success');
                this.render();
            } else {
                this.showNotification(result.error, 'error');
            }
        }
    }
    
    /**
     * Save access point settings
     */
    async saveAccessPointSettings() {
        const settings = {
            ssid: document.getElementById('ssid').value,
            password: document.getElementById('password').value,
            band: document.getElementById('band').value,
            channel: document.getElementById('channel').value,
            security: document.getElementById('security').value,
            maxClients: parseInt(document.getElementById('maxClients').value),
            broadcastSSID: document.getElementById('broadcastSSID').checked,
            guestIsolation: document.getElementById('guestIsolation').checked
        };
        
        this.showNotification('Saving access point settings...', 'info');
        const result = await this.wirelessManager.updateAccessPointSettings(settings);
        
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Discard access point settings
     */
    discardAccessPointSettings() {
        this.render();
        this.showNotification('Changes discarded', 'info');
    }
    
    /**
     * Show notification
     */
    showNotification(message, type = 'info') {
        // Create a simple notification system
        const notification = document.createElement('div');
        notification.className = `fixed top-4 right-4 px-4 py-3 rounded-lg shadow-lg z-50 transform transition-all duration-300 translate-x-full`;
        
        const colors = {
            success: 'bg-green-500 text-white',
            error: 'bg-red-500 text-white',
            info: 'bg-blue-500 text-white',
            warning: 'bg-yellow-500 text-white'
        };
        
        notification.classList.add(...colors[type].split(' '));
        notification.innerHTML = `
            <div class="flex items-center gap-2">
                <i class="fas fa-${type === 'success' ? 'check-circle' : type === 'error' ? 'exclamation-circle' : 'info-circle'}"></i>
                <span>${message}</span>
            </div>
        `;
        
        document.body.appendChild(notification);
        
        // Animate in
        setTimeout(() => {
            notification.classList.remove('translate-x-full');
            notification.classList.add('translate-x-0');
        }, 100);
        
        // Remove after 3 seconds
        setTimeout(() => {
            notification.classList.add('translate-x-full');
            setTimeout(() => {
                document.body.removeChild(notification);
            }, 300);
        }, 3000);
    }
    
    /**
     * Cleanup method
     */
    destroy() {
        // Remove global reference
        if (window.wirelessUI === this) {
            delete window.wirelessUI;
        }
    }
}

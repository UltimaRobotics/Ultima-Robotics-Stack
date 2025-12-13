/**
 * Wired Configuration UI Controller
 * Handles all UI rendering and interactions for wired network configuration
 */

import { WiredManager } from './wired.js';

export class WiredUI {
    constructor(httpClient, container) {
        this.httpClient = httpClient;
        this.container = container;
        this.wiredManager = new WiredManager(httpClient);
        this.expandedSections = new Set();
        
        this.init();
    }
    
    init() {
        this.render();
        this.setupEventListeners();
    }
    
    /**
     * Render the complete wired configuration UI
     */
    render() {
        const content = this.getWiredConfigHTML();
        this.container.innerHTML = content;
        
        // Initialize toggle states
        this.initializeToggleSwitch();
        this.updateConnectionStatus();
    }
    
    /**
     * Get the complete HTML for wired configuration
     */
    getWiredConfigHTML() {
        const status = this.wiredManager.getConnectionStatus();
        const isEnabled = this.wiredManager.isEthernetEnabled;
        
        return `
            <div class="max-w-6xl mx-auto">
                <!-- Wired Configuration Header -->
                <div class="bg-white rounded-lg shadow-sm border border-gray-200 p-6 mb-4">
                    <div class="flex justify-between items-center">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-ethernet text-gray-600 text-xl"></i>
                            <div>
                                <h1 class="text-xl font-semibold text-gray-900">Wired Configuration</h1>
                                <p class="text-sm text-gray-500">Configure Ethernet settings and network connections</p>
                            </div>
                        </div>
                        <div class="text-right">
                            <p class="text-sm text-gray-600 mb-1">Ethernet Interface</p>
                            <div class="flex items-center gap-2">
                                <span class="text-xs text-gray-500">Enable or disable wired functionality</span>
                                <label class="toggle-switch">
                                    <input type="checkbox" id="ethernetToggle" ${isEnabled ? 'checked' : ''}>
                                    <span class="slider"></span>
                                </label>
                            </div>
                        </div>
                    </div>
                </div>

                ${this.getConnectionStatusSectionHTML()}
                ${this.getBasicSettingsSectionHTML()}
                ${this.getAdvancedSettingsSectionHTML()}
                ${this.getConnectionProfilesSectionHTML()}
            </div>
            
            <style>
                .section-card {
                    transition: all 0.3s ease;
                }
                .section-card:hover {
                    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
                }
                .expandable-content {
                    max-height: 0;
                    overflow: hidden;
                    transition: max-height 0.3s ease;
                }
                .expandable-content.expanded {
                    max-height: 2000px;
                }
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
                .slider {
                    position: absolute;
                    cursor: pointer;
                    top: 0;
                    left: 0;
                    right: 0;
                    bottom: 0;
                    background-color: #cbd5e1;
                    transition: 0.4s;
                    border-radius: 24px;
                }
                .slider:before {
                    position: absolute;
                    content: "";
                    height: 18px;
                    width: 18px;
                    left: 3px;
                    bottom: 3px;
                    background-color: white;
                    transition: 0.4s;
                    border-radius: 50%;
                }
                input:checked + .slider {
                    background-color: #3b82f6;
                }
                input:checked + .slider:before {
                    transform: translateX(24px);
                }
            </style>
        `;
    }
    
    /**
     * Get connection status section HTML
     */
    getConnectionStatusSectionHTML() {
        const status = this.wiredManager.getConnectionStatus();
        const isExpanded = this.expandedSections.has('connectionStatus');
        
        return `
            <div class="bg-white rounded-lg shadow-sm border border-gray-200 mb-4 section-card">
                <div class="p-6 cursor-pointer" onclick="window.wiredUI.toggleSection('connectionStatus')">
                    <div class="flex justify-between items-center">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-signal text-gray-600"></i>
                            <div>
                                <h2 class="text-lg font-semibold text-gray-900">Connection Status</h2>
                                <p class="text-sm text-gray-500">View real-time network connection details</p>
                            </div>
                        </div>
                        <div class="flex items-center gap-3">
                            <span class="px-3 py-1 ${status.connected ? 'bg-blue-500 text-white' : 'bg-gray-300 text-gray-600'} text-xs font-medium rounded-full">
                                ${status.connected ? 'Connected' : 'Disconnected'}
                            </span>
                            <span class="text-sm text-gray-600">${status.interface}</span>
                            <span class="text-sm text-gray-600">${status.connectionType}</span>
                            <i class="fas fa-chevron-down text-gray-400 transition-transform ${isExpanded ? 'rotate-180' : ''}" id="connectionStatusIcon"></i>
                        </div>
                    </div>
                </div>
                
                <div id="connectionStatus" class="expandable-content ${isExpanded ? 'expanded' : ''}">
                    <div class="px-6 pb-6 border-t border-gray-100 pt-6">
                        ${status.connected ? this.getConnectedStatusHTML(status) : this.getDisconnectedStatusHTML()}
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get connected status HTML
     */
    getConnectedStatusHTML(status) {
        return `
            <div class="flex items-center gap-2 mb-4">
                <div class="w-2 h-2 bg-green-500 rounded-full"></div>
                <span class="font-semibold text-gray-900">CONNECTED</span>
                <span class="text-sm text-gray-500 ml-auto">${status.linkSpeed} (${status.duplex}) • Last updated: ${this.getTimeAgo(status.lastUpdated)}</span>
            </div>
            
            <div class="grid grid-cols-3 gap-4">
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">Connection Type</p>
                    <p class="font-semibold text-gray-900">${status.connectionType}</p>
                    <p class="text-xs text-gray-500 mt-1">Network configuration</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">IP Address</p>
                    <p class="font-semibold text-gray-900">${status.ipAddress}</p>
                    <p class="text-xs text-gray-500 mt-1">IPv4 primary</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">MAC Address</p>
                    <p class="font-semibold text-gray-900">${status.macAddress}</p>
                    <p class="text-xs text-gray-500 mt-1">Physical address</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">Link Speed</p>
                    <p class="font-semibold text-gray-900">${status.linkSpeed}</p>
                    <p class="text-xs text-gray-500 mt-1">Interface speed</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">Session Duration</p>
                    <p class="font-semibold text-gray-900">${status.sessionDuration}</p>
                    <p class="text-xs text-gray-500 mt-1">Current session</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">Gateway</p>
                    <p class="font-semibold text-gray-900">${status.gateway}</p>
                    <p class="text-xs text-gray-500 mt-1">Default route</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">External IP</p>
                    <p class="font-semibold text-gray-900">${status.externalIp}</p>
                    <p class="text-xs text-gray-500 mt-1">Public address</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">DNS Servers</p>
                    <p class="font-semibold text-gray-900">${status.dnsServers}</p>
                    <p class="text-xs text-gray-500 mt-1">Name resolution</p>
                </div>
                <div class="bg-blue-50 p-4 rounded-lg">
                    <p class="text-xs text-gray-600 mb-1">Network Speed</p>
                    <p class="font-semibold text-gray-900">${status.networkSpeed.download} / ${status.networkSpeed.upload} Mbps</p>
                    <p class="text-xs text-gray-500 mt-1">Download / Upload</p>
                </div>
            </div>

            <div class="flex gap-3 mt-6">
                <button class="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.reconnectInterface()">
                    Reconnect
                </button>
                <button class="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.renewDhcpLease()">
                    Renew IP (DHCP)
                </button>
                <button class="px-4 py-2 bg-blue-500 text-white rounded-lg text-sm font-medium hover:bg-blue-600" onclick="window.wiredUI.refreshStatus()">
                    Refresh Status
                </button>
            </div>

            <p class="text-xs text-gray-500 mt-4">If the connection drops, check the cable, switch port, and IP configuration in Basic Settings.</p>
        `;
    }
    
    /**
     * Get disconnected status HTML
     */
    getDisconnectedStatusHTML() {
        return `
            <div class="flex items-center gap-2 mb-4">
                <div class="w-2 h-2 bg-red-500 rounded-full"></div>
                <span class="font-semibold text-gray-900">DISCONNECTED</span>
                <span class="text-sm text-gray-500 ml-auto">No connection detected</span>
            </div>
            
            <div class="bg-red-50 border border-red-200 rounded-lg p-4 mb-4">
                <p class="text-sm text-red-800">The Ethernet interface is not connected. Please check:</p>
                <ul class="text-sm text-red-700 mt-2 ml-4 list-disc">
                    <li>Ethernet cable is securely plugged in</li>
                    <li>Switch or router port is active</li>
                    <li>Network interface is enabled</li>
                </ul>
            </div>

            <div class="flex gap-3">
                <button class="px-4 py-2 bg-blue-500 text-white rounded-lg text-sm font-medium hover:bg-blue-600" onclick="window.wiredUI.reconnectInterface()">
                    Try Reconnect
                </button>
                <button class="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.refreshStatus()">
                    Refresh Status
                </button>
            </div>
        `;
    }
    
    /**
     * Get basic settings section HTML
     */
    getBasicSettingsSectionHTML() {
        const settings = this.wiredManager.getBasicSettings();
        const isExpanded = this.expandedSections.has('basicSettings');
        
        return `
            <div class="bg-white rounded-lg shadow-sm border border-gray-200 mb-4 section-card">
                <div class="p-6 cursor-pointer" onclick="window.wiredUI.toggleSection('basicSettings')">
                    <div class="flex justify-between items-center">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-sliders-h text-gray-600"></i>
                            <div>
                                <h2 class="text-lg font-semibold text-gray-900">Basic Settings</h2>
                                <p class="text-sm text-gray-500">Configure basic network parameters</p>
                            </div>
                        </div>
                        <i class="fas fa-chevron-down text-gray-400 transition-transform ${isExpanded ? 'rotate-180' : ''}" id="basicSettingsIcon"></i>
                    </div>
                </div>
                
                <div id="basicSettings" class="expandable-content ${isExpanded ? 'expanded' : ''}">
                    <div class="px-6 pb-6 border-t border-gray-100 pt-6">
                        ${this.getBasicSettingsFormHTML(settings)}
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get basic settings form HTML
     */
    getBasicSettingsFormHTML(settings) {
        return `
            <div class="flex items-center gap-2 mb-6">
                <span class="px-3 py-1 bg-blue-500 text-white text-xs font-medium rounded-full">DHCP</span>
                <span class="text-xs text-gray-500">IPv4 automatic • Lease active</span>
            </div>

            <div class="grid grid-cols-2 gap-6 mb-6">
                <div>
                    <label class="block text-sm font-medium text-gray-700 mb-2">Connection mode</label>
                    <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500" id="connectionMode">
                        <option value="dhcp" ${settings.connectionMode.includes('DHCP') ? 'selected' : ''}>DHCP (Automatic)</option>
                        <option value="static" ${settings.connectionMode.includes('Static') ? 'selected' : ''}>Static IP</option>
                        <option value="pppoe" ${settings.connectionMode.includes('PPPoE') ? 'selected' : ''}>PPPoE</option>
                    </select>
                    <p class="text-xs text-gray-500 mt-1">Select how IP address is assigned</p>
                </div>
                <div>
                    <label class="block text-sm font-medium text-gray-700 mb-2">Interface priority</label>
                    <select class="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500" id="interfacePriority">
                        <option value="low" ${settings.interfacePriority.includes('Low') ? 'selected' : ''}>Low priority</option>
                        <option value="medium" ${settings.interfacePriority.includes('Medium') ? 'selected' : ''}>Medium priority</option>
                        <option value="high" ${settings.interfacePriority.includes('High') ? 'selected' : ''}>High priority</option>
                    </select>
                    <p class="text-xs text-gray-500 mt-1">Network interface priority level</p>
                </div>
            </div>

            <div class="bg-blue-50 p-4 rounded-lg mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-3">IPv4 (DHCP lease)</h3>
                <p class="text-xs text-gray-600 mb-4">Values are assigned automatically by the DHCP server</p>
                
                <div class="grid grid-cols-2 gap-4 mb-4">
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">IP address (IPv4)</label>
                        <input type="text" value="${settings.ipv4.address}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" readonly>
                        <p class="text-xs text-gray-500 mt-1">Assigned</p>
                    </div>
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Subnet mask</label>
                        <input type="text" value="${settings.ipv4.subnetMask}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" readonly>
                        <p class="text-xs text-gray-500 mt-1">CIDR / mask</p>
                    </div>
                </div>

                <div class="grid grid-cols-2 gap-4">
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Default gateway</label>
                        <input type="text" value="${settings.ipv4.gateway}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" readonly>
                        <p class="text-xs text-gray-500 mt-1">Router address</p>
                    </div>
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Lease time remaining</label>
                        <input type="text" value="${settings.ipv4.leaseTimeRemaining}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" readonly>
                        <p class="text-xs text-gray-500 mt-1">Renews automatically</p>
                    </div>
                </div>

                <div class="mt-4 text-right">
                    <a href="#" class="text-xs text-blue-600 hover:text-blue-700">Lease from DHCP server</a>
                </div>
            </div>

            <div class="bg-gray-50 p-4 rounded-lg mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-3">DNS servers (automatic)</h3>
                <p class="text-xs text-gray-600 mb-4">Servers advertised by the DHCP server for this interface</p>
                
                <div class="grid grid-cols-2 gap-4 mb-4">
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Primary DNS</label>
                        <input type="text" value="${settings.dns.primary}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" id="primaryDns">
                    </div>
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Secondary DNS</label>
                        <input type="text" value="${settings.dns.secondary}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" id="secondaryDns">
                    </div>
                </div>

                <div>
                    <label class="block text-xs text-gray-600 mb-1">Search domains</label>
                    <input type="text" placeholder="example.local, corp.internal" value="${settings.dns.searchDomains}" class="w-full px-3 py-2 bg-white border border-gray-300 rounded-lg text-sm" id="searchDomains">
                </div>

                <div class="mt-4 text-right">
                    <a href="#" class="text-xs text-blue-600 hover:text-blue-700">Automatic from DHCP</a>
                </div>
            </div>

            <p class="text-xs text-gray-500 mb-4">In DHCP mode, IP address and DNS servers are managed automatically by the network. Changing connection mode may briefly interrupt the wired connection.</p>

            <div class="flex justify-end gap-3">
                <button class="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.cancelBasicSettings()">
                    Cancel
                </button>
                <button class="px-4 py-2 bg-blue-500 text-white rounded-lg text-sm font-medium hover:bg-blue-600" onclick="window.wiredUI.saveBasicSettings()">
                    Save & Apply
                </button>
            </div>
        `;
    }
    
    /**
     * Get advanced settings section HTML
     */
    getAdvancedSettingsSectionHTML() {
        const settings = this.wiredManager.getAdvancedSettings();
        const isExpanded = this.expandedSections.has('advancedSettings');
        
        return `
            <div class="bg-white rounded-lg shadow-sm border border-gray-200 mb-4 section-card">
                <div class="p-6 cursor-pointer" onclick="window.wiredUI.toggleSection('advancedSettings')">
                    <div class="flex justify-between items-center">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-cog text-gray-600"></i>
                            <div>
                                <h2 class="text-lg font-semibold text-gray-900">Advanced Settings</h2>
                                <p class="text-sm text-gray-500">Configure advanced network parameters and protocols</p>
                            </div>
                        </div>
                        <i class="fas fa-chevron-down text-gray-400 transition-transform ${isExpanded ? 'rotate-180' : ''}" id="advancedSettingsIcon"></i>
                    </div>
                </div>
                
                <div id="advancedSettings" class="expandable-content ${isExpanded ? 'expanded' : ''}">
                    <div class="px-6 pb-6 border-t border-gray-100 pt-6">
                        ${this.getAdvancedSettingsFormHTML(settings)}
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get advanced settings form HTML
     */
    getAdvancedSettingsFormHTML(settings) {
        return `
            <div class="mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-3">VLAN Tagging</h3>
                <p class="text-xs text-gray-600 mb-4">Tag outgoing traffic with a VLAN ID when enabled</p>
                
                <div class="grid grid-cols-2 gap-4">
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">VLAN ID</label>
                        <input type="text" placeholder="Not set" value="${settings.vlan.vlanId}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm" id="vlanId">
                        <p class="text-xs text-gray-500 mt-1">1-4094, leave empty when disabled</p>
                    </div>
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Priority (802.1p)</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm" id="vlanPriority">
                            <option value="0" ${settings.vlan.priority.includes('Default') ? 'selected' : ''}>Default (0)</option>
                            <option value="1" ${settings.vlan.priority.includes('Best effort') ? 'selected' : ''}>Best effort (1)</option>
                            <option value="2" ${settings.vlan.priority.includes('Excellent effort') ? 'selected' : ''}>Excellent effort (2)</option>
                            <option value="3" ${settings.vlan.priority.includes('Critical applications') ? 'selected' : ''}>Critical applications (3)</option>
                            <option value="4" ${settings.vlan.priority.includes('Video') ? 'selected' : ''}>Video (4)</option>
                            <option value="5" ${settings.vlan.priority.includes('Voice') ? 'selected' : ''}>Voice (5)</option>
                            <option value="6" ${settings.vlan.priority.includes('Internetwork control') ? 'selected' : ''}>Internetwork control (6)</option>
                            <option value="7" ${settings.vlan.priority.includes('Network control') ? 'selected' : ''}>Network control (7)</option>
                        </select>
                        <p class="text-xs text-gray-500 mt-1">Optional traffic class</p>
                    </div>
                </div>
            </div>

            <div class="mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-3">MTU Size</h3>
                <p class="text-xs text-gray-600 mb-4">Maximum Transmission Unit for this interface</p>
                
                <div class="grid grid-cols-2 gap-4">
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">MTU mode</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm" id="mtuMode">
                            <option value="auto" ${settings.mtu.mode.includes('Automatic') ? 'selected' : ''}>Automatic</option>
                            <option value="custom" ${settings.mtu.mode.includes('Custom') ? 'selected' : ''}>Custom</option>
                        </select>
                        <p class="text-xs text-gray-500 mt-1">Choose automatic or custom value</p>
                    </div>
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Custom MTU</label>
                        <input type="text" value="${settings.mtu.customMtu}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm" id="customMtu">
                        <p class="text-xs text-gray-500 mt-1">576 - 9000 bytes</p>
                    </div>
                </div>
                <p class="text-xs text-gray-500 mt-2">Default: 1500 bytes</p>
            </div>

            <div class="mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-3">Hardware Offload</h3>
                <p class="text-xs text-gray-600 mb-4">Let the network adapter handle specific tasks</p>
                
                <div class="grid grid-cols-2 gap-4">
                    <div class="bg-blue-50 p-4 rounded-lg">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm font-medium text-gray-900">Checksum offload</span>
                            <input type="checkbox" ${settings.hardwareOffload.checksum ? 'checked' : ''} class="h-4 w-4 text-blue-600 rounded" id="checksumOffload">
                        </div>
                        <p class="text-xs text-gray-600">Reduce CPU usage for checksum calculations</p>
                    </div>
                    <div class="bg-blue-50 p-4 rounded-lg">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm font-medium text-gray-900">TCP segmentation offload</span>
                            <input type="checkbox" ${settings.hardwareOffload.tcpSegmentation ? 'checked' : ''} class="h-4 w-4 text-blue-600 rounded" id="tcpSegmentationOffload">
                        </div>
                        <p class="text-xs text-gray-600">Let the NIC split large TCP packets</p>
                    </div>
                    <div class="bg-blue-50 p-4 rounded-lg">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm font-medium text-gray-900">Receive side scaling</span>
                            <input type="checkbox" ${settings.hardwareOffload.receiveSideScaling ? 'checked' : ''} class="h-4 w-4 text-blue-600 rounded" id="receiveSideScaling">
                        </div>
                        <p class="text-xs text-gray-600">Distribute RX load across CPU cores</p>
                    </div>
                    <div class="bg-blue-50 p-4 rounded-lg">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm font-medium text-gray-900">Large send offload</span>
                            <input type="checkbox" ${settings.hardwareOffload.largeSendOffload ? 'checked' : ''} class="h-4 w-4 text-blue-600 rounded" id="largeSendOffload">
                        </div>
                        <p class="text-xs text-gray-600">Batch and offload large outbound frames</p>
                    </div>
                </div>
            </div>

            <div class="mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-3">Link Negotiation</h3>
                <p class="text-xs text-gray-600 mb-4">Control advertise speeds and duplex settings</p>
                
                <div class="grid grid-cols-2 gap-4">
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Negotiation mode</label>
                        <select class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm" id="negotiationMode">
                            <option value="auto" ${settings.linkNegotiation.mode.includes('Auto') ? 'selected' : ''}>Auto negotiate</option>
                            <option value="10-half">10 Mbps, Half duplex</option>
                            <option value="10-full">10 Mbps, Full duplex</option>
                            <option value="100-half">100 Mbps, Half duplex</option>
                            <option value="100-full">100 Mbps, Full duplex</option>
                            <option value="1000-full" ${settings.linkNegotiation.mode.includes('1 Gbps') ? 'selected' : ''}>1 Gbps, Full duplex</option>
                        </select>
                        <p class="text-xs text-gray-500 mt-1">Recommended: Auto</p>
                    </div>
                    <div>
                        <label class="block text-xs text-gray-600 mb-1">Forced speed / duplex</label>
                        <input type="text" value="${settings.linkNegotiation.forcedSpeed}" class="w-full px-3 py-2 border border-gray-300 rounded-lg text-sm" readonly>
                        <p class="text-xs text-gray-500 mt-1">Only when not using auto</p>
                    </div>
                </div>
            </div>

            <div class="bg-blue-50 p-4 rounded-lg mb-6">
                <div class="flex items-center justify-between">
                    <div>
                        <span class="text-sm font-medium text-gray-900">Energy-Efficient Ethernet (IEEE)</span>
                        <p class="text-xs text-gray-600 mt-1">Allow low-power idle when link is idle</p>
                    </div>
                    <input type="checkbox" ${settings.energyEfficientEthernet ? 'checked' : ''} class="h-4 w-4 text-blue-600 rounded" id="energyEfficientEthernet">
                </div>
            </div>

            <p class="text-xs text-gray-500 mb-4">Advanced options can affect compatibility with some switches and routers. If connectivity issues appear after changes, revert to defaults.</p>

            <div class="flex justify-end gap-3">
                <button class="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.resetAdvancedSettings()">
                    Reset to defaults
                </button>
                <button class="px-4 py-2 bg-blue-500 text-white rounded-lg text-sm font-medium hover:bg-blue-600" onclick="window.wiredUI.saveAdvancedSettings()">
                    Save & Apply
                </button>
            </div>
        `;
    }
    
    /**
     * Get connection profiles section HTML
     */
    getConnectionProfilesSectionHTML() {
        const profiles = this.wiredManager.getProfiles();
        const isExpanded = this.expandedSections.has('connectionProfiles');
        
        return `
            <div class="bg-white rounded-lg shadow-sm border border-gray-200 section-card">
                <div class="p-6 cursor-pointer" onclick="window.wiredUI.toggleSection('connectionProfiles')">
                    <div class="flex justify-between items-center">
                        <div class="flex items-center gap-3">
                            <i class="fas fa-folder text-gray-600"></i>
                            <div>
                                <h2 class="text-lg font-semibold text-gray-900">Connection Profiles</h2>
                                <p class="text-sm text-gray-500">Manage multiple network configurations</p>
                            </div>
                        </div>
                        <div class="flex items-center gap-3">
                            <span class="px-3 py-1 bg-blue-500 text-white text-xs font-medium rounded-full">Profiles</span>
                            <span class="text-sm text-gray-600">${profiles.length} saved</span>
                            <span class="text-sm text-gray-600">Active: ${this.wiredManager.getActiveProfile()?.name || 'None'}</span>
                            <i class="fas fa-chevron-down text-gray-400 transition-transform ${isExpanded ? 'rotate-180' : ''}" id="connectionProfilesIcon"></i>
                        </div>
                    </div>
                </div>
                
                <div id="connectionProfiles" class="expandable-content ${isExpanded ? 'expanded' : ''}">
                    <div class="px-6 pb-6 border-t border-gray-100 pt-6">
                        ${this.getConnectionProfilesHTML(profiles)}
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get connection profiles list HTML
     */
    getConnectionProfilesHTML(profiles) {
        return `
            <div class="mb-6">
                <h3 class="text-sm font-semibold text-gray-900 mb-2">Profile selection</h3>
                <p class="text-xs text-gray-600 mb-4">Choose which saved configuration should be applied to Ethernet 1</p>
            </div>

            <div class="grid gap-4 mb-6">
                ${profiles.map(profile => this.getProfileItemHTML(profile)).join('')}
            </div>

            <p class="text-xs text-gray-500 mb-4">Profiles store IP configuration, DNS, VLAN, and advanced options. Switching profiles will briefly restart the Ethernet link.</p>

            <div class="flex justify-end gap-3">
                <button class="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.cancelProfiles()">
                    Cancel
                </button>
                <button class="px-4 py-2 bg-blue-500 text-white rounded-lg text-sm font-medium hover:bg-blue-600" onclick="window.wiredUI.createNewProfile()">
                    Create new profile
                </button>
            </div>
        `;
    }
    
    /**
     * Get individual profile item HTML
     */
    getProfileItemHTML(profile) {
        return `
            <div class="border-2 ${profile.isActive ? 'border-blue-500 bg-blue-50' : 'border-gray-200'} rounded-lg p-4 hover:border-gray-300">
                <div class="flex items-start justify-between mb-2">
                    <div class="flex items-center gap-2">
                        <div class="w-4 h-4 border-2 ${profile.isActive ? 'border-blue-500' : 'border-gray-300'} rounded-full flex items-center justify-center">
                            ${profile.isActive ? '<div class="w-2 h-2 bg-blue-500 rounded-full"></div>' : ''}
                        </div>
                        <span class="font-semibold text-gray-900">${profile.name}</span>
                        ${profile.isActive ? '<span class="px-2 py-0.5 bg-blue-500 text-white text-xs rounded-full">Applied to Ethernet 1</span>' : ''}
                        ${profile.autoLoad ? '<span class="px-2 py-0.5 bg-green-500 text-white text-xs rounded-full">Auto-load on link up</span>' : ''}
                    </div>
                    <div class="flex gap-2">
                        ${!profile.isActive ? `<button class="px-3 py-1 bg-blue-500 text-white rounded text-xs font-medium hover:bg-blue-600" onclick="window.wiredUI.setActiveProfile('${profile.id}')">Set active</button>` : ''}
                        <button class="px-3 py-1 bg-white border border-gray-300 rounded text-xs font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.editProfile('${profile.id}')">
                            Edit
                        </button>
                        <button class="px-3 py-1 bg-white border border-gray-300 rounded text-xs font-medium text-gray-700 hover:bg-gray-50" onclick="window.wiredUI.duplicateProfile('${profile.id}')">
                            Duplicate
                        </button>
                        ${!profile.isActive ? `<button class="px-3 py-1 bg-white border border-gray-300 rounded text-xs font-medium text-red-600 hover:bg-red-50" onclick="window.wiredUI.deleteProfile('${profile.id}')">Delete</button>` : ''}
                    </div>
                </div>
                <div class="text-xs text-gray-600 ml-6">
                    <p>${profile.config.ipv4Mode} • ${profile.config.ipv6Enabled ? 'IPv6 enabled' : 'IPv6 disabled'} • MTU ${profile.config.mtu}</p>
                    <p class="text-gray-500">Last used: ${profile.lastUsed} • ${profile.gateway ? `Gateway: ${profile.gateway}` : profile.appliesWhen || 'Not auto-selected'}</p>
                </div>
                ${profile.isActive ? `
                    <div class="ml-6 mt-2">
                        <button class="text-xs text-blue-600 hover:text-blue-700 font-medium mr-3" onclick="window.wiredUI.disableAutoSwitch('${profile.id}')">Disable auto-switch</button>
                    </div>
                ` : ''}
            </div>
        `;
    }
    
    /**
     * Setup event listeners
     */
    setupEventListeners() {
        // Make UI instance globally available for onclick handlers
        window.wiredUI = this;
    }
    
    /**
     * Initialize toggle switch
     */
    initializeToggleSwitch() {
        const toggle = document.getElementById('ethernetToggle');
        if (toggle) {
            toggle.addEventListener('change', async (e) => {
                const result = await this.wiredManager.toggleEthernet(e.target.checked);
                if (result.success) {
                    this.showNotification(result.message, 'success');
                    this.render(); // Re-render to update status
                } else {
                    this.showNotification(result.error, 'error');
                    e.target.checked = !e.target.checked; // Revert toggle
                }
            });
        }
    }
    
    /**
     * Toggle section expansion
     */
    toggleSection(sectionId) {
        const content = document.getElementById(sectionId);
        const icon = document.getElementById(sectionId + 'Icon');
        
        if (content && icon) {
            const isExpanded = this.expandedSections.has(sectionId);
            
            if (isExpanded) {
                this.expandedSections.delete(sectionId);
                content.classList.remove('expanded');
                icon.classList.remove('rotate-180');
            } else {
                this.expandedSections.add(sectionId);
                content.classList.add('expanded');
                icon.classList.add('rotate-180');
            }
        }
    }
    
    /**
     * Update connection status display
     */
    updateConnectionStatus() {
        // This would be called periodically to update real-time status
        // For now, it's handled in render()
    }
    
    /**
     * Handle ethernet toggle
     */
    async toggleEthernet(enabled) {
        const result = await this.wiredManager.toggleEthernet(enabled);
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle reconnect interface
     */
    async reconnectInterface() {
        this.showNotification('Reconnecting interface...', 'info');
        const result = await this.wiredManager.reconnectInterface();
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle renew DHCP lease
     */
    async renewDhcpLease() {
        this.showNotification('Renewing DHCP lease...', 'info');
        const result = await this.wiredManager.renewDhcpLease();
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle refresh status
     */
    async refreshStatus() {
        this.showNotification('Refreshing status...', 'info');
        const result = await this.wiredManager.refreshStatus();
        if (result.success) {
            this.showNotification('Status updated', 'success');
            this.render();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle save basic settings
     */
    async saveBasicSettings() {
        const settings = {
            connectionMode: document.getElementById('connectionMode').value,
            interfacePriority: document.getElementById('interfacePriority').value,
            dns: {
                primary: document.getElementById('primaryDns').value,
                secondary: document.getElementById('secondaryDns').value,
                searchDomains: document.getElementById('searchDomains').value
            }
        };
        
        this.showNotification('Saving basic settings...', 'info');
        const result = await this.wiredManager.updateBasicSettings(settings);
        if (result.success) {
            this.showNotification(result.message, 'success');
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle cancel basic settings
     */
    cancelBasicSettings() {
        this.render();
        this.showNotification('Changes cancelled', 'info');
    }
    
    /**
     * Handle save advanced settings
     */
    async saveAdvancedSettings() {
        const settings = {
            vlan: {
                vlanId: document.getElementById('vlanId').value,
                priority: document.getElementById('vlanPriority').value
            },
            mtu: {
                mode: document.getElementById('mtuMode').value,
                customMtu: document.getElementById('customMtu').value
            },
            hardwareOffload: {
                checksum: document.getElementById('checksumOffload').checked,
                tcpSegmentation: document.getElementById('tcpSegmentationOffload').checked,
                receiveSideScaling: document.getElementById('receiveSideScaling').checked,
                largeSendOffload: document.getElementById('largeSendOffload').checked
            },
            linkNegotiation: {
                mode: document.getElementById('negotiationMode').value
            },
            energyEfficientEthernet: document.getElementById('energyEfficientEthernet').checked
        };
        
        this.showNotification('Saving advanced settings...', 'info');
        const result = await this.wiredManager.updateAdvancedSettings(settings);
        if (result.success) {
            this.showNotification(result.message, 'success');
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle reset advanced settings
     */
    resetAdvancedSettings() {
        this.wiredManager.initializeDefaultData();
        this.render();
        this.showNotification('Advanced settings reset to defaults', 'info');
    }
    
    /**
     * Handle set active profile
     */
    async setActiveProfile(profileId) {
        this.showNotification('Switching profile...', 'info');
        const result = await this.wiredManager.switchProfile(profileId);
        if (result.success) {
            this.showNotification(result.message, 'success');
            this.render();
        } else {
            this.showNotification(result.error, 'error');
        }
    }
    
    /**
     * Handle edit profile
     */
    editProfile(profileId) {
        this.showNotification(`Editing profile: ${profileId}`, 'info');
        // TODO: Implement profile editing modal
    }
    
    /**
     * Handle duplicate profile
     */
    duplicateProfile(profileId) {
        this.showNotification(`Duplicating profile: ${profileId}`, 'info');
        // TODO: Implement profile duplication
    }
    
    /**
     * Handle delete profile
     */
    async deleteProfile(profileId) {
        if (confirm('Are you sure you want to delete this profile?')) {
            this.showNotification('Deleting profile...', 'info');
            const result = await this.wiredManager.deleteProfile(profileId);
            if (result.success) {
                this.showNotification(result.message, 'success');
                this.render();
            } else {
                this.showNotification(result.error, 'error');
            }
        }
    }
    
    /**
     * Handle disable auto switch
     */
    disableAutoSwitch(profileId) {
        this.showNotification(`Disabling auto-switch for: ${profileId}`, 'info');
        // TODO: Implement auto-switch disable
    }
    
    /**
     * Handle create new profile
     */
    createNewProfile() {
        this.showNotification('Opening profile creation dialog...', 'info');
        // TODO: Implement profile creation modal
    }
    
    /**
     * Handle cancel profiles
     */
    cancelProfiles() {
        this.render();
        this.showNotification('Changes cancelled', 'info');
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
     * Get time ago string
     */
    getTimeAgo(date) {
        const seconds = Math.floor((new Date() - date) / 1000);
        
        if (seconds < 5) return 'Just now';
        if (seconds < 60) return `${seconds} seconds ago`;
        
        const minutes = Math.floor(seconds / 60);
        if (minutes < 60) return `${minutes} minute${minutes > 1 ? 's' : ''} ago`;
        
        const hours = Math.floor(minutes / 60);
        if (hours < 24) return `${hours} hour${hours > 1 ? 's' : ''} ago`;
        
        const days = Math.floor(hours / 24);
        return `${days} day${days > 1 ? 's' : ''} ago`;
    }
    
    /**
     * Cleanup method
     */
    destroy() {
        // Remove global reference
        if (window.wiredUI === this) {
            delete window.wiredUI;
        }
    }
}

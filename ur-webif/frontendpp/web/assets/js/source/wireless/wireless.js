/**
 * Wireless Configuration Data Manager
 * Handles all data operations for wireless network configuration
 */

export class WirelessManager {
    constructor(httpClient) {
        this.httpClient = httpClient;
        this.isWiFiEnabled = true;
        this.currentMode = 'accessPoint'; // 'accessPoint' or 'client'
        this.accessPointSettings = null;
        this.clientSettings = null;
        this.availableNetworks = [];
        this.savedNetworks = [];
        this.connectedDevices = [];
        
        this.initializeDefaultData();
    }
    
    /**
     * Initialize default data for demo purposes
     */
    initializeDefaultData() {
        this.accessPointSettings = {
            ssid: 'MyDevice-AP',
            password: 'mypassword123',
            band: 'dual', // 'dual', '2.4', '5'
            channel: 'auto', // 'auto', '1', '6', '11'
            security: 'wpa2-psk', // 'wpa2-psk', 'wpa3', 'open'
            maxClients: 10,
            broadcastSSID: true,
            guestIsolation: true,
            status: {
                enabled: true,
                channel: 6,
                throughput: { current: 48, peak: 96 }
            }
        };
        
        this.clientSettings = {
            currentNetwork: null,
            scanning: false,
            autoConnect: true
        };
        
        this.availableNetworks = [
            {
                id: 'home-office',
                ssid: 'Home-Office',
                signal: 'strong',
                security: 'WPA2',
                band: '2.4 / 5 GHz',
                connected: true,
                signalStrength: -45,
                frequency: '5 GHz'
            },
            {
                id: 'guest-wifi',
                ssid: 'Guest-WiFi',
                signal: 'medium',
                security: 'WPA2',
                band: '5 GHz',
                connected: false,
                signalStrength: -68,
                frequency: '5 GHz'
            },
            {
                id: 'cafe-nextdoor',
                ssid: 'Cafe-Nextdoor',
                signal: 'weak',
                security: 'Open',
                band: 'Public',
                connected: false,
                signalStrength: -78,
                frequency: '2.4 GHz'
            }
        ];
        
        this.savedNetworks = [
            {
                id: 'home-office-saved',
                ssid: 'Home-Office',
                priority: 'preferred',
                autoConnect: true,
                band: '2.4 / 5 GHz',
                security: 'WPA2',
                password: '********'
            },
            {
                id: 'guest-wifi-saved',
                ssid: 'Guest-WiFi',
                priority: 'saved',
                autoConnect: false,
                band: '5 GHz',
                security: 'WPA2',
                password: '********'
            },
            {
                id: 'cafe-nextdoor-saved',
                ssid: 'Cafe-Nextdoor',
                priority: 'public',
                autoConnect: false,
                band: 'Public',
                security: 'Open',
                password: null
            }
        ];
        
        this.connectedDevices = [
            {
                id: 'device-1',
                name: 'Work Laptop',
                macAddress: 'AA:BB:CC:DD:EE:01',
                band: '5 GHz',
                signalQuality: 'excellent',
                signalDbm: -45,
                trafficToday: '2.4 GB',
                trafficLive: '6 Mbps',
                ipAddress: '192.168.1.24',
                leaseType: 'Static lease'
            },
            {
                id: 'device-2',
                name: "Alice's Phone",
                macAddress: 'AA:BB:CC:DD:EE:02',
                band: '5 GHz',
                signalQuality: 'good',
                signalDbm: -68,
                trafficToday: '860 MB',
                trafficLive: '3 Mbps',
                ipAddress: '192.168.1.37',
                leaseType: 'DHCP lease'
            },
            {
                id: 'device-3',
                name: 'Living Room TV',
                macAddress: 'AA:BB:CC:DD:EE:03',
                band: '2.4 GHz',
                signalQuality: 'fair',
                signalDbm: -72,
                trafficToday: '3.1 GB',
                trafficLive: '5 Mbps',
                ipAddress: '192.168.1.45',
                leaseType: 'DHCP lease'
            }
        ];
    }
    
    /**
     * Get WiFi radio status
     */
    getWiFiStatus() {
        return {
            enabled: this.isWiFiEnabled,
            mode: this.currentMode
        };
    }
    
    /**
     * Get access point settings
     */
    getAccessPointSettings() {
        return this.accessPointSettings;
    }
    
    /**
     * Get client settings
     */
    getClientSettings() {
        return this.clientSettings;
    }
    
    /**
     * Get available networks
     */
    getAvailableNetworks() {
        return this.availableNetworks;
    }
    
    /**
     * Get saved networks
     */
    getSavedNetworks() {
        return this.savedNetworks;
    }
    
    /**
     * Get connected devices
     */
    getConnectedDevices() {
        return this.connectedDevices;
    }
    
    /**
     * Toggle WiFi radio
     */
    async toggleWiFi(enabled) {
        try {
            
            this.isWiFiEnabled = enabled;
            
            // Simulate API delay
            await new Promise(resolve => setTimeout(resolve, 500));
            
            return { 
                success: true, 
                message: `WiFi ${enabled ? 'enabled' : 'disabled'}`,
                enabled: this.isWiFiEnabled
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to toggle WiFi:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Switch wireless mode
     */
    async switchMode(mode) {
        try {
            
            if (!['accessPoint', 'client'].includes(mode)) {
                throw new Error('Invalid mode. Must be "accessPoint" or "client"');
            }
            
            this.currentMode = mode;
            
            await new Promise(resolve => setTimeout(resolve, 300));
            
            return { 
                success: true, 
                message: `Switched to ${mode === 'accessPoint' ? 'Access Point' : 'Client'} mode`,
                mode: this.currentMode
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to switch mode:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Update access point settings
     */
    async updateAccessPointSettings(settings) {
        try {
            
            this.accessPointSettings = { ...this.accessPointSettings, ...settings };
            
            await new Promise(resolve => setTimeout(resolve, 500));
            
            return { success: true, message: 'Access point settings updated successfully' };
        } catch (error) {
            console.error('[WIRELESS] Failed to update access point settings:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Scan for available networks
     */
    async scanNetworks() {
        try {
            
            this.clientSettings.scanning = true;
            
            // Simulate scanning process
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            // Randomize signal strengths slightly for demo
            this.availableNetworks = this.availableNetworks.map(network => ({
                ...network,
                signalStrength: network.signalStrength + Math.floor(Math.random() * 5) - 2
            }));
            
            this.clientSettings.scanning = false;
            
            return { 
                success: true, 
                message: `Found ${this.availableNetworks.length} networks`,
                networks: this.availableNetworks
            };
        } catch (error) {
            this.clientSettings.scanning = false;
            console.error('[WIRELESS] Failed to scan networks:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Connect to a network
     */
    async connectToNetwork(networkId, password = null) {
        try {
            
            const network = this.availableNetworks.find(n => n.id === networkId);
            if (!network) {
                throw new Error('Network not found');
            }
            
            // Simulate connection process
            await new Promise(resolve => setTimeout(resolve, 3000));
            
            // Update network status
            this.availableNetworks.forEach(n => n.connected = false);
            network.connected = true;
            
            // Set as current network
            this.clientSettings.currentNetwork = network;
            
            // Add to saved networks if not already there
            if (!this.savedNetworks.find(n => n.ssid === network.ssid)) {
                this.savedNetworks.unshift({
                    id: network.id + '-saved',
                    ssid: network.ssid,
                    priority: 'saved',
                    autoConnect: false,
                    band: network.band,
                    security: network.security,
                    password: password ? '********' : null
                });
            }
            
            return { 
                success: true, 
                message: `Connected to ${network.ssid}`,
                network: network
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to connect to network:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Disconnect from current network
     */
    async disconnectFromNetwork() {
        try {
            
            if (!this.clientSettings.currentNetwork) {
                throw new Error('No network currently connected');
            }
            
            const networkName = this.clientSettings.currentNetwork.ssid;
            
            // Simulate disconnection
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Update network status
            this.availableNetworks.forEach(n => n.connected = false);
            this.clientSettings.currentNetwork = null;
            
            return { 
                success: true, 
                message: `Disconnected from ${networkName}`
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to disconnect from network:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Remove saved network
     */
    async removeSavedNetwork(networkId) {
        try {
            
            const networkIndex = this.savedNetworks.findIndex(n => n.id === networkId);
            if (networkIndex === -1) {
                throw new Error('Saved network not found');
            }
            
            const network = this.savedNetworks[networkIndex];
            this.savedNetworks.splice(networkIndex, 1);
            
            await new Promise(resolve => setTimeout(resolve, 300));
            
            return { 
                success: true, 
                message: `Removed ${network.ssid} from saved networks`
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to remove saved network:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Disconnect a device from access point
     */
    async disconnectDevice(deviceId) {
        try {
            
            const deviceIndex = this.connectedDevices.findIndex(d => d.id === deviceId);
            if (deviceIndex === -1) {
                throw new Error('Device not found');
            }
            
            const device = this.connectedDevices[deviceIndex];
            
            await new Promise(resolve => setTimeout(resolve, 500));
            
            this.connectedDevices.splice(deviceIndex, 1);
            
            return { 
                success: true, 
                message: `Disconnected ${device.name}`
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to disconnect device:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Get network statistics
     */
    getNetworkStatistics() {
        const connectedCount = this.connectedDevices.length;
        const currentThroughput = this.accessPointSettings.status.throughput.current;
        const peakThroughput = this.accessPointSettings.status.throughput.peak;
        
        return {
            connectedDevices: connectedCount,
            currentThroughput: currentThroughput,
            peakThroughput: peakThroughput
        };
    }
    
    /**
     * Forget all saved networks
     */
    async forgetAllNetworks() {
        try {
            
            await new Promise(resolve => setTimeout(resolve, 300));
            
            this.savedNetworks = [];
            
            return { 
                success: true, 
                message: 'All saved networks have been forgotten'
            };
        } catch (error) {
            console.error('[WIRELESS] Failed to forget all networks:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Get signal quality text
     */
    getSignalQualityText(dbm) {
        if (dbm >= -50) return 'Excellent';
        if (dbm >= -60) return 'Good';
        if (dbm >= -70) return 'Fair';
        return 'Poor';
    }
    
    /**
     * Get signal color class
     */
    getSignalColorClass(dbm) {
        if (dbm >= -50) return 'text-green-600';
        if (dbm >= -60) return 'text-yellow-600';
        if (dbm >= -70) return 'text-orange-600';
        return 'text-red-600';
    }
}

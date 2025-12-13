/**
 * Wired Configuration Data Manager
 * Handles all data operations for wired network configuration
 */

export class WiredManager {
    constructor(httpClient) {
        this.httpClient = httpClient;
        this.connectionStatus = null;
        this.basicSettings = null;
        this.advancedSettings = null;
        this.profiles = [];
        this.isEthernetEnabled = true;
        
        this.initializeDefaultData();
    }
    
    /**
     * Initialize default data for demo purposes
     */
    initializeDefaultData() {
        this.connectionStatus = {
            connected: true,
            interface: 'Ethernet 1',
            connectionType: 'DHCP (IPv4)',
            ipAddress: '192.168.0.24',
            macAddress: 'A4:9B:4C:2F:BD:11',
            linkSpeed: '1 Gbps',
            sessionDuration: '02:14:37',
            gateway: '192.168.0.1',
            externalIp: '203.0.113.42',
            dnsServers: '8.8.8.8, 8.8.4.4',
            networkSpeed: { download: 220, upload: 18 },
            duplex: 'Full duplex',
            lastUpdated: new Date()
        };
        
        this.basicSettings = {
            connectionMode: 'DHCP (Automatic)',
            interfacePriority: 'Medium priority',
            ipv4: {
                address: '192.168.0.24',
                subnetMask: '255.255.255.0',
                gateway: '192.168.0.1',
                leaseTimeRemaining: '01:45:12'
            },
            dns: {
                primary: '8.8.8.8',
                secondary: '8.8.4.4',
                searchDomains: ''
            }
        };
        
        this.advancedSettings = {
            vlan: {
                enabled: false,
                vlanId: '',
                priority: 'Default (0)'
            },
            mtu: {
                mode: 'Automatic',
                customMtu: '1500'
            },
            hardwareOffload: {
                checksum: true,
                tcpSegmentation: true,
                receiveSideScaling: true,
                largeSendOffload: true
            },
            linkNegotiation: {
                mode: 'Auto negotiate',
                forcedSpeed: '1 Gbps, full duplex'
            },
            energyEfficientEthernet: true
        };
        
        this.profiles = [
            {
                id: 'home-network',
                name: 'Home network',
                isActive: true,
                autoLoad: true,
                config: {
                    ipv4Mode: 'DHCP',
                    ipv6Enabled: false,
                    mtu: 1500,
                    vlan: null
                },
                lastUsed: 'Just now',
                gateway: '192.168.0.1'
            },
            {
                id: 'office-vlan-20',
                name: 'Office VLAN 20',
                isActive: false,
                autoLoad: false,
                config: {
                    ipv4Mode: 'Static',
                    ipv6Enabled: false,
                    mtu: 1500,
                    vlan: { id: 20, priority: 0 }
                },
                lastUsed: '2 days ago',
                appliesWhen: 'Port 2 link'
            },
            {
                id: 'lab-static',
                name: 'Lab static',
                isActive: false,
                autoLoad: false,
                config: {
                    ipv4Mode: 'Static',
                    ipv6Enabled: false,
                    mtu: 9000,
                    gateway: null
                },
                lastUsed: '1 week ago'
            }
        ];
    }
    
    /**
     * Get connection status
     */
    getConnectionStatus() {
        return this.connectionStatus;
    }
    
    /**
     * Get basic settings
     */
    getBasicSettings() {
        return this.basicSettings;
    }
    
    /**
     * Get advanced settings
     */
    getAdvancedSettings() {
        return this.advancedSettings;
    }
    
    /**
     * Get all profiles
     */
    getProfiles() {
        return this.profiles;
    }
    
    /**
     * Get active profile
     */
    getActiveProfile() {
        return this.profiles.find(profile => profile.isActive);
    }
    
    /**
     * Update basic settings
     */
    async updateBasicSettings(settings) {
        try {
            // Simulate API call
            
            // Update local data
            this.basicSettings = { ...this.basicSettings, ...settings };
            
            // Simulate API delay
            await new Promise(resolve => setTimeout(resolve, 500));
            
            return { success: true, message: 'Settings updated successfully' };
        } catch (error) {
            console.error('[WIRED] Failed to update basic settings:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Update advanced settings
     */
    async updateAdvancedSettings(settings) {
        try {
            
            this.advancedSettings = { ...this.advancedSettings, ...settings };
            
            await new Promise(resolve => setTimeout(resolve, 500));
            
            return { success: true, message: 'Advanced settings updated successfully' };
        } catch (error) {
            console.error('[WIRED] Failed to update advanced settings:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Switch to a different profile
     */
    async switchProfile(profileId) {
        try {
            
            // Deactivate all profiles
            this.profiles.forEach(profile => profile.isActive = false);
            
            // Activate the selected profile
            const profile = this.profiles.find(p => p.id === profileId);
            if (profile) {
                profile.isActive = true;
                profile.lastUsed = 'Just now';
            }
            
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            return { success: true, message: `Switched to ${profile?.name} profile` };
        } catch (error) {
            console.error('[WIRED] Failed to switch profile:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Create a new profile
     */
    async createProfile(profileData) {
        try {
            const newProfile = {
                id: profileData.name.toLowerCase().replace(/\s+/g, '-'),
                name: profileData.name,
                isActive: false,
                autoLoad: false,
                config: profileData.config,
                lastUsed: 'Never'
            };
            
            this.profiles.push(newProfile);
            
            await new Promise(resolve => setTimeout(resolve, 300));
            
            return { success: true, profile: newProfile };
        } catch (error) {
            console.error('[WIRED] Failed to create profile:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Delete a profile
     */
    async deleteProfile(profileId) {
        try {
            const profileIndex = this.profiles.findIndex(p => p.id === profileId);
            if (profileIndex === -1) {
                throw new Error('Profile not found');
            }
            
            const profile = this.profiles[profileIndex];
            
            // Don't allow deletion of active profile
            if (profile.isActive) {
                throw new Error('Cannot delete active profile');
            }
            
            this.profiles.splice(profileIndex, 1);
            
            await new Promise(resolve => setTimeout(resolve, 300));
            
            return { success: true, message: `Deleted ${profile.name} profile` };
        } catch (error) {
            console.error('[WIRED] Failed to delete profile:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Toggle ethernet interface
     */
    async toggleEthernet(enabled) {
        try {
            
            this.isEthernetEnabled = enabled;
            
            if (!enabled) {
                // Clear connection status when disabled
                this.connectionStatus.connected = false;
            } else {
                // Restore connection status when enabled
                this.connectionStatus.connected = true;
            }
            
            await new Promise(resolve => setTimeout(resolve, 500));
            
            return { 
                success: true, 
                message: `Ethernet ${enabled ? 'enabled' : 'disabled'}`,
                enabled: this.isEthernetEnabled
            };
        } catch (error) {
            console.error('[WIRED] Failed to toggle ethernet:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Renew DHCP lease
     */
    async renewDhcpLease() {
        try {
            
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            // Update lease time
            this.basicSettings.ipv4.leaseTimeRemaining = '12:00:00';
            
            return { success: true, message: 'DHCP lease renewed successfully' };
        } catch (error) {
            console.error('[WIRED] Failed to renew DHCP lease:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Reconnect interface
     */
    async reconnectInterface() {
        try {
            
            // Simulate disconnection
            this.connectionStatus.connected = false;
            
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Simulate reconnection
            this.connectionStatus.connected = true;
            this.connectionStatus.sessionDuration = '00:00:01';
            this.connectionStatus.lastUpdated = new Date();
            
            return { success: true, message: 'Interface reconnected successfully' };
        } catch (error) {
            console.error('[WIRED] Failed to reconnect interface:', error);
            return { success: false, error: error.message };
        }
    }
    
    /**
     * Refresh connection status
     */
    async refreshStatus() {
        try {
            
            await new Promise(resolve => setTimeout(resolve, 500));
            
            this.connectionStatus.lastUpdated = new Date();
            
            return { success: true, status: this.connectionStatus };
        } catch (error) {
            console.error('[WIRED] Failed to refresh status:', error);
            return { success: false, error: error.message };
        }
    }
}

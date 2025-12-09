/**
 * Cellular Configuration Data Manager
 * Handles all data operations for cellular network configuration
 */

export class CellularManager {
    constructor(httpClient) {
        this.httpClient = httpClient;
        this.isCellularEnabled = false;
        this.connectionStatus = 'disconnected'; // 'disconnected', 'connecting', 'connected'
        this.currentNetwork = null;
        this.signalStrength = null;
        this.dataUsage = { current: 0, total: 0 };
        this.simInfo = null;
        this.deviceInfo = null;
        this.availableNetworks = [];
        this.configuration = {
            networkType: 'auto', // 'auto', 'lte', '3g', '5g'
            networkSelection: 'automatic', // 'automatic', 'manual'
            apn: {
                name: '',
                authType: 'none', // 'none', 'pap', 'chap'
                username: '',
                password: ''
            },
            autoConnect: true,
            dataRoaming: false,
            dataLimit: {
                enabled: false,
                limit: 0 // MB
            }
        };
        
        this.initializeDefaultData();
    }
    
    /**
     * Initialize default data for demo purposes
     */
    initializeDefaultData() {
        this.deviceInfo = {
            name: 'Edge Gateway',
            model: 'EG-500',
            serialNumber: 'SN-1234-5678',
            imei: null,
            modemChipset: 'Quectel RG500Q',
            firmwareVersion: '1.2.3-build45',
            bootloader: 'v0.9.7',
            hardwareRevision: 'Rev B'
        };
        
        this.simInfo = {
            available: false,
            slot: 1,
            operator: null,
            iccid: null,
            lastSeen: null
        };
        
        this.availableNetworks = [
            {
                id: 'network-1',
                operator: 'ExampleTel',
                technology: 'LTE',
                signal: 'excellent',
                status: 'available'
            },
            {
                id: 'network-2',
                operator: 'TestMobile',
                technology: '5G',
                signal: 'good',
                status: 'available'
            },
            {
                id: 'network-3',
                operator: 'DemoCom',
                technology: 'LTE',
                signal: 'fair',
                status: 'available'
            }
        ];
    }
    
    /**
     * Toggle cellular interface
     */
    toggleCellular() {
        this.isCellularEnabled = !this.isCellularEnabled;
        if (!this.isCellularEnabled) {
            this.connectionStatus = 'disconnected';
            this.currentNetwork = null;
        }
        return this.isCellularEnabled;
    }
    
    /**
     * Get cellular enabled status
     */
    isCellularInterfaceEnabled() {
        return this.isCellularEnabled;
    }
    
    /**
     * Get connection status
     */
    getConnectionStatus() {
        return {
            status: this.connectionStatus,
            network: this.currentNetwork,
            signal: this.signalStrength,
            dataUsage: this.dataUsage,
            lastAttempt: new Date().toISOString()
        };
    }
    
    /**
     * Connect to cellular network
     */
    async connectToNetwork() {
        if (!this.isCellularEnabled) {
            throw new Error('Cellular interface is disabled');
        }
        
        if (!this.simInfo.available) {
            throw new Error('No SIM card available');
        }
        
        this.connectionStatus = 'connecting';
        
        // Simulate connection process
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        // Simulate successful connection
        this.connectionStatus = 'connected';
        this.currentNetwork = {
            operator: 'ExampleTel',
            technology: 'LTE',
            ipAddress: '10.24.18.6',
            roaming: false,
            apn: this.configuration.apn.name || 'Internet'
        };
        
        this.signalStrength = {
            rssi: -78,
            rsrp: -101,
            rsrq: -9,
            status: 'Good signal strength',
            bars: 4
        };
        
        return this.currentNetwork;
    }
    
    /**
     * Disconnect from cellular network
     */
    disconnectNetwork() {
        this.connectionStatus = 'disconnected';
        this.currentNetwork = null;
        this.signalStrength = null;
        this.dataUsage.current = 0;
    }
    
    /**
     * Scan for available networks
     */
    async scanNetworks() {
        // Simulate network scanning
        await new Promise(resolve => setTimeout(resolve, 3000));
        
        return this.availableNetworks.map(network => ({
            ...network,
            signalBars: this.getSignalBars(network.signal)
        }));
    }
    
    /**
     * Get signal bars count based on signal quality
     */
    getSignalBars(signal) {
        const signalMap = {
            'excellent': 4,
            'good': 3,
            'fair': 2,
            'poor': 1
        };
        return signalMap[signal] || 0;
    }
    
    /**
     * Select network manually
     */
    selectNetwork(networkId) {
        const network = this.availableNetworks.find(n => n.id === networkId);
        if (network) {
            this.configuration.networkSelection = 'manual';
            this.configuration.selectedNetwork = network;
            return network;
        }
        throw new Error('Network not found');
    }
    
    /**
     * Get SIM information
     */
    getSimInfo() {
        return {
            ...this.simInfo,
            slots: [
                { id: 1, status: this.simInfo.available ? 'occupied' : 'empty', primary: true },
                { id: 2, status: 'empty', primary: false }
            ]
        };
    }
    
    /**
     * Get device information
     */
    getDeviceInfo() {
        return { ...this.deviceInfo };
    }
    
    /**
     * Update configuration
     */
    updateConfiguration(newConfig) {
        this.configuration = { ...this.configuration, ...newConfig };
        return this.configuration;
    }
    
    /**
     * Get current configuration
     */
    getConfiguration() {
        return { ...this.configuration };
    }
    
    /**
     * Save configuration to backend
     */
    async saveConfiguration() {
        // Simulate API call
        await new Promise(resolve => setTimeout(resolve, 1000));
        console.log('[CELLULAR] Configuration saved:', this.configuration);
        return true;
    }
    
    /**
     * Reset configuration to defaults
     */
    resetToDefaults() {
        this.configuration = {
            networkType: 'auto',
            networkSelection: 'automatic',
            apn: {
                name: '',
                authType: 'none',
                username: '',
                password: ''
            },
            autoConnect: true,
            dataRoaming: false,
            dataLimit: {
                enabled: false,
                limit: 0
            }
        };
        return this.configuration;
    }
    
    /**
     * Get data usage statistics
     */
    getDataUsage() {
        return {
            current: this.dataUsage.current,
            total: this.dataUsage.total,
            session: this.dataUsage.current,
            limit: this.configuration.dataLimit.enabled ? this.configuration.dataLimit.limit : null,
            percentage: this.configuration.dataLimit.enabled 
                ? Math.round((this.dataUsage.current / this.configuration.dataLimit.limit) * 100)
                : 0
        };
    }
    
    /**
     * Reset data usage counters
     */
    resetDataUsage() {
        this.dataUsage = { current: 0, total: 0 };
        return this.dataUsage;
    }
    
    /**
     * Update data usage (simulated)
     */
    updateDataUsage() {
        if (this.connectionStatus === 'connected') {
            // Simulate data usage increase
            this.dataUsage.current += Math.random() * 0.1; // MB
            this.dataUsage.total += Math.random() * 0.1; // MB
        }
        return this.getDataUsage();
    }
}

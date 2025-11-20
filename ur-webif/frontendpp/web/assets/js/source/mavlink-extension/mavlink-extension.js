/**
 * MAVLink Extension Manager
 * Core functionality for managing MAVLink endpoints and system operations
 */

export class MavlinkExtensionManager {
    constructor() {
        this.endpoints = new Map();
        this.statistics = {
            online: 0,
            warning: 0,
            offline: 0
        };
        this.alerts = [];
        this._isInitialized = false;
        this.updateInterval = null;
        this.eventListeners = new Map();
        this.requestTimeout = null;
        this._isDataLoaded = false;
        this.dataRequestPromise = null;
        
        this.init();
    }

    async init() {
        try {
            console.log('[MAVLINK-EXTENSION] Initializing MAVLink extension manager...');
            
            // Always try to request data from backend via WebSocket
            // But ensure initialization completes even if it fails
            try {
                await this.requestMavLinkExtensionData();
            } catch (error) {
                console.warn('[MAVLINK-EXTENSION] Backend request failed, using demo data:', error);
                this.loadDemoEndpoints();
                this._isDataLoaded = true;
                this.emit('dataLoadTimeout');
            }
            
            // Start real-time updates
            this.startRealTimeUpdates();
            
            this._isInitialized = true;
            this.emit('initialized');
            
            console.log('[MAVLINK-EXTENSION] MAVLink extension manager initialized successfully');
        } catch (error) {
            console.error('[MAVLINK-EXTENSION] Failed to initialize:', error);
            
            // Ensure initialization always completes
            this.loadDemoEndpoints();
            this._isDataLoaded = true;
            this._isInitialized = true;
            this.emit('initialized');
            this.emit('dataLoadTimeout');
            
            this.emit('error', error);
        }
    }

    /**
     * Request MAVLink extension data from backend via WebSocket
     */
    async requestMavLinkExtensionData() {
        return new Promise((resolve, reject) => {
            try {
                // Get WebSocket client from source manager
                const sourceManager = window.sourceManager;
                if (!sourceManager || !sourceManager.wsClient) {
                    console.warn('[MAVLINK-EXTENSION] WebSocket client not available, using demo data');
                    this.loadDemoEndpoints();
                    this._isDataLoaded = true;
                    resolve();
                    return;
                }

                // Check if WebSocket is connected
                if (!sourceManager.wsClient.isClientConnected()) {
                    console.warn('[MAVLINK-EXTENSION] WebSocket not connected, using demo data');
                    this.loadDemoEndpoints();
                    this._isDataLoaded = true;
                    resolve();
                    return;
                }

                console.log('[MAVLINK-EXTENSION] Requesting MAVLink extension data from backend...');
                
                // Set up timeout for 1 second
                this.requestTimeout = setTimeout(() => {
                    console.warn('[MAVLINK-EXTENSION] Backend did not respond within 1 second, using demo data');
                    this.loadDemoEndpoints();
                    this._isDataLoaded = true;
                    this.emit('dataLoadTimeout');
                    resolve();
                }, 1000);

                // Set up one-time listener for the response
                const handleResponse = (event) => {
                    const data = event.detail;
                    
                    // Check if this is our response
                    if (data.type === 'mavlink_extension_data_response') {
                        clearTimeout(this.requestTimeout);
                        document.removeEventListener('websocketMessage', handleResponse);
                        
                        if (data.data) {
                            console.log('[MAVLINK-EXTENSION] Received MAVLink extension data from backend');
                            this.processBackendData(data.data);
                            this._isDataLoaded = true;
                            this.emit('dataLoaded');
                        } else {
                            console.warn('[MAVLINK-EXTENSION] No data in response, using demo data');
                            this.loadDemoEndpoints();
                            this._isDataLoaded = true;
                        }
                        resolve();
                    }
                };

                // Listen for WebSocket messages
                document.addEventListener('websocketMessage', handleResponse);

                // Send the request
                sourceManager.wsClient.send({
                    type: 'request_mavlink_extension_data',
                    timestamp: Date.now()
                });

            } catch (error) {
                console.error('[MAVLINK-EXTENSION] Error requesting MAVLink extension data:', error);
                this.loadDemoEndpoints();
                this._isDataLoaded = true;
                reject(error);
            }
        });
    }

    /**
     * Process backend data and update endpoints
     */
    processBackendData(data) {
        try {
            // Clear existing endpoints
            this.endpoints.clear();

            // Process endpoints from backend
            if (data.endpoints && Array.isArray(data.endpoints)) {
                data.endpoints.forEach(endpoint => {
                    this.endpoints.set(endpoint.id, {
                        ...endpoint,
                        lastSeen: endpoint.lastSeen ? new Date(endpoint.lastSeen) : new Date()
                    });
                });
            }

            // Update statistics
            this.updateStatistics();

            // Emit data update event
            this.emit('dataUpdated', {
                endpoints: Array.from(this.endpoints.values()),
                statistics: this.statistics
            });

        } catch (error) {
            console.error('[MAVLINK-EXTENSION] Error processing backend data:', error);
            throw error;
        }
    }

    /**
     * Check if data has been loaded
     */
    isDataLoaded() {
        return this._isDataLoaded;
    }

    /**
     * Get initialization status
     */
    isInitialized() {
        return this._isInitialized;
    }

    loadDemoEndpoints() {
        const demoEndpoints = [
            {
                id: 'endpoint-1',
                name: 'Ground Station',
                protocol: 'UDP',
                port: 14550,
                hostIp: '192.168.1.100',
                description: 'Primary ground station connection',
                status: 'online',
                enabled: true,
                autostart: true,
                messages: 45,
                bandwidth: 12.5,
                lastSeen: new Date()
            },
            {
                id: 'endpoint-2',
                name: 'Companion Computer',
                protocol: 'TCP',
                port: 5760,
                hostIp: '192.168.1.101',
                description: 'Onboard companion computer',
                status: 'online',
                enabled: true,
                autostart: false,
                messages: 128,
                bandwidth: 8.3,
                lastSeen: new Date()
            },
            {
                id: 'endpoint-3',
                name: 'GCS Laptop',
                protocol: 'UDP',
                port: 14550,
                hostIp: '192.168.1.102',
                description: 'Ground control station laptop',
                status: 'warning',
                enabled: true,
                autostart: false,
                messages: 23,
                bandwidth: 2.1,
                lastSeen: new Date(Date.now() - 60000)
            },
            {
                id: 'endpoint-4',
                name: 'Mission Planner',
                protocol: 'TCP',
                port: 5760,
                hostIp: '192.168.1.103',
                description: 'Mission planning station',
                status: 'offline',
                enabled: false,
                autostart: false,
                messages: 0,
                bandwidth: 0,
                lastSeen: new Date(Date.now() - 300000)
            }
        ];

        demoEndpoints.forEach(endpoint => {
            this.endpoints.set(endpoint.id, endpoint);
        });

        this.updateStatistics();
    }

    startRealTimeUpdates() {
        this.updateInterval = setInterval(() => {
            this.simulateDataUpdate();
            this.updateStatistics();
            this.emit('dataUpdated');
        }, 2000);
    }

    simulateDataUpdate() {
        this.endpoints.forEach(endpoint => {
            if (endpoint.status === 'online' && endpoint.enabled) {
                // Simulate message count changes
                endpoint.messages += Math.floor(Math.random() * 10);
                endpoint.bandwidth += (Math.random() * 2 - 1);
                endpoint.bandwidth = Math.max(0, endpoint.bandwidth);
                endpoint.lastSeen = new Date();
            }
        });
    }

    updateStatistics() {
        this.statistics.online = 0;
        this.statistics.warning = 0;
        this.statistics.offline = 0;

        this.endpoints.forEach(endpoint => {
            switch (endpoint.status) {
                case 'online':
                    this.statistics.online++;
                    break;
                case 'warning':
                    this.statistics.warning++;
                    break;
                case 'offline':
                    this.statistics.offline++;
                    break;
            }
        });

        // Generate random alerts
        if (Math.random() < 0.1) {
            this.generateRandomAlert();
        }
    }

    generateRandomAlert() {
        const alertTypes = [
            { type: 'warning', message: 'High latency detected on Ground Station' },
            { type: 'info', message: 'New endpoint connected' },
            { type: 'error', message: 'Connection lost on Mobile Station' }
        ];

        const randomAlert = alertTypes[Math.floor(Math.random() * alertTypes.length)];
        const alert = {
            id: `alert-${Date.now()}`,
            ...randomAlert,
            timestamp: new Date()
        };

        this.alerts.unshift(alert);
        
        // Keep only last 10 alerts
        if (this.alerts.length > 10) {
            this.alerts = this.alerts.slice(0, 10);
        }

        this.emit('alert', alert);
    }

    // Endpoint management methods
    getEndpoints() {
        return Array.from(this.endpoints.values());
    }

    getEndpoint(id) {
        return this.endpoints.get(id);
    }

    addEndpoint(endpointData) {
        const endpoint = {
            id: `endpoint-${Date.now()}`,
            status: 'offline',
            messages: 0,
            bandwidth: 0,
            lastSeen: null,
            ...endpointData
        };

        this.endpoints.set(endpoint.id, endpoint);
        this.updateStatistics();
        this.emit('endpointAdded', endpoint);
        
        return endpoint;
    }

    updateEndpoint(id, updates) {
        const endpoint = this.endpoints.get(id);
        if (endpoint) {
            Object.assign(endpoint, updates);
            this.updateStatistics();
            this.emit('endpointUpdated', endpoint);
            return endpoint;
        }
        return null;
    }

    deleteEndpoint(id) {
        const endpoint = this.endpoints.get(id);
        if (endpoint) {
            this.endpoints.delete(id);
            this.updateStatistics();
            this.emit('endpointDeleted', endpoint);
            return true;
        }
        return false;
    }

    startEndpoint(id) {
        const endpoint = this.endpoints.get(id);
        if (endpoint) {
            endpoint.status = 'online';
            endpoint.lastSeen = new Date();
            this.updateStatistics();
            this.emit('endpointStarted', endpoint);
            return true;
        }
        return false;
    }

    stopEndpoint(id) {
        const endpoint = this.endpoints.get(id);
        if (endpoint) {
            endpoint.status = 'offline';
            this.updateStatistics();
            this.emit('endpointStopped', endpoint);
            return true;
        }
        return false;
    }

    startAllEndpoints() {
        let started = 0;
        this.endpoints.forEach(endpoint => {
            if (endpoint.enabled) {
                this.startEndpoint(endpoint.id);
                started++;
            }
        });
        return started;
    }

    stopAllEndpoints() {
        let stopped = 0;
        this.endpoints.forEach(endpoint => {
            if (endpoint.status !== 'offline') {
                this.stopEndpoint(endpoint.id);
                stopped++;
            }
        });
        return stopped;
    }

    // Configuration methods
    exportConfiguration() {
        const config = {
            version: '1.0.0',
            timestamp: new Date().toISOString(),
            endpoints: this.getEndpoints(),
            settings: {
                autoStart: true,
                monitoringInterval: 2000
            }
        };
        
        return JSON.stringify(config, null, 2);
    }

    importConfiguration(config) {
        try {
            if (!config.endpoints || !Array.isArray(config.endpoints)) {
                throw new Error('Invalid configuration format');
            }
            
            let importedCount = 0;
            
            // Import endpoints from config
            config.endpoints.forEach(endpointData => {
                const endpoint = {
                    id: `endpoint-${Date.now()}-${importedCount}`,
                    status: 'offline',
                    messages: 0,
                    bandwidth: 0,
                    lastSeen: null,
                    ...endpointData
                };
                
                this.endpoints.set(endpoint.id, endpoint);
                importedCount++;
            });
            
            this.updateStatistics();
            this.emit('dataUpdated');
            
            return importedCount;
        } catch (error) {
            console.error('[MAVLINK-EXTENSION] Import configuration failed:', error);
            throw error;
        }
    }

    addDemoEndpoint() {
        const demoEndpoint = {
            name: `Demo Endpoint ${Date.now()}`,
            protocol: 'UDP',
            hostIp: '192.168.1.100',
            port: 14550,
            description: 'Demo MAVLink endpoint for testing',
            mode: 'normal',
            baudRate: '115200',
            autostart: false,
            enabled: true
        };
        
        return this.addEndpoint(demoEndpoint);
    }

    // Endpoint configuration methods
    getEndpointConfiguration(endpointId) {
        const endpoint = this.getEndpoint(endpointId);
        if (!endpoint) return null;

        return {
            name: endpoint.name,
            protocol: endpoint.protocol,
            hostIp: endpoint.hostIp || '192.168.1.100',
            port: endpoint.port,
            description: endpoint.description || '',
            mode: endpoint.mode || 'normal',
            baudRate: endpoint.baudRate || '115200',
            autostart: endpoint.autostart || false,
            enabled: endpoint.enabled !== false
        };
    }

    // System validation
    validateSystem() {
        const issues = [];
        const warnings = [];

        this.endpoints.forEach(endpoint => {
            // Check for port conflicts
            const portConflicts = Array.from(this.endpoints.values()).filter(
                e => e.id !== endpoint.id && e.port === endpoint.port && e.protocol === endpoint.protocol
            );
            
            if (portConflicts.length > 0) {
                issues.push(`Port conflict detected for ${endpoint.name} (${endpoint.protocol}:${endpoint.port})`);
            }

            // Check for offline endpoints that should be online
            if (endpoint.enabled && endpoint.autostart && endpoint.status === 'offline') {
                warnings.push(`Auto-start endpoint ${endpoint.name} is offline`);
            }

            // Check for high bandwidth usage
            if (endpoint.bandwidth > 50) {
                warnings.push(`High bandwidth usage on ${endpoint.name}: ${endpoint.bandwidth.toFixed(1)} KB/s`);
            }
        });

        const validationResult = {
            isValid: issues.length === 0,
            issues,
            warnings,
            timestamp: new Date()
        };

        this.emit('systemValidated', validationResult);
        return validationResult;
    }

    // Event handling
    on(event, callback) {
        if (!this.eventListeners.has(event)) {
            this.eventListeners.set(event, []);
        }
        this.eventListeners.get(event).push(callback);
    }

    off(event, callback) {
        if (this.eventListeners.has(event)) {
            const listeners = this.eventListeners.get(event);
            const index = listeners.indexOf(callback);
            if (index > -1) {
                listeners.splice(index, 1);
            }
        }
    }

    emit(event, data) {
        if (this.eventListeners.has(event)) {
            this.eventListeners.get(event).forEach(callback => {
                try {
                    callback(data);
                } catch (error) {
                    console.error(`[MAVLINK-EXTENSION] Error in event listener for ${event}:`, error);
                }
            });
        }
    }

    // Utility methods
    isInitialized() {
        return this._isInitialized;
    }

    getStatistics() {
        return { ...this.statistics };
    }

    getAlerts() {
        return [...this.alerts];
    }

    clearAlerts() {
        this.alerts = [];
        this.emit('alertsCleared');
    }

    destroy() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
        }
        
        this.eventListeners.clear();
        this.endpoints.clear();
        this.alerts = [];
        this._isInitialized = false;
        
        console.log('[MAVLINK-EXTENSION] MAVLink extension manager destroyed');
    }
}

/**
 * Source Page Main Controller
 * Integrates authentication, HTTP client, and WebSocket functionality
 */

import { JWTTokenManager } from '../jwt-token-manager.js';
import { HttpClient } from '../http-client/src/HttpClient.js';
import { HttpConfig } from '../http-client/lib/config.js';

// Import auth module components
import { JwtAuthManager } from '../auth-module/lib/jwt-auth-manager.js';
import { JwtUtils } from '../auth-module/lib/jwt-utils.js';

// Import WebSocket client
import { WebSocketClient } from '../websocket-client-shared/src/WebSocketClient.js';

// Import popup manager
import '../popup-manager.js';

class SourceManager {
    constructor() {
        this.httpClient = null;
        this.tokenManager = null;
        this.authManager = null;
        this.wsClient = null;
        this.currentUser = null;
        this.initialized = false;
        this.currentTab = 'system-dashboard'; // Track current active tab
        
        this.init();
    }
    
    async init() {
        try {
            console.log('[SOURCE-MANAGER] Initializing source page...');
            
            // Initialize JWT Token Manager first (needed for authentication check)
            this.initializeTokenManager();
            
            // Check if user is authenticated
            if (!this.checkAuthentication()) {
                console.log('[SOURCE-MANAGER] User not authenticated, redirecting to login');
                window.location.replace('/login-page.html');
                return;
            }
            
            // Initialize HTTP client with shared instance
            this.initializeHttpClient();
            
            // Initialize Auth Manager
            this.initializeAuthManager();
            
            // Initialize WebSocket client
            this.initializeWebSocketClient();
            
            // Load user information
            await this.loadUserInfo();
            
            this.initialized = true;
            
            // Setup tab switching listeners
            this.setupTabSwitchingListeners();
            
            // Emit initialization complete event
            document.dispatchEvent(new CustomEvent('sourceInitialized', {
                detail: { sourceManager: this }
            }));
            
            console.log('[SOURCE-MANAGER] Source page initialized successfully');
            
        } catch (error) {
            console.error('[SOURCE-MANAGER] Initialization failed:', error);
            this.handleInitializationError(error);
        }
    }
    
    checkAuthentication() {
        const session = localStorage.getItem('session');
        const sessionValidated = localStorage.getItem('session_validated');
        
        // Use JWT Token Manager to get access token
        const accessToken = this.tokenManager ? this.tokenManager.getAccessToken() : null;
        
        return session === 'authenticated' && 
               sessionValidated === 'true' && 
               accessToken;
    }
    
    initializeHttpClient() {
        try {
            // Get shared HTTP client or create new one
            this.httpClient = HttpClient.getSharedClient({
                baseURL: '',
                timeout: 15000,
                headers: {
                    'Content-Type': 'application/json'
                },
                retryConfig: {
                    maxRetries: 3,
                    retryDelay: 1000
                }
            });
            
            console.log('[SOURCE-MANAGER] HTTP client initialized');
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to initialize HTTP client:', error);
            throw error;
        }
    }
    
    initializeTokenManager() {
        try {
            this.tokenManager = new JWTTokenManager({
                apiBaseUrl: '/api/auth'
            });
            
            console.log('[SOURCE-MANAGER] JWT Token Manager initialized');
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to initialize Token Manager:', error);
            throw error;
        }
    }
    
    initializeAuthManager() {
        try {
            this.authManager = new JwtAuthManager({
                apiBaseUrl: '/api/auth',
                httpClient: this.httpClient,
                tokenManager: this.tokenManager
            });
            
            console.log('[SOURCE-MANAGER] Auth Manager initialized');
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to initialize Auth Manager:', error);
            throw error;
        }
    }
    
    initializeWebSocketClient() {
        try {
            // Get shared WebSocket client or create new one
            this.wsClient = WebSocketClient.getSharedClient({
                url: `ws://${window.location.hostname}:9002`,
                protocols: [],
                debug: true,
                autoConnect: true,
                reconnectConfig: {
                    enabled: true,
                    maxAttempts: 5,
                    interval: 3000,
                    backoffMultiplier: 1.5,
                    maxInterval: 15000
                },
                heartbeatConfig: {
                    enabled: false, // Disable heartbeat initially
                    interval: 30000,
                    timeout: 5000,
                    message: { type: 'heartbeat', client: 'source-manager' }
                },
                queueMessages: false, // Disable queuing initially
                maxQueueSize: 50,
                timeout: 8000
            });
            
            // Set up WebSocket event handlers
            this.setupWebSocketHandlers();
            
            console.log('[SOURCE-MANAGER] WebSocket client initialized with shared architecture');
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to initialize WebSocket client:', error);
            // Don't throw error - WebSocket is optional for basic functionality
        }
    }
    
    setupWebSocketHandlers() {
        if (!this.wsClient) return;
        
        this.wsClient.on('open', () => {
            console.log('[SOURCE-MANAGER] ✅ WebSocket connected successfully');
            
            // Dispatch event for UI update
            document.dispatchEvent(new CustomEvent('websocketOpen', {
                detail: { status: 'connected' }
            }));
            
            // Authenticate WebSocket connection
            this.authenticateWebSocket();
        });
        
        this.wsClient.on('message', (data) => {
            this.handleWebSocketMessage(data);
        });
        
        this.wsClient.on('close', (event) => {
            console.log(`[SOURCE-MANAGER] ❌ WebSocket disconnected: ${event.code} - ${event.reason}`);
            
            // Dispatch event for UI update
            document.dispatchEvent(new CustomEvent('websocketClose', {
                detail: { status: 'disconnected', code: event.code, reason: event.reason }
            }));
            
            // Provide specific error information
            if (event.code === 1006) {
                console.log('[SOURCE-MANAGER] 💡 Error 1006: Connection was closed abnormally');
                console.log('[SOURCE-MANAGER] 💡 This usually indicates:');
                console.log('[SOURCE-MANAGER]    - Server is not running or not accepting WebSocket connections');
                console.log('[SOURCE-MANAGER]    - Network connectivity issues');
                console.log('[SOURCE-MANAGER]    - Firewall blocking the connection');
            } else if (event.code === 1000) {
                console.log('[SOURCE-MANAGER] 💡 Normal closure');
            } else {
                console.log(`[SOURCE-MANAGER] 💡 WebSocket close code ${event.code}: https://www.rfc-editor.org/rfc/rfc6455.html#section-7.4.1`);
            }
        });
        
        this.wsClient.on('error', (error) => {
            console.error('[SOURCE-MANAGER] ❌ WebSocket error:', error);
            console.log('[SOURCE-MANAGER] 💡 Check if WebSocket server is running on port 9002');
            
            // Dispatch event for UI update
            document.dispatchEvent(new CustomEvent('websocketError', {
                detail: { status: 'error', error: error.message }
            }));
        });
        
        this.wsClient.on('reconnecting', (info) => {
            console.log(`[SOURCE-MANAGER] 🔄 WebSocket reconnecting: attempt ${info.attempt}/${this.wsClient.config.reconnectConfig.maxAttempts} (delay: ${info.delay}ms)`);
            
            // Dispatch event for UI update
            document.dispatchEvent(new CustomEvent('websocketReconnecting', {
                detail: { status: 'connecting', attempt: info.attempt, maxAttempts: info.maxAttempts, delay: info.delay }
            }));
        });
        
        this.wsClient.on('maxReconnectAttemptsReached', () => {
            console.error('[SOURCE-MANAGER] ❌ WebSocket max reconnection attempts reached - giving up');
            console.log('[SOURCE-MANAGER] 💡 Please check the WebSocket server status');
            
            // Dispatch event for UI update
            document.dispatchEvent(new CustomEvent('websocketMaxReconnectReached', {
                detail: { status: 'error', message: 'Max reconnection attempts reached' }
            }));
        });
        
        this.wsClient.on('heartbeat', (data) => {
            console.log('[SOURCE-MANAGER] 💓 WebSocket heartbeat response:', data);
        });
    }
    
    async authenticateWebSocket() {
        try {
            const token = this.tokenManager.getAccessToken();
            if (token && this.wsClient.isClientConnected()) {
                await this.wsClient.send({
                    type: 'auth',
                    token: token
                });
                console.log('[SOURCE-MANAGER] WebSocket authenticated');
            }
        } catch (error) {
            console.error('[SOURCE-MANAGER] WebSocket authentication failed:', error);
        }
    }
    
    handleWebSocketMessage(data) {
        console.log('[SOURCE-MANAGER] ===========================================');
        console.log('[SOURCE-MANAGER] Received WebSocket message:', JSON.stringify(data, null, 2));
        console.log('[SOURCE-MANAGER] Message type:', data.type);
        console.log('[SOURCE-MANAGER] Message category:', data.category);
        console.log('[SOURCE-MANAGER] Message data:', JSON.stringify(data.data, null, 2));
        console.log('[SOURCE-MANAGER] ===========================================');
        
        // Handle network priority specific messages
        if (this.isNetworkPriorityData(data)) {
            console.log('[SOURCE-MANAGER] Network priority data detected, updating UI...');
            this.updateNetworkPriorityFromWebSocket(data);
        }
        // Check if this is dashboard-related data and update dashboard components
        else if (this.isDashboardData(data)) {
            console.log('[SOURCE-MANAGER] Dashboard data detected, updating components...');
            this.updateDashboardWithData(data);
        } else {
            console.log('[SOURCE-MANAGER] Not dashboard or network priority data, skipping updates');
        }
        
        // Emit custom event for UI components to handle
        document.dispatchEvent(new CustomEvent('websocketMessage', {
            detail: data
        }));
    }
    
    /**
     * Check if WebSocket message contains network priority data
     */
    isNetworkPriorityData(data) {
        // Check for network priority update messages
        if (data.type === 'network_priority_update') {
            return true;
        }
        
        // Check for dashboard updates with network priority category
        if (data.type === 'dashboard_update' && data.category === 'network_priority') {
            return true;
        }
        
        // Check if data contains network priority fields
        const networkPriorityFields = ['interfaces', 'routingRules'];
        return networkPriorityFields.some(field => 
            (data.data && data.data.hasOwnProperty(field)) || data.hasOwnProperty(field)
        );
    }
    
    /**
     * Update network priority components from WebSocket data
     */
    updateNetworkPriorityFromWebSocket(data) {
        console.log('[SOURCE-MANAGER] Processing network priority WebSocket update...');
        
        try {
            // Extract network priority data from various message formats
            let networkPriorityData;
            
            if (data.category === 'network_priority' && data.data) {
                // Category format
                networkPriorityData = this.transformBackendNetworkPriorityData(data.data);
            } else if (data.data) {
                // Direct data format
                networkPriorityData = {
                    interfaces: data.data.interfaces || [],
                    routingRules: data.data.routingRules || []
                };
            } else {
                // Flat format
                networkPriorityData = {
                    interfaces: data.interfaces || [],
                    routingRules: data.routingRules || []
                };
            }
            
            console.log('[SOURCE-MANAGER] Extracted network priority data:', networkPriorityData);
            
            // Update the UI through the central update method
            this.updateNetworkPriorityUI(networkPriorityData);
            
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to process network priority WebSocket update:', error);
        }
    }
    
    /**
     * Check if WebSocket message contains dashboard data
     */
    isDashboardData(data) {
        // Check for dashboard update messages from backend
        if (data.type === 'dashboard_update') {
            return true;
        }
        
        // Check for various dashboard data types
        const dashboardTypes = [
            'system_stats',
            'network_status', 
            'cellular_status',
            'system_update',
            'performance_data',
            'network_priority_update'
        ];
        
        // Check message type
        if (data.type && dashboardTypes.includes(data.type)) {
            return true;
        }
        
        // Check if data contains dashboard-related fields (flat structure from backend)
        const dashboardFields = [
            'usage_percent', 'cores', 'temperature_celsius', 'frequency_ghz',  // CPU
            'used_gb', 'total_gb', 'usage_percent',                           // RAM  
            'used_mb', 'total_gb', 'status',                                  // Swap
            'internet', 'connection',                                         // Network
            'ultima_server', 'signal',                                        // Server & Cellular
            'interfaces', 'routing_rules'                                    // Network Priority
        ];
        
        return dashboardFields.some(field => data.data && data.data.hasOwnProperty(field));
    }
    
    /**
     * Update dashboard components with received data
     */
    updateDashboardWithData(data) {
        console.log('[SOURCE-MANAGER] ===========================================');
        console.log('[SOURCE-MANAGER] Updating dashboard with WebSocket data:', JSON.stringify(data, null, 2));
        
        try {
            // Create standardized data structure for both components
            const standardizedData = this.createStandardizedDashboardData(data);
            console.log('[SOURCE-MANAGER] Standardized data created:', JSON.stringify(standardizedData, null, 2));
            
            if (window.dashboard) {
                console.log('[SOURCE-MANAGER] Updating dashboard controller...');
                this.updateDashboardController(standardizedData);
            } else {
                console.warn('[SOURCE-MANAGER] Dashboard controller not found');
            }
            
            if (window.dashboardUI) {
                console.log('[SOURCE-MANAGER] Updating dashboard UI...');
                this.updateDashboardUI(standardizedData);
            } else {
                console.warn('[SOURCE-MANAGER] Dashboard UI not found');
            }
            
            // Update network priority UI if network priority data is present
            // Note: Network priority updates are now handled separately in handleWebSocketMessage
            // to avoid duplication and ensure source manager is the central authority
            if (false && standardizedData.networkPriority && (standardizedData.networkPriority.interfaces.length > 0 || standardizedData.networkPriority.routingRules.length > 0)) {
                console.log('[SOURCE-MANAGER] Updating network priority UI via dashboard flow...');
                this.updateNetworkPriorityUI(standardizedData.networkPriority);
            }
            
            document.dispatchEvent(new CustomEvent('dashboardDataUpdate', {
                detail: { data: standardizedData, source: 'websocket' }
            }));
            
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to update dashboard with WebSocket data:', error);
        }
        
        console.log('[SOURCE-MANAGER] ===========================================');
    }
    
    /**
     * Create standardized dashboard data structure from WebSocket data
     */
    createStandardizedDashboardData(data) {
        console.log('[SOURCE-MANAGER] Creating standardized data from:', data);
        
        const standardized = {
            system: {
                cpu: {},
                ram: {},
                swap: {}
            },
            network: {
                internet: {},
                server: {},
                connection: {}
            },
            cellular: {
                signal: {},
                connection: {}
            },
            networkPriority: {
                interfaces: [],
                routingRules: []
            },
            lastUpdate: data.timestamp ? new Date(data.timestamp * 1000).toLocaleString() : new Date().toLocaleString()
        };
        
        // Handle backend dashboard_update messages with category
        if (data.type === 'dashboard_update' && data.category && data.data) {
            const category = data.category;
            const categoryData = data.data;
            
            console.log('[SOURCE-MANAGER] Processing backend category update:');
            console.log('[SOURCE-MANAGER] - Category:', category);
            console.log('[SOURCE-MANAGER] - Category Data:', JSON.stringify(categoryData, null, 2));
            
            switch (category) {
                case 'system':
                    console.log('[SOURCE-MANAGER] -> Handling SYSTEM category (contains CPU data only)');
                    // System category from backend contains ONLY CPU data, not nested system data
                    standardized.system.cpu = this.transformBackendCpuData(categoryData);
                    break;
                case 'cpu':
                    console.log('[SOURCE-MANAGER] -> Handling CPU category');
                    standardized.system.cpu = this.transformBackendCpuData(categoryData);
                    break;
                case 'ram':
                    console.log('[SOURCE-MANAGER] -> Handling RAM category');
                    standardized.system.ram = this.transformBackendRamData(categoryData);
                    break;
                case 'swap':
                    console.log('[SOURCE-MANAGER] -> Handling SWAP category');
                    standardized.system.swap = this.transformBackendSwapData(categoryData);
                    break;
                case 'network':
                    console.log('[SOURCE-MANAGER] -> Handling NETWORK category');
                    standardized.network = this.transformBackendNetworkData(categoryData);
                    break;
                case 'ultima_server':
                    console.log('[SOURCE-MANAGER] -> Handling ULTIMA_SERVER category');
                    standardized.network.server = this.transformBackendServerData(categoryData);
                    break;
                case 'signal':
                    console.log('[SOURCE-MANAGER] -> Handling SIGNAL category');
                    standardized.cellular = this.transformBackendCellularData(categoryData);
                    break;
                case 'network_priority':
                    console.log('[SOURCE-MANAGER] -> Handling NETWORK_PRIORITY category');
                    standardized.networkPriority = this.transformBackendNetworkPriorityData(categoryData);
                    break;
                default:
                    console.warn('[SOURCE-MANAGER] Unknown dashboard category:', category);
            }
        }
        // Handle nested system data (legacy format)
        else if (data.system) {
            if (data.system.cpu) standardized.system.cpu = { ...standardized.system.cpu, ...data.system.cpu };
            if (data.system.ram) standardized.system.ram = { ...standardized.system.ram, ...data.system.ram };
            if (data.system.swap) standardized.system.swap = { ...standardized.system.swap, ...data.system.swap };
        }
        // Handle nested network data (legacy format)
        else if (data.network) {
            if (data.network.internet) standardized.network.internet = { ...standardized.network.internet, ...data.network.internet };
            if (data.network.server) standardized.network.server = { ...standardized.network.server, ...data.network.server };
            if (data.network.connection) standardized.network.connection = { ...standardized.network.connection, ...data.network.connection };
        }
        // Handle nested cellular data (legacy format)
        else if (data.cellular) {
            if (data.cellular.signal) standardized.cellular.signal = { ...standardized.cellular.signal, ...data.cellular.signal };
            if (data.cellular.connection) standardized.cellular.connection = { ...standardized.cellular.connection, ...data.cellular.connection };
        }
        // Handle nested network priority data (legacy format)
        else if (data.networkPriority) {
            if (data.networkPriority.interfaces) standardized.networkPriority.interfaces = data.networkPriority.interfaces;
            if (data.networkPriority.routingRules) standardized.networkPriority.routingRules = data.networkPriority.routingRules;
        }
        // Handle direct network priority fields (legacy format)
        else if (data.interfaces || data.routingRules) {
            if (data.interfaces) standardized.networkPriority.interfaces = data.interfaces;
            if (data.routingRules) standardized.networkPriority.routingRules = data.routingRules;
        }
        // Handle flat data structure (direct fields)
        else if (data.data) {
            const flatData = data.data;
            if (flatData.usage_percent !== undefined || flatData.cores !== undefined) {
                standardized.system.cpu = this.transformBackendCpuData(flatData);
            }
            if (flatData.used_gb !== undefined || flatData.total_gb !== undefined) {
                standardized.system.ram = this.transformBackendRamData(flatData);
            }
            if (flatData.used_mb !== undefined || flatData.total_gb !== undefined) {
                standardized.system.swap = this.transformBackendSwapData(flatData);
            }
        }
        
        // Handle additional metadata
        if (data.lastUpdate) standardized.lastUpdate = data.lastUpdate;
        if (data.lastLogin) standardized.lastLogin = data.lastLogin;
        
        console.log('[SOURCE-MANAGER] Standardized data created:', standardized);
        return standardized;
    }
    
    /**
     * Transform backend CPU data to frontend format
     */
    transformBackendCpuData(cpuData) {
        console.log('[SOURCE-MANAGER] --- Transforming CPU Data ---');
        console.log('[SOURCE-MANAGER] Input CPU data:', JSON.stringify(cpuData, null, 2));
        
        const transformed = {
            usage: cpuData.usage_percent !== undefined ? `${Math.round(cpuData.usage_percent)}%` : '0%',
            cores: cpuData.cores !== undefined ? `${cpuData.cores} Cores` : '0 Cores',
            temperature: cpuData.temperature_celsius !== undefined ? `${Math.round(cpuData.temperature_celsius)}°C` : '0°C',
            baseClock: cpuData.frequency_ghz !== undefined ? `${cpuData.frequency_ghz.toFixed(1)} GHz` : '0 GHz',
            boostClock: cpuData.frequency_ghz !== undefined ? `${(cpuData.frequency_ghz * 1.2).toFixed(1)} GHz` : '0 GHz'
        };
        
        console.log('[SOURCE-MANAGER] Transformed CPU data:', JSON.stringify(transformed, null, 2));
        console.log('[SOURCE-MANAGER] --- End CPU Transformation ---');
        
        return transformed;
    }
    
    /**
     * Transform backend RAM data to frontend format
     */
    transformBackendRamData(ramData) {
        const usedGB = ramData.used_gb || 0;
        const totalGB = ramData.total_gb || 0;
        const availableGB = totalGB - usedGB;
        const usagePercent = ramData.usage_percent || (totalGB > 0 ? (usedGB / totalGB * 100) : 0);
        
        return {
            usage: `${Math.round(usagePercent)}%`,
            used: `${usedGB.toFixed(1)} GB`,
            total: `${totalGB.toFixed(1)} GB`,
            available: `${availableGB.toFixed(1)} GB`,
            type: 'DDR4'
        };
    }
    
    /**
     * Transform backend Swap data to frontend format
     */
    transformBackendSwapData(swapData) {
        const usedMB = swapData.used_mb || 0;
        const totalGB = swapData.total_gb || 0;
        const totalMB = totalGB * 1024;
        const availableMB = totalMB - usedMB;
        const usagePercent = swapData.usage_percent || (totalMB > 0 ? (usedMB / totalMB * 100) : 0);
        
        return {
            usage: `${Math.round(usagePercent)}%`,
            used: `${Math.round(usedMB)} MB`,
            total: `${totalGB.toFixed(1)} GB`,
            available: `${Math.round(availableMB)} MB`,
            priority: swapData.status || 'Normal'
        };
    }
    
    /**
     * Transform backend Network data to frontend format
     */
    transformBackendNetworkData(networkData) {
        return {
            internet: {
                status: networkData.internet?.status || 'Unknown',
                externalIp: networkData.internet?.external_ip || 'N/A',
                dnsPrimary: networkData.internet?.dns_primary || 'N/A',
                dnsSecondary: networkData.internet?.dns_secondary || 'N/A',
                latency: networkData.internet?.latency_ms !== undefined ? `${networkData.internet.latency_ms.toFixed(1)} ms` : '0 ms',
                bandwidth: networkData.internet?.bandwidth || 'N/A'
            },
            connection: {
                type: 'Ethernet',
                interface: networkData.connection?.interface || 'N/A',
                macAddress: networkData.connection?.mac_address || 'N/A',
                localIp: networkData.connection?.local_ip || 'N/A',
                gateway: networkData.connection?.gateway || 'N/A',
                speed: networkData.connection?.speed || 'N/A'
            }
        };
    }
    
    /**
     * Transform backend Server data to frontend format
     */
    transformBackendServerData(serverData) {
        return {
            status: serverData.status || 'Unknown',
            hostname: serverData.server || 'N/A',
            port: serverData.port || 0,
            protocol: serverData.protocol || 'N/A',
            lastPing: serverData.last_ping_ms !== undefined ? `${serverData.last_ping_ms.toFixed(1)} ms` : '0 ms',
            sessionDuration: serverData.session || 'N/A'
        };
    }
    
    /**
     * Transform backend Cellular data to frontend format
     */
    transformBackendCellularData(signalData) {
        const rssi = signalData.strength?.rssi_dbm || 0;
        const bars = this.calculateSignalBars(rssi);
        
        return {
            signal: {
                status: signalData.strength?.status || 'No Signal',
                bars: bars,
                rssi: signalData.strength?.rssi_dbm !== undefined ? `${signalData.strength.rssi_dbm.toFixed(1)} dBm` : '0 dBm',
                rsrp: signalData.strength?.rsrp_dbm !== undefined ? `${signalData.strength.rsrp_dbm.toFixed(1)} dBm` : '0 dBm',
                rsrq: signalData.strength?.rsrq_db !== undefined ? `${signalData.strength.rsrq_db.toFixed(1)} dB` : '0 dB',
                sinr: signalData.strength?.sinr_db !== undefined ? `${signalData.strength.sinr_db.toFixed(1)} dB` : '0 dB',
                cellId: signalData.strength?.cell_id || 'N/A'
            },
            connection: {
                status: signalData.connection?.status || 'Disconnected',
                network: signalData.connection?.network || 'N/A',
                technology: signalData.connection?.technology || 'N/A',
                band: signalData.connection?.band || 'N/A',
                apn: signalData.connection?.apn || 'N/A',
                dataUsage: signalData.connection?.data_usage_mb !== undefined ? `${signalData.connection.data_usage_mb.toFixed(1)} MB` : '0 MB'
            }
        };
    }
    
    /**
     * Transform backend Network Priority data to frontend format
     */
    transformBackendNetworkPriorityData(networkPriorityData) {
        console.log('[SOURCE-MANAGER] Transforming network priority data:', networkPriorityData);
        
        // Transform network interfaces data
        const interfaces = (networkPriorityData.networkInterfaces || []).map(iface => ({
            id: iface.id,
            name: iface.name,
            type: iface.type,
            status: iface.status?.toLowerCase() || 'unknown',
            priority: iface.priority || 100,
            ip: iface.ipAddress || 'N/A',
            gateway: iface.gateway || 'N/A',
            speed: iface.speed ? `${iface.speed} Mbps` : 'N/A',
            metric: iface.metric || 100,
            isDefault: iface.isDefault || false,
            netmask: iface.netmask || 'N/A',
            lastSeen: 'Just now',
            dataRate: iface.speed ? `${iface.speed} Mbps` : 'N/A',
            latency: 'N/A',
            uptime: 'N/A'
        }));
        
        // Transform routing rules data
        const routingRules = (networkPriorityData.routingRules || []).map(rule => ({
            id: rule.id,
            priority: rule.priority || 100,
            destination: rule.destination || 'N/A',
            gateway: rule.gateway || 'N/A',
            interface: rule.interface || 'N/A',
            metric: rule.metric || 100,
            status: rule.status?.toLowerCase() || 'unknown',
            type: rule.type || 'dynamic',
            table: rule.table || 'main',
            isDefault: rule.destination === '0.0.0.0/0',
            matchCount: 0,
            lastMatched: 'Never',
            efficiency: rule.priority <= 10 ? 95 : 75
        }));
        
        return {
            interfaces: interfaces,
            routingRules: routingRules,
            statistics: networkPriorityData.statistics || {
                total: interfaces.length,
                online: interfaces.filter(i => i.status === 'online').length,
                offline: interfaces.filter(i => i.status === 'offline').length,
                activeRules: routingRules.length,
                lastUpdated: networkPriorityData.lastUpdated || new Date().toLocaleString()
            },
            lastUpdated: networkPriorityData.lastUpdated || new Date().toLocaleString()
        };
    }
    
    /**
     * Calculate signal bars based on RSSI
     */
    calculateSignalBars(rssi) {
        if (rssi >= -50) return 5;      // Excellent
        if (rssi >= -60) return 4;      // Good
        if (rssi >= -70) return 3;      // Fair
        if (rssi >= -80) return 2;      // Poor
        if (rssi >= -90) return 1;      // Very Poor
        return 0;                       // No Signal
    }
    
    /**
     * Setup tab switching listeners
     */
    setupTabSwitchingListeners() {
        console.log('[SOURCE-MANAGER] Setting up tab switching listeners...');
        
        // Listen for tab change events from UI
        document.addEventListener('tabChanged', (e) => {
            const { section, previousSection } = e.detail;
            this.handleTabChange(section, previousSection);
        });
        
        // Listen for navigation events
        document.addEventListener('navigationRequested', (e) => {
            const section = e.detail.section;
            this.requestNavigation(section);
        });
        
        console.log('[SOURCE-MANAGER] Tab switching listeners setup complete');
    }
    
    /**
     * Handle tab change events
     */
    handleTabChange(newSection, previousSection) {
        console.log(`[SOURCE-MANAGER] Tab changed from ${previousSection} to ${newSection}`);
        
        // Update current tab tracking
        this.currentTab = newSection;
        
        // Perform tab-specific actions
        this.performTabSpecificActions(newSection);
        
        // Update WebSocket subscriptions if needed
        this.updateWebSocketSubscriptions(newSection);
        
        // Emit tab change confirmation
        document.dispatchEvent(new CustomEvent('tabChangeConfirmed', {
            detail: {
                section: newSection,
                previousSection: previousSection,
                timestamp: Date.now()
            }
        }));
    }
    
    /**
     * Perform actions specific to certain tabs
     */
    performTabSpecificActions(section) {
        console.log(`[SOURCE-MANAGER] Performing tab-specific actions for: ${section}`);
        
        switch (section) {
            case 'system-dashboard':
                this.handleSystemDashboardTab();
                break;
            case 'network-priority':
                this.handleNetworkPriorityTab();
                break;
            case 'network-benchmark':
                this.handleNetworkBenchmarkTab();
                break;
            case 'mavlink-extension':
                this.handleMavlinkExtensionTab();
                break;
            case 'firmware-upgrade':
                this.handleFirmwareUpgradeTab();
                break;
            case 'license-management':
                this.handleLicenseManagementTab();
                break;
            case 'backup-restore':
                this.handleBackupRestoreTab();
                break;
            default:
                console.log(`[SOURCE-MANAGER] No specific actions for tab: ${section}`);
        }
    }
    
    /**
     * Handle system dashboard tab activation
     */
    handleSystemDashboardTab() {
        console.log('[SOURCE-MANAGER] System dashboard tab activated');
        
        // Ensure WebSocket data processing is active for dashboard
        if (this.wsClient && this.wsClient.isClientConnected()) {
            console.log('[SOURCE-MANAGER] WebSocket is connected, dashboard data will be processed');
        } else {
            console.log('[SOURCE-MANAGER] WebSocket is not connected, dashboard may show stale data');
        }
    }
    
    /**
     * Handle network priority tab activation
     */
    handleNetworkPriorityTab() {
        console.log('[SOURCE-MANAGER] Network priority tab activated');
        
        // Network priority data now comes exclusively from WebSocket
        // No HTTP request needed - data will be pushed via WebSocket
        if (this.wsClient && this.wsClient.isClientConnected()) {
            console.log('[SOURCE-MANAGER] WebSocket connected - waiting for network priority data...');
            
            // Show loading state while waiting for WebSocket data
            if (window.dataLoadingUI) {
                window.dataLoadingUI.showInterfacesLoading();
                window.dataLoadingUI.showRoutingRulesLoading();
            }
            
            // Request data via WebSocket instead of HTTP
            this.wsClient.send({
                type: 'request_network_priority_data',
                timestamp: Date.now()
            });
        } else {
            console.log('[SOURCE-MANAGER] WebSocket not connected, cannot request network priority data');
            
            // Show error state
            if (window.dataLoadingUI) {
                window.dataLoadingUI.showError('interfaces', 'WebSocket not connected');
                window.dataLoadingUI.showError('routingRules', 'WebSocket not connected');
            }
        }
    }
    
    /**
     * Handle network benchmark tab activation
     */
    handleNetworkBenchmarkTab() {
        console.log('[SOURCE-MANAGER] Network benchmark tab activated');
        // Implementation for network benchmark tab actions
    }
    
    /**
     * Handle MAVLink extension tab activation
     */
    handleMavlinkExtensionTab() {
        console.log('[SOURCE-MANAGER] MAVLink extension tab activated');
        
        // Ensure WebSocket is available for MAVLink extension
        if (this.wsClient && this.wsClient.isClientConnected()) {
            console.log('[SOURCE-MANAGER] WebSocket is connected, MAVLink extension can communicate with backend');
        } else {
            console.log('[SOURCE-MANAGER] WebSocket is not connected, MAVLink extension will run in demo mode');
        }
    }
    
    /**
     * Handle firmware upgrade tab activation
     */
    handleFirmwareUpgradeTab() {
        console.log('[SOURCE-MANAGER] Firmware upgrade tab activated');
        
        // Initialize firmware upgrade data refresh if needed
        this.initializeFirmwareUpgradeData();
    }
    
    /**
     * Initialize firmware upgrade data
     */
    initializeFirmwareUpgradeData() {
        console.log('[SOURCE-MANAGER] Initializing firmware upgrade data...');
        
        // Trigger initial data load if firmware upgrade manager is available
        if (window.firmwareUpgradeManager) {
            // The firmware upgrade manager will handle its own data loading
            console.log('[SOURCE-MANAGER] Firmware upgrade manager available, data will be loaded automatically');
        } else {
            console.log('[SOURCE-MANAGER] Firmware upgrade manager not yet available, will be initialized by UI');
        }
    }
    
    /**
     * Handle license management tab activation
     */
    handleLicenseManagementTab() {
        console.log('[SOURCE-MANAGER] License management tab activated');
        // License management specific actions can be added here
        // For example: refresh licence data, validate licence status, etc.
    }
    
    /**
     * Handle backup & restore tab activation
     */
    handleBackupRestoreTab() {
        console.log('[SOURCE-MANAGER] Backup & restore tab activated');
        
        // Initialize backup & restore data refresh if needed
        this.initializeBackupRestoreData();
    }
    
    /**
     * Initialize backup & restore data
     */
    initializeBackupRestoreData() {
        console.log('[SOURCE-MANAGER] Initializing backup & restore data...');
        
        // Backup & restore specific data initialization can be added here
        // For example: load backup history, check system state, etc.
    }
    
    /**
     * Update WebSocket subscriptions based on active tab
     */
    updateWebSocketSubscriptions(section) {
        console.log(`[SOURCE-MANAGER] Updating WebSocket subscriptions for tab: ${section}`);
        
        // Send subscription message to backend if needed
        if (this.wsClient && this.wsClient.isClientConnected()) {
            const subscriptionData = {
                type: 'subscribe_updates',
                section: section,
                timestamp: Date.now()
            };
            
            this.wsClient.send(subscriptionData);
            console.log('[SOURCE-MANAGER] Sent subscription update for section:', section);
        }
    }
    
    /**
     * Request navigation to a specific section
     */
    requestNavigation(section) {
        console.log('[SOURCE-MANAGER] Navigation requested for section:', section);
        
        // Validate section
        if (!this.isValidSection(section)) {
            console.warn('[SOURCE-MANAGER] Invalid section requested:', section);
            return;
        }
        
        // Emit navigation request to UI
        document.dispatchEvent(new CustomEvent('navigationRequest', {
            detail: { section: section }
        }));
    }
    
    /**
     * Check if section is valid
     */
    isValidSection(section) {
        const validSections = [
            'system-dashboard',
            'wired-config', 'wireless-config', 'cellular-config', 'vpn-config',
            'advanced-network', 'network-benchmark', 'network-priority',
            'mavlink-overview', 'attached-ip-devices', 'mavlink-extension',
            'firmware-upgrade', 'license-management', 'backup-restore', 'about-us'
        ];
        return validSections.includes(section);
    }
    
    /**
     * Get current active tab
     */
    getCurrentTab() {
        return this.currentTab;
    }
    
    /**
     * Check if specific tab is active
     */
    isTabActive(section) {
        return this.currentTab === section;
    }
    
    /**
     * Update dashboard controller (dashboard.js)
     */
    updateDashboardController(data) {
        if (!window.dashboard) return;
        
        console.log('[SOURCE-MANAGER] Updating dashboard controller with:', data);
        
        // Update dashboard data structure
        if (window.dashboard.data) {
            // Merge with existing data, preserving defaults for missing fields
            this.mergeDashboardData(window.dashboard.data, data);
        } else {
            // Set new data
            window.dashboard.data = data;
        }
        
        // Trigger UI update through the dashboard controller
        if (window.dashboard.dashboardUI && window.dashboard.dashboardUI.updateData) {
            window.dashboard.dashboardUI.updateData(window.dashboard.data);
        } else {
            console.warn('[SOURCE-MANAGER] Dashboard UI not available for update');
        }
    }
    
    /**
     * Merge dashboard data safely, preserving existing structure
     */
    mergeDashboardData(existingData, newData) {
        // Merge system data
        if (newData.system) {
            // Ensure system object exists in existingData
            if (!existingData.system) {
                existingData.system = { cpu: {}, ram: {}, swap: {} };
            }
            if (newData.system.cpu) Object.assign(existingData.system.cpu, newData.system.cpu);
            if (newData.system.ram) Object.assign(existingData.system.ram, newData.system.ram);
            if (newData.system.swap) Object.assign(existingData.system.swap, newData.system.swap);
        }
        
        // Merge network data
        if (newData.network) {
            // Ensure network object exists in existingData
            if (!existingData.network) {
                existingData.network = { internet: {}, server: {}, connection: {} };
            }
            if (newData.network.internet) Object.assign(existingData.network.internet, newData.network.internet);
            if (newData.network.server) Object.assign(existingData.network.server, newData.network.server);
            if (newData.network.connection) Object.assign(existingData.network.connection, newData.network.connection);
        }
        
        // Merge cellular data
        if (newData.cellular) {
            // Ensure cellular object exists in existingData
            if (!existingData.cellular) {
                existingData.cellular = { signal: {}, connection: {} };
            }
            if (newData.cellular.signal) Object.assign(existingData.cellular.signal, newData.cellular.signal);
            if (newData.cellular.connection) Object.assign(existingData.cellular.connection, newData.cellular.connection);
        }
        
        // Merge network priority data
        if (newData.networkPriority) {
            // Ensure networkPriority object exists in existingData
            if (!existingData.networkPriority) {
                existingData.networkPriority = {
                    interfaces: [],
                    routingRules: []
                };
            }
            if (newData.networkPriority.interfaces) existingData.networkPriority.interfaces = newData.networkPriority.interfaces;
            if (newData.networkPriority.routingRules) existingData.networkPriority.routingRules = newData.networkPriority.routingRules;
        }
        
        // Update metadata
        if (newData.lastUpdate) existingData.lastUpdate = newData.lastUpdate;
        if (newData.lastLogin) existingData.lastLogin = newData.lastLogin;
    }
    
    /**
     * Update dashboard UI directly (dashboard-ui.js)
     */
    updateDashboardUI(data) {
        if (!window.dashboardUI) return;
        
        console.log('[SOURCE-MANAGER] Updating dashboard UI directly with:', data);
        
        // Update UI directly with standardized data - add safety check
        if (window.dashboardUI.updateData) {
            window.dashboardUI.updateData(data);
        } else {
            console.warn('[SOURCE-MANAGER] Dashboard UI updateData method not available');
        }
    }
    
    /**
     * Update network priority UI components
     */
    updateNetworkPriorityUI(networkPriorityData) {
        console.log('[SOURCE-MANAGER] Updating network priority UI with data:', networkPriorityData);
        
        // Update network priority manager if available
        if (window.networkPriorityManager) {
            console.log('[SOURCE-MANAGER] Updating network priority manager...');
            
            // Update interfaces data
            if (networkPriorityData.interfaces && networkPriorityData.interfaces.length > 0) {
                window.networkPriorityManager.interfaces = networkPriorityData.interfaces;
                window.networkPriorityManager.updateInterfacesDisplay();
                console.log('[SOURCE-MANAGER] Updated', networkPriorityData.interfaces.length, 'interfaces');
            }
            
            // Update routing rules data
            if (networkPriorityData.routingRules && networkPriorityData.routingRules.length > 0) {
                window.networkPriorityManager.routingRules = networkPriorityData.routingRules;
                window.networkPriorityManager.updateRoutingRulesDisplay();
                console.log('[SOURCE-MANAGER] Updated', networkPriorityData.routingRules.length, 'routing rules');
            }
            
            // Show priority connection status blinking indicator instead of notification
            if (window.networkPriorityManager.showPriorityConnectionStatus) {
                window.networkPriorityManager.showPriorityConnectionStatus();
            }
            
        } else {
            console.warn('[SOURCE-MANAGER] Network priority manager not found - cannot update UI');
        }
        
        // Also emit custom event for network priority components
        document.dispatchEvent(new CustomEvent('networkPriorityDataUpdate', {
            detail: { data: networkPriorityData, source: 'websocket' }
        }));
    }
    
    /**
     * Transform WebSocket data to dashboard controller format
     */
    transformWebSocketDataForDashboard(data) {
        const transformed = {};
        
        // Handle system stats
        if (data.system || data.cpu || data.ram || data.swap) {
            transformed.system = {
                cpu: data.cpu || data.system?.cpu || {},
                ram: data.ram || data.system?.ram || {},
                swap: data.swap || data.system?.swap || {}
            };
        }
        
        // Handle network status
        if (data.network || data.internet || data.server || data.connection) {
            transformed.network = {
                internet: data.internet || data.network?.internet || {},
                server: data.server || data.network?.server || {},
                connection: data.connection || data.network?.connection || {}
            };
        }
        
        // Handle cellular status
        if (data.cellular || data.signal) {
            transformed.cellular = {
                signal: data.signal || data.cellular?.signal || {},
                connection: data.cellular?.connection || {}
            };
        }
        
        // Add metadata
        if (data.timestamp) {
            transformed.lastUpdate = new Date(data.timestamp).toLocaleString();
        }
        
        return transformed;
    }
    
    /**
     * Transform WebSocket data to UI format
     */
    transformWebSocketDataForUI(data) {
        const uiData = {
            system: {},
            network: {},
            cellular: {}
        };
        
        // System data
        if (data.system) {
            uiData.system = data.system;
        } else {
            // Individual system components
            if (data.cpu) uiData.system.cpu = data.cpu;
            if (data.ram) uiData.system.ram = data.ram;
            if (data.swap) uiData.system.swap = data.swap;
        }
        
        // Network data
        if (data.network) {
            uiData.network = data.network;
        } else {
            // Individual network components
            if (data.internet) uiData.network.internet = data.internet;
            if (data.server) uiData.network.server = data.server;
            if (data.connection) uiData.network.connection = data.connection;
        }
        
        // Cellular data
        if (data.cellular) {
            uiData.cellular = data.cellular;
        } else {
            // Individual cellular components
            if (data.signal) uiData.cellular.signal = data.signal;
        }
        
        // Add timing info
        if (data.timestamp) {
            uiData.lastUpdate = new Date(data.timestamp).toLocaleString();
        }
        
        return uiData;
    }
    
    async loadUserInfo() {
        try {
            // Get JWT token and include it in the request
            const token = this.tokenManager.getAccessToken();
            if (!token) {
                throw new Error('No access token available');
            }
            
            const response = await this.httpClient.get('/api/auth/user', {
                headers: {
                    'Authorization': `Bearer ${token}`
                }
            });
            
            if (response.data && response.data.user) {
                this.currentUser = response.data.user;
                console.log('[SOURCE-MANAGER] User info loaded:', this.currentUser.username);
                // Cache user info for fallback
                localStorage.setItem('user_info', JSON.stringify(this.currentUser));
            }
        } catch (error) {
            console.error('[SOURCE-MANAGER] Failed to load user info:', error);
            
            // Handle 401 Unauthorized - token might be expired
            if (error.message.includes('401') || error.response?.status === 401) {
                console.log('[SOURCE-MANAGER] Token expired or invalid, attempting refresh...');
                try {
                    // Try to refresh the token
                    const refreshed = await this.tokenManager.refreshAccessToken();
                    if (refreshed) {
                        console.log('[SOURCE-MANAGER] Token refreshed, retrying user info request...');
                        // Retry the request with new token
                        const newToken = this.tokenManager.getAccessToken();
                        const response = await this.httpClient.get('/api/auth/user', {
                            headers: {
                                'Authorization': `Bearer ${newToken}`
                            }
                        });
                        
                        if (response.data && response.data.user) {
                            this.currentUser = response.data.user;
                            console.log('[SOURCE-MANAGER] User info loaded after token refresh:', this.currentUser.username);
                            localStorage.setItem('user_info', JSON.stringify(this.currentUser));
                            return;
                        }
                    }
                } catch (refreshError) {
                    console.error('[SOURCE-MANAGER] Token refresh failed:', refreshError);
                }
                
                // If refresh failed, redirect to login
                console.log('[SOURCE-MANAGER] Authentication failed, redirecting to login');
                this.handleAuthenticationFailure();
                return;
            }
            
            // Use cached user info as fallback for other errors
            const cachedUser = localStorage.getItem('user_info');
            if (cachedUser) {
                this.currentUser = JSON.parse(cachedUser);
                console.log('[SOURCE-MANAGER] Using cached user info as fallback');
            } else {
                // No cached info and request failed - handle authentication failure
                this.handleAuthenticationFailure();
            }
        }
    }
    
    handleInitializationError(error) {
        console.error('[SOURCE-MANAGER] Critical initialization error:', error);
        
        // Show error to user and redirect to login
        if (window.popupManager) {
            window.popupManager.showError({
                title: 'Initialization Error',
                message: 'Failed to initialize the application. Please log in again.',
                buttonText: 'Go to Login'
            }).then(() => {
                this.logout();
            });
        } else {
            alert('Initialization error. Please log in again.');
            this.logout();
        }
    }
    
    handleAuthenticationFailure() {
        console.log('[SOURCE-MANAGER] Handling authentication failure...');
        
        // Clear authentication data
        if (this.tokenManager) {
            this.tokenManager.clearAuthData();
        }
        
        // Clear local storage
        localStorage.removeItem('session');
        localStorage.removeItem('session_validated');
        localStorage.removeItem('user_info');
        
        // Show custom authentication popup if popup manager is available
        if (window.popupManager) {
            window.popupManager.showCustom({
                title: '', // No title since it's in the custom content
                content: `
                    <div class="authentication-modal">
                        <!-- Modal Header -->
                        <div class="flex items-center justify-between px-6 py-4 border-b border-neutral-200">
                            <h2 class="text-xl text-neutral-900">Authentication Required</h2>
                            <button class="p-1 hover:bg-neutral-100 rounded-md transition-colors" onclick="window.popupManager.close()">
                                <i class="text-neutral-500 text-lg">✕</i>
                            </button>
                        </div>

                        <!-- Modal Content -->
                        <div class="px-6 py-6">
                            <div class="flex flex-col items-center justify-center text-center">
                                <!-- Warning Icon -->
                                <div class="w-12 h-12 bg-yellow-100 rounded-full flex items-center justify-center mb-4">
                                    <i class="text-yellow-600 text-xl">⚠</i>
                                </div>
                                
                                <!-- Message -->
                                <h3 class="text-lg text-neutral-900 mb-2">
                                    Your session has expired
                                </h3>
                                
                                <p class="text-sm text-neutral-600 mb-6">
                                    Please log in again to continue accessing your account and dashboard.
                                </p>
                            </div>
                        </div>

                        <!-- Modal Footer -->
                        <div class="flex justify-end gap-3 px-6 py-4 border-t border-neutral-200">
                            <button class="px-4 py-2 text-sm text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50 focus:outline-none focus:ring-2 focus:ring-neutral-500 transition-colors" onclick="window.popupManager.close()">
                                Cancel
                            </button>
                            <button class="px-4 py-2 text-sm text-white bg-blue-600 rounded-md hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-blue-500 transition-colors" onclick="window.location.replace('/login-page.html')">
                                Login Again
                            </button>
                        </div>
                    </div>
                `,
                buttons: [], // No buttons since we handle them in custom content
                closeOnBackdrop: true,
                closeOnEscape: true
            });
        } else {
            // Fallback to alert and redirect
            alert('Your session has expired. Please log in again.');
            window.location.replace('/login-page.html');
        }
    }
    
    async logout() {
        try {
            console.log('[SOURCE-MANAGER] Logging out...');
            
            // Call logout API
            if (this.httpClient) {
                await this.httpClient.post('/api/auth/logout');
            }
            
            // Clear authentication data
            if (this.tokenManager) {
                this.tokenManager.clearAuthData();
            }
            
            // Clear local storage
            localStorage.removeItem('session');
            localStorage.removeItem('session_validated');
            
            // Disconnect WebSocket
            if (this.wsClient) {
                this.wsClient.close(); // Use close() to remove from shared storage
            }
            
            // Redirect to login page
            window.location.replace('/login-page.html');
            
        } catch (error) {
            console.error('[SOURCE-MANAGER] Logout error:', error);
            // Force redirect even if API call fails
            window.location.replace('/login-page.html');
        }
    }
    
    // Getters for UI components
    getHttpClient() {
        return this.httpClient;
    }
    
    getTokenManager() {
        return this.tokenManager;
    }
    
    getAuthManager() {
        return this.authManager;
    }
    
    getWebSocketClient() {
        return this.wsClient;
    }
    
    getCurrentUser() {
        return this.currentUser;
    }
    
    isInitialized() {
        return this.initialized;
    }
}

// Create global instance
window.sourceManager = new SourceManager();

// Export for use in other modules
export { SourceManager };
export default SourceManager;

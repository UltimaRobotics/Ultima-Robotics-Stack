/**
 * Network Benchmark Core Component
 * Handles the main business logic and API communications for network benchmarking
 */

class NetworkBenchmark {
    constructor() {
        this.servers = [];
        this.filteredServers = [];
        this.currentTest = null;
        this.testResults = [];
        this.filters = {
            keyword: '',
            continent: '',
            country: '',
            site: '',
            provider: '',
            host: '',
            port: '',
            minSpeed: 0
        };
        this.pagination = {
            currentPage: 1,
            itemsPerPage: 10,
            totalItems: 0
        };
        this.isInitialized = false;
        this.isConnected = false;
        this.lastDataReceived = null;
        this.sourceManager = null;
        this.loadingUI = null;
        this.runtimeUI = null;
    }

    /**
     * Initialize the network benchmark component
     */
    async initialize() {
        try {
            // Get source manager reference for WebSocket access
            this.sourceManager = window.sourceManager;
            
            // Get UI component references
            this.loadingUI = window.networkBenchmarkLoadingUI;
            this.runtimeUI = window.networkBenchmarkRuntimeUI;
            
            // Initialize UI components in order
            if (window.networkBenchmarkUI) {
                await window.networkBenchmarkUI.initialize();
            }
            
            // Setup WebSocket event listeners
            this.setupWebSocketListeners();
            
            this.isInitialized = true;
            console.log('[NETWORK-BENCHMARK] Component initialized successfully');
            return true;
            
        } catch (error) {
            console.error('[NETWORK-BENCHMARK] Failed to initialize:', error);
            return false;
        }
    }

    /**
     * Setup WebSocket event listeners
     */
    setupWebSocketListeners() {
        if (this.sourceManager && this.sourceManager.websocket) {
            this.sourceManager.websocket.addEventListener('message', (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleWebSocketMessage(data);
                } catch (error) {
                    console.error('[NETWORK-BENCHMARK] Failed to parse WebSocket message:', error);
                }
            });
        }
    }

    /**
     * Handle WebSocket messages
     */
    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'server_status':
                this.handleServerStatusUpdate(data);
                break;
            case 'test_result':
                this.handleTestResult(data);
                break;
            case 'test_update':
                this.handleTestUpdate(data);
                break;
            case 'error':
                this.handleError(data);
                break;
            default:
                console.log('[NETWORK-BENCHMARK] Unknown message type:', data.type);
        }
    }

    /**
     * Handle server status update
     */
    handleServerStatusUpdate(data) {
        this.servers = data.servers || [];
        this.filteredServers = [...this.servers];
        this.lastDataReceived = data.timestamp || Date.now();
        
        // Apply current filters
        this.applyFilters();
        
        // Update UI
        this.updateUI();
    }

    /**
     * Handle test result
     */
    handleTestResult(data) {
        this.testResults.push(data.result);
        
        // Update UI through runtime UI
        if (this.runtimeUI) {
            this.runtimeUI.handleTestResult(data);
        }
    }

    /**
     * Handle test update
     */
    handleTestUpdate(data) {
        // Update UI through runtime UI
        if (this.runtimeUI) {
            this.runtimeUI.handleTestUpdate(data);
        }
    }

    /**
     * Handle error message
     */
    handleError(data) {
        // Log error to console but don't show to user
        console.error('[NETWORK-BENCHMARK] Server error:', data.message);
    }

    /**
     * Update UI components
     */
    updateUI() {
        if (window.networkBenchmarkUI) {
            // Update server statistics
            const stats = this.calculateServerStats();
            window.networkBenchmarkUI.updateServerStats(stats);
            
            // Update server table
            window.networkBenchmarkUI.updateServerTable(this.getPaginatedServers());
        }
    }

    /**
     * Calculate server statistics
     */
    calculateServerStats() {
        const total = this.servers.length;
        const online = this.servers.filter(server => server.status === 'online').length;
        const offline = this.servers.filter(server => server.status === 'offline').length;
        const avgPing = this.calculateAveragePing();
        
        return {
            total,
            online,
            offline,
            avgPing,
            lastUpdated: this.lastDataReceived ? new Date(this.lastDataReceived).toLocaleString() : '--'
        };
    }

    /**
     * Calculate average ping time
     */
    calculateAveragePing() {
        const onlineServers = this.servers.filter(server => server.status === 'online' && server.ping);
        if (onlineServers.length === 0) return 0;
        
        const totalPing = onlineServers.reduce((sum, server) => sum + server.ping, 0);
        return Math.round(totalPing / onlineServers.length);
    }

    /**
     * Get paginated servers
     */
    getPaginatedServers() {
        const start = (this.pagination.currentPage - 1) * this.pagination.itemsPerPage;
        const end = start + this.pagination.itemsPerPage;
        return this.filteredServers.slice(start, end);
    }

    /**
     * Apply filters to server list
     */
    applyFilters(filters = null) {
        if (filters) {
            this.filters = { ...this.filters, ...filters };
        }
        
        this.filteredServers = this.servers.filter(server => {
            return this.matchesKeyword(server, this.filters.keyword) &&
                   this.matchesContinent(server, this.filters.continent) &&
                   this.matchesCountry(server, this.filters.country) &&
                   this.matchesSite(server, this.filters.site) &&
                   this.matchesProvider(server, this.filters.provider) &&
                   this.matchesHost(server, this.filters.host) &&
                   this.matchesPort(server, this.filters.port) &&
                   this.matchesSpeed(server, this.filters.minSpeed);
        });
        
        this.pagination.totalItems = this.filteredServers.length;
        this.pagination.currentPage = 1;
    }

    /**
     * Filter helper methods
     */
    matchesKeyword(server, keyword) {
        if (!keyword) return true;
        const searchLower = keyword.toLowerCase();
        return server.name.toLowerCase().includes(searchLower) ||
               server.host.toLowerCase().includes(searchLower) ||
               server.region.toLowerCase().includes(searchLower);
    }

    matchesContinent(server, continent) {
        if (!continent || continent === 'All Continents') return true;
        return server.continent === continent;
    }

    matchesCountry(server, country) {
        if (!country || country === 'All Countries') return true;
        return server.country === country;
    }

    matchesSite(server, site) {
        if (!site || site === 'All Sites') return true;
        return server.site === site;
    }

    matchesProvider(server, provider) {
        if (!provider || provider === 'All Providers') return true;
        return server.provider === provider;
    }

    matchesHost(server, host) {
        if (!host) return true;
        return server.host.toLowerCase().includes(host.toLowerCase());
    }

    matchesPort(server, port) {
        if (!port) return true;
        return server.ports.includes(port);
    }

    matchesSpeed(server, minSpeed) {
        if (!minSpeed || minSpeed === 0) return true;
        return (server.speed || 0) >= minSpeed;
    }

    /**
     * Start traceroute test
     */
    startTraceroute() {
        if (this.currentTest) {
            console.warn('[NETWORK-BENCHMARK] Test already in progress');
            return;
        }

        const targetHost = document.getElementById('traceroute-target-host')?.value;
        const maxHops = parseInt(document.getElementById('traceroute-max-hops')?.value) || 30;
        const timeout = parseInt(document.getElementById('traceroute-timeout')?.value) || 5;
        const protocol = document.querySelector('input[name="traceroute-protocol"]:checked')?.value || 'icmp';

        if (!targetHost) {
            if (window.networkBenchmarkUI) {
                window.networkBenchmarkUI.showErrorMessage('Please enter a target host');
            }
            return;
        }

        this.currentTest = {
            type: 'traceroute',
            target: targetHost,
            maxHops,
            timeout,
            protocol,
            startTime: Date.now()
        };

        // Send test request to backend
        this.sendTestRequest('traceroute', {
            target: targetHost,
            maxHops,
            timeout,
            protocol
        });

        // Update UI to show test in progress
        this.updateTracerouteUI('running');
    }

    /**
     * Stop traceroute test
     */
    stopTraceroute() {
        if (!this.currentTest || this.currentTest.type !== 'traceroute') {
            return;
        }

        // Send stop request to backend
        this.sendTestRequest('stop_traceroute', {
            testId: this.currentTest.startTime
        });

        // Reset current test
        this.currentTest = null;

        // Update UI
        this.updateTracerouteUI('stopped');
    }

    /**
     * Update traceroute UI state
     */
    updateTracerouteUI(state) {
        const startBtn = document.getElementById('start-traceroute-btn');
        const stopBtn = document.getElementById('stop-traceroute-btn');
        const statusSection = document.getElementById('traceroute-status-section');

        switch (state) {
            case 'running':
                if (startBtn) startBtn.style.display = 'none';
                if (stopBtn) stopBtn.style.display = 'flex';
                if (statusSection) statusSection.classList.remove('hidden');
                break;
            case 'stopped':
            case 'completed':
                if (startBtn) startBtn.style.display = 'flex';
                if (stopBtn) stopBtn.style.display = 'none';
                break;
        }
    }

    /**
     * Send test request to backend
     */
    sendTestRequest(testType, params) {
        if (this.sourceManager && this.sourceManager.websocket) {
            const message = {
                type: testType,
                timestamp: Date.now(),
                ...params
            };
            
            this.sourceManager.websocket.send(JSON.stringify(message));
        }
    }

    /**
     * Setup WebSocket event listeners
     */
    setupWebSocketListeners() {
        // Listen for WebSocket messages
        document.addEventListener('websocketMessage', (e) => {
            const message = e.detail;
            if (message.type === 'network_benchmark_update') {
                this.handleNetworkBenchmarkUpdate(message.data);
            }
        });
        
        // Listen for network benchmark data requests
        document.addEventListener('networkBenchmarkDataReceived', (e) => {
            this.handleDataReceived(e.detail.data);
        });
        
        console.log('[NETWORK-BENCHMARK] WebSocket listeners setup complete');
    }

    /**
     * Load initial data
     */
    async loadInitialData() {
        try {
            console.log('[NETWORK-BENCHMARK] Loading initial network benchmark data...');
            
            // Don't load mock data - wait for real WebSocket data
            // The loading UI will show skeleton screens until data arrives
            console.log('[NETWORK-BENCHMARK] Waiting for real WebSocket data...');
            
            // Initialize empty data arrays - will be populated by WebSocket messages
            this.servers = [];
            this.filteredServers = [];
            
            // Update connection status to show we're waiting for real data
            this.updateConnectionStatus(false);
            
            // Show loading states in the UI only if UI is initialized
            if (window.networkBenchmarkUI && window.networkBenchmarkUI.isInitialized) {
                window.networkBenchmarkUI.showInitialLoadingState();
            } else {
                console.log('[NETWORK-BENCHMARK] UI not yet initialized, skipping UI updates');
            }
            
            console.log('[NETWORK-BENCHMARK] Initial data setup complete - waiting for WebSocket data');
            
        } catch (error) {
            console.error('[NETWORK-BENCHMARK] Failed to initialize:', error);
            this.updateConnectionStatus(false);
        }
    }

    /**
     * Handle WebSocket network benchmark updates
     */
    handleNetworkBenchmarkUpdate(data) {
        console.log('[NETWORK-BENCHMARK] Received network benchmark update:', data);
        
        // Ensure DOM is ready before processing
        if (!this.isDOMReady()) {
            console.log('[NETWORK-BENCHMARK] DOM not ready, retrying in 50ms...');
            setTimeout(() => {
                this.handleNetworkBenchmarkUpdate(data);
            }, 50);
            return;
        }
        
        let hasUpdates = false;
        
        // Update connection status to connected since we're receiving data
        this.updateConnectionStatus(true);
        
        // Update servers if provided
        if (data.servers) {
            console.log('[NETWORK-BENCHMARK] Processing servers update:', data.servers);
            const oldServers = [...this.servers];
            
            // Check if servers data actually changed
            const serversChanged = this.hasServersChanged(oldServers, data.servers);
            
            if (serversChanged) {
                this.servers = data.servers;
                console.log('[NETWORK-BENCHMARK] Servers data changed, updating display');
                
                // Apply current filters and update display
                this.applyFilters();
                this.updateUI();
                
                // Update UI with WebSocket data
                if (window.networkBenchmarkUI) {
                    window.networkBenchmarkUI.handleWebSocketUpdate({
                        lastUpdated: data.lastUpdated,
                        statusChanges: this.detectServerStatusChanges(oldServers, this.servers)
                    });
                }
                
                // Update last received timestamp
                this.lastDataReceived = new Date();
                
                // Check for status changes
                const statusChanges = this.detectServerStatusChanges(oldServers, this.servers);
                if (statusChanges.length > 0) {
                    statusChanges.forEach(change => {
                        this.showNotification(`Server ${change.name} status changed to ${change.status}`, 'warning');
                    });
                }
                
                console.log('[NETWORK-BENCHMARK] Servers updated via WebSocket:', this.servers.length, 'servers');
                hasUpdates = true;
            } else {
                console.log('[NETWORK-BENCHMARK] Servers data unchanged, skipping update');
            }
        }
        
        // Update test results if provided
        if (data.testResults) {
            this.testResults = data.testResults;
            console.log('[NETWORK-BENCHMARK] Test results updated via WebSocket:', this.testResults.length, 'results');
            hasUpdates = true;
        }
        
        // Update statistics if provided
        if (data.statistics) {
            this.updateStatistics(data.statistics);
            hasUpdates = true;
        }
        
        // Show connection indicator if we have updates
        if (hasUpdates) {
            this.showConnectionIndicator();
        }
    }

    /**
     * Handle generic data received event
     */
    handleDataReceived(data) {
        console.log('[NETWORK-BENCHMARK] Data received:', data);
        
        // Update connection status to connected since we're receiving data
        this.updateConnectionStatus(true);
        
        if (data.servers) {
            this.servers = data.servers;
            this.applyFilters();
            this.updateUI();
        }
        
        if (data.testResults) {
            this.testResults = data.testResults;
        }
        
        if (data.statistics) {
            this.updateStatistics(data.statistics);
        }
        
        // Show connection indicator
        this.showConnectionIndicator();
    }

    /**
     * Check if DOM is ready
     */
    isDOMReady() {
        return document.readyState === 'complete' || document.readyState === 'interactive';
    }

    /**
     * Check if servers data has changed
     */
    hasServersChanged(oldServers, newServers) {
        if (oldServers.length !== newServers.length) {
            return true;
        }
        
        for (let i = 0; i < oldServers.length; i++) {
            const oldServer = oldServers[i];
            const newServer = newServers[i];
            
            if (oldServer.id !== newServer.id ||
                oldServer.status !== newServer.status ||
                oldServer.ping !== newServer.ping ||
                oldServer.speed !== newServer.speed) {
                return true;
            }
        }
        
        return false;
    }

    /**
     * Detect server status changes
     */
    detectServerStatusChanges(oldServers, newServers) {
        const changes = [];
        
        oldServers.forEach(oldServer => {
            const newServer = newServers.find(s => s.id === oldServer.id);
            if (newServer && oldServer.status !== newServer.status) {
                changes.push({
                    id: oldServer.id,
                    name: oldServer.name,
                    oldStatus: oldServer.status,
                    status: newServer.status
                });
            }
        });
        
        return changes;
    }

    /**
     * Update connection status
     */
    updateConnectionStatus(connected) {
        this.isConnected = connected;
        
        // Update UI connection status if available
        const statusElement = document.getElementById('benchmark-connection-status');
        if (statusElement) {
            const statusDot = statusElement.querySelector('#benchmark-status-dot');
            const statusText = statusElement.querySelector('#benchmark-connection-text');
            
            if (statusDot && statusText) {
                if (connected) {
                    statusDot.className = 'w-2 h-2 bg-green-500 rounded-full';
                    statusText.textContent = 'Connected';
                } else {
                    statusDot.className = 'w-2 h-2 bg-red-500 rounded-full';
                    statusText.textContent = 'Disconnected';
                }
            }
        }
    }

    /**
     * Update statistics
     */
    updateStatistics(statistics) {
        // Update any statistics displays
        console.log('[NETWORK-BENCHMARK] Statistics updated:', statistics);
    }

    /**
     * Show connection indicator
     */
    showConnectionIndicator() {
        // Show brief connection indicator
        const indicator = document.getElementById('benchmark-connection-indicator');
        if (indicator) {
            indicator.style.display = 'flex';
            setTimeout(() => {
                indicator.style.display = 'none';
            }, 2000);
        }
    }

    /**
     * Show notification
     */
    showNotification(message, type = 'info') {
        console.log(`[NETWORK-BENCHMARK] Notification (${type}): ${message}`);
        
        // If popup manager is available, use it
        if (window.popupManager) {
            window.popupManager.showNotification(message, type);
        } else {
            // Fallback to alert
            alert(message);
        }
    }

    /**
     * Refresh server status
     */
    async refreshServerStatus() {
        try {
            console.log('[NETWORK-BENCHMARK] Requesting server status refresh...');
            
            if (window.networkBenchmarkUI) {
                window.networkBenchmarkUI.showTestingStatus(true, 0, this.servers.length);
            }
            
            // Send WebSocket request for server status refresh
            if (this.sourceManager && this.sourceManager.getWebSocketClient) {
                const wsClient = this.sourceManager.getWebSocketClient();
                if (wsClient && wsClient.isConnected()) {
                    wsClient.send({
                        type: 'network_benchmark_request',
                        action: 'refresh_server_status',
                        timestamp: Date.now()
                    });
                    console.log('[NETWORK-BENCHMARK] Server status refresh request sent via WebSocket');
                } else {
                    console.warn('[NETWORK-BENCHMARK] WebSocket not connected, cannot refresh server status');
                    this.showNotification('WebSocket not connected. Please check your connection.', 'error');
                    if (window.networkBenchmarkUI) {
                        window.networkBenchmarkUI.showTestingStatus(false);
                    }
                }
            } else {
                console.warn('[NETWORK-BENCHMARK] Source manager or WebSocket client not available');
                this.showNotification('Unable to refresh server status. Please try again later.', 'error');
                if (window.networkBenchmarkUI) {
                    window.networkBenchmarkUI.showTestingStatus(false);
                }
            }
            
        } catch (error) {
            console.error('[NETWORK-BENCHMARK] Failed to refresh server status:', error);
            if (window.networkBenchmarkUI) {
                window.networkBenchmarkUI.showTestingStatus(false);
            }
            this.showNotification('Failed to refresh server status: ' + error.message, 'error');
        }
    }

    /**
     * Test all servers
     */
    async testAllServers() {
        try {
            console.log('[NETWORK-BENCHMARK] Requesting test for all servers...');
            
            if (window.networkBenchmarkUI) {
                window.networkBenchmarkUI.showTestingStatus(true, 0, this.servers.length);
            }
            
            // Send WebSocket request for testing all servers
            if (this.sourceManager && this.sourceManager.getWebSocketClient) {
                const wsClient = this.sourceManager.getWebSocketClient();
                if (wsClient && wsClient.isConnected()) {
                    wsClient.send({
                        type: 'network_benchmark_request',
                        action: 'test_all_servers',
                        servers: this.servers.map(s => s.id),
                        timestamp: Date.now()
                    });
                    console.log('[NETWORK-BENCHMARK] Test all servers request sent via WebSocket');
                } else {
                    console.warn('[NETWORK-BENCHMARK] WebSocket not connected, cannot test servers');
                    this.showNotification('WebSocket not connected. Please check your connection.', 'error');
                    if (window.networkBenchmarkUI) {
                        window.networkBenchmarkUI.showTestingStatus(false);
                    }
                }
            } else {
                console.warn('[NETWORK-BENCHMARK] Source manager or WebSocket client not available');
                this.showNotification('Unable to test servers. Please try again later.', 'error');
                if (window.networkBenchmarkUI) {
                    window.networkBenchmarkUI.showTestingStatus(false);
                }
            }
            
        } catch (error) {
            console.error('[NETWORK-BENCHMARK] Failed to test all servers:', error);
            if (window.networkBenchmarkUI) {
                window.networkBenchmarkUI.showTestingStatus(false);
            }
            this.showNotification('Failed to test servers: ' + error.message, 'error');
        }
    }

    /**
     * Test individual server
     */
    async testServer(serverId) {
        try {
            console.log(`[NETWORK-BENCHMARK] Requesting test for server: ${serverId}`);
            
            // Send WebSocket request for testing individual server
            if (this.sourceManager && this.sourceManager.getWebSocketClient) {
                const wsClient = this.sourceManager.getWebSocketClient();
                if (wsClient && wsClient.isConnected()) {
                    wsClient.send({
                        type: 'network_benchmark_request',
                        action: 'test_server',
                        serverId: serverId,
                        timestamp: Date.now()
                    });
                    console.log(`[NETWORK-BENCHMARK] Test server request sent via WebSocket for: ${serverId}`);
                } else {
                    console.warn('[NETWORK-BENCHMARK] WebSocket not connected, cannot test server');
                    this.showNotification('WebSocket not connected. Please check your connection.', 'error');
                }
            } else {
                console.warn('[NETWORK-BENCHMARK] Source manager or WebSocket client not available');
                this.showNotification('Unable to test server. Please try again later.', 'error');
            }
            
        } catch (error) {
            console.error(`[NETWORK-BENCHMARK] Failed to test server ${serverId}:`, error);
            this.showNotification('Failed to test server: ' + error.message, 'error');
        }
    }

    /**
     * View server configuration
     */
    viewServerConfig(serverId) {
        const server = this.servers.find(s => s.id === serverId);
        if (!server) {
            console.error(`Server not found: ${serverId}`);
            return;
        }
        
        // Show server configuration modal or details
        console.log('Server Configuration:', server);
        alert(`Server Configuration:\n\nName: ${server.name}\nRegion: ${server.region}\nHost: ${server.host}\nPorts: ${server.ports}\nStatus: ${server.status.toUpperCase()}\nPing: ${server.ping || 'N/A'}ms\nSpeed: ${server.speed}Mbps`);
    }

    /**
     * Apply filters to server list
     */
    applyFilters(filters = null) {
        if (filters) {
            this.filters = { ...this.filters, ...filters };
        }
        
        this.filteredServers = this.servers.filter(server => {
            // Keyword filter
            if (this.filters.keyword) {
                const keyword = this.filters.keyword.toLowerCase();
                if (!server.name.toLowerCase().includes(keyword) &&
                    !server.host.toLowerCase().includes(keyword) &&
                    !server.region.toLowerCase().includes(keyword)) {
                    return false;
                }
            }
            
            // Region filter
            if (this.filters.continent && this.filters.continent !== 'All Continents') {
                if (!server.region.includes(this.filters.continent)) {
                    return false;
                }
            }
            
            // Host filter
            if (this.filters.host) {
                if (!server.host.includes(this.filters.host)) {
                    return false;
                }
            }
            
            // Port filter
            if (this.filters.port) {
                if (!server.ports.includes(this.filters.port)) {
                    return false;
                }
            }
            
            // Minimum speed filter
            if (this.filters.minSpeed > 0) {
                if (server.speed < this.filters.minSpeed) {
                    return false;
                }
            }
            
            return true;
        });
        
        this.pagination.totalItems = this.filteredServers.length;
        this.pagination.currentPage = 1;
    }

    /**
     * Get paginated servers
     */
    getPaginatedServers() {
        const startIndex = (this.pagination.currentPage - 1) * this.pagination.itemsPerPage;
        const endIndex = startIndex + this.pagination.itemsPerPage;
        return this.filteredServers.slice(startIndex, endIndex);
    }

    /**
     * Update UI with current data
     */
    updateUI() {
        if (!window.networkBenchmarkUI) return;
        
        // Calculate statistics
        const stats = {
            total: this.servers.length,
            online: this.servers.filter(s => s.status !== 'unreachable').length,
            offline: this.servers.filter(s => s.status === 'unreachable').length,
            avgPing: this.calculateAveragePing(),
            lastUpdated: new Date().toLocaleString()
        };
        
        // Update UI components
        window.networkBenchmarkUI.updateServerStats(stats);
        window.networkBenchmarkUI.updateServerTable(this.getPaginatedServers());
    }

    /**
     * Calculate average ping
     */
    calculateAveragePing() {
        const reachableServers = this.servers.filter(s => s.ping !== null);
        if (reachableServers.length === 0) return 0;
        
        const totalPing = reachableServers.reduce((sum, server) => sum + server.ping, 0);
        return Math.round(totalPing / reachableServers.length);
    }

    /**
     * Start traceroute test
     */
    async startTraceroute() {
        try {
            const targetHost = document.getElementById('traceroute-target-host')?.value;
            const maxHops = parseInt(document.getElementById('traceroute-max-hops')?.value) || 30;
            const timeout = parseInt(document.getElementById('traceroute-timeout')?.value) || 5;
            const protocol = document.querySelector('input[name="traceroute-protocol"]:checked')?.value || 'icmp';
            
            if (!targetHost) {
                alert('Please enter a target host');
                return;
            }
            
            // Update UI to show running state with null checks
            const startBtn = document.getElementById('start-traceroute-btn');
            if (startBtn) startBtn.style.display = 'none';
            
            const resetBtn = document.getElementById('reset-traceroute-btn');
            if (resetBtn) resetBtn.style.display = 'none';
            
            const stopBtn = document.getElementById('stop-traceroute-btn');
            if (stopBtn) stopBtn.style.display = 'flex';
            
            // Show status section
            const statusSection = document.getElementById('traceroute-status-section');
            if (statusSection) statusSection.classList.remove('hidden');
            
            // Show results section
            const resultsSection = document.getElementById('traceroute-results');
            if (resultsSection) resultsSection.style.display = 'block';
            
            // Initialize empty results array
            const tracerouteResults = [];
            
            // Update initial status
            if (window.networkBenchmarkLoadingUI) {
                window.networkBenchmarkLoadingUI.updateTracerouteStatus('Starting...', 0, maxHops, '00:00');
            }
            
            // Simulate traceroute
            for (let hop = 1; hop <= Math.min(maxHops, 10); hop++) {
                // Update status
                const elapsed = this.formatTime(hop * 2);
                if (window.networkBenchmarkLoadingUI) {
                    window.networkBenchmarkLoadingUI.updateTracerouteStatus('Running', hop, maxHops, elapsed);
                }
                
                // Simulate hop data
                const hopData = {
                    hop: hop,
                    ip: `192.168.${hop}.${hop * 10}`,
                    hostname: `hop-${hop}.example.com`,
                    rtt1: `${Math.floor(Math.random() * 100) + 10}ms`,
                    rtt2: `${Math.floor(Math.random() * 100) + 10}ms`,
                    rtt3: `${Math.floor(Math.random() * 100) + 10}ms`
                };
                const avgRtt = Math.round((parseInt(hopData.rtt1) + parseInt(hopData.rtt2) + parseInt(hopData.rtt3)) / 3);
                hopData.avgRtt = `${avgRtt}ms`;
                
                // Add to results array
                tracerouteResults.push(hopData);
                
                // Update results table using the proper method
                if (window.networkBenchmarkLoadingUI) {
                    window.networkBenchmarkLoadingUI.updateTracerouteResults(tracerouteResults);
                }
                
                await this.simulateApiCall(500);
            }
            
            // Update final status
            if (window.networkBenchmarkLoadingUI) {
                window.networkBenchmarkLoadingUI.updateTracerouteStatus('Completed', maxHops, maxHops, this.formatTime(maxHops * 2));
            }
            
            // Update total hops with null check
            const totalHopsElement = document.getElementById('traceroute-total-hops');
            if (totalHopsElement) totalHopsElement.textContent = Math.min(maxHops, 10);
            
            // Reset buttons with null checks (reuse existing variables)
            if (startBtn) startBtn.style.display = 'flex';
            if (resetBtn) resetBtn.style.display = 'flex';
            if (stopBtn) stopBtn.style.display = 'none';
            
        } catch (error) {
            console.error('Traceroute test failed:', error);
            this.stopTraceroute();
        }
    }

    /**
     * Stop traceroute test
     */
    stopTraceroute() {
        // Update UI to show stopped state with null checks
        const startBtn = document.getElementById('start-traceroute-btn');
        if (startBtn) startBtn.style.display = 'flex';
        
        const resetBtn = document.getElementById('reset-traceroute-btn');
        if (resetBtn) resetBtn.style.display = 'flex';
        
        const stopBtn = document.getElementById('stop-traceroute-btn');
        if (stopBtn) stopBtn.style.display = 'none';
        
        // Update status to show stopped
        if (window.networkBenchmarkLoadingUI) {
            window.networkBenchmarkLoadingUI.updateTracerouteStatus('Stopped', 0, 30, '00:00');
        }
    }

    /**
     * Start ping test
     */
    async startPingTest() {
        try {
            const targetHost = document.getElementById('ping-target-host')?.value;
            const count = parseInt(document.getElementById('ping-count')?.value) || 10;
            const size = parseInt(document.getElementById('ping-size')?.value) || 64;
            const interval = parseInt(document.getElementById('ping-interval')?.value) || 1;
            const timeout = parseInt(document.getElementById('ping-timeout')?.value) || 5;
            const continuous = document.getElementById('continuous-ping')?.checked || false;
            
            if (!targetHost) {
                alert('Please enter a target host');
                return;
            }
            
            // Switch to running UI
            document.getElementById('ping-config-content').style.display = 'none';
            document.getElementById('ping-running-content').style.display = 'block';
            
            // Copy configuration to running UI
            document.getElementById('ping-target-host-running').value = targetHost;
            document.getElementById('ping-count-running').value = count;
            document.getElementById('ping-size-running').value = size;
            
            // Clear previous results
            const tbody = document.getElementById('ping-results-tbody');
            tbody.innerHTML = '';
            
            let packetsSent = 0;
            let packetsReceived = 0;
            let totalLatency = 0;
            
            // Run ping test
            for (let i = 1; i <= count; i++) {
                packetsSent++;
                
                // Simulate ping
                const success = Math.random() > 0.1; // 90% success rate
                const latency = success ? Math.floor(Math.random() * 100) + 10 : null;
                
                if (success) {
                    packetsReceived++;
                    totalLatency += latency;
                }
                
                // Add result row
                const row = `
                    <tr>
                        <td class="px-4 py-3 text-sm text-neutral-900">${i}</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${new Date().toLocaleTimeString()}</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${size}</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${Math.floor(Math.random() * 64) + 1}</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${latency ? latency + 'ms' : 'Timeout'}</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">
                            <span class="px-2 py-1 text-xs rounded ${success ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}">
                                ${success ? 'Success' : 'Failed'}
                            </span>
                        </td>
                    </tr>
                `;
                tbody.insertAdjacentHTML('beforeend', row);
                
                // Update statistics
                const avgLatency = packetsReceived > 0 ? Math.round(totalLatency / packetsReceived) : 0;
                if (window.networkBenchmarkLoadingUI) {
                    window.networkBenchmarkLoadingUI.updatePingStats(packetsSent, packetsReceived, avgLatency);
                }
                
                await this.simulateApiCall(interval * 1000);
            }
            
        } catch (error) {
            console.error('Ping test failed:', error);
            this.stopPingTest();
        }
    }

    /**
     * Stop ping test
     */
    stopPingTest() {
        // Switch back to config UI
        document.getElementById('ping-config-content').style.display = 'block';
        document.getElementById('ping-running-content').style.display = 'none';
    }

    /**
     * Start DNS lookup
     */
    async startDnsLookup() {
        try {
            const domain = document.getElementById('domain-input')?.value;
            const recordType = document.getElementById('record-type')?.value;
            const timeout = parseInt(document.getElementById('timeout-input')?.value) || 5;
            const dnsServer = document.getElementById('dns-server')?.value;
            
            if (!domain) {
                alert('Please enter a domain or IP address');
                return;
            }
            
            // Switch to running UI
            document.getElementById('dns-selection-ui').style.display = 'none';
            document.getElementById('dns-running-ui').style.display = 'block';
            
            // Copy configuration to running UI
            document.getElementById('domain-input-running').value = domain;
            document.getElementById('record-type-running').value = recordType;
            document.getElementById('timeout-input-running').value = timeout;
            document.getElementById('dns-server-running').value = dnsServer;
            
            // Clear previous results
            document.getElementById('dns-results-content').innerHTML = '';
            
            // Simulate DNS lookup
            await this.simulateApiCall(1000);
            
            // Generate mock results
            const mockResults = this.generateMockDnsResults(domain, recordType);
            
            // Display results
            const resultsContent = document.getElementById('dns-results-content');
            resultsContent.innerHTML = mockResults.map(record => `
                <div class="flex items-start space-x-3 pb-3 border-b border-neutral-100">
                    <i class="fa-solid fa-circle-check text-neutral-600 mt-1"></i>
                    <div class="flex-1">
                        <div class="flex items-center justify-between mb-1">
                            <span class="text-sm text-neutral-500">${record.type} Record</span>
                            <span class="text-xs text-neutral-400">TTL: ${record.ttl}</span>
                        </div>
                        <p class="text-neutral-900">${record.value}</p>
                    </div>
                </div>
            `).join('');
            
            // Update query information
            document.getElementById('query-time').textContent = '42ms';
            document.getElementById('server-used').textContent = dnsServer || '8.8.8.8';
            document.getElementById('records-count').textContent = mockResults.length;
            
        } catch (error) {
            console.error('DNS lookup failed:', error);
        }
    }

    /**
     * Generate mock DNS results
     */
    generateMockDnsResults(domain, recordType) {
        const results = [];
        
        switch (recordType) {
            case 'A':
                results.push(
                    { type: 'A', value: '93.184.216.34', ttl: 300 },
                    { type: 'A', value: '93.184.216.35', ttl: 300 }
                );
                break;
            case 'AAAA':
                results.push(
                    { type: 'AAAA', value: '2606:2800:220:1:248:1893:25c8:1946', ttl: 300 }
                );
                break;
            case 'CNAME':
                results.push(
                    { type: 'CNAME', value: 'cdn.example.com', ttl: 3600 }
                );
                break;
            case 'MX':
                results.push(
                    { type: 'MX', value: 'mail.example.com (10)', ttl: 3600 },
                    { type: 'MX', value: 'backup.example.com (20)', ttl: 3600 }
                );
                break;
            case 'NS':
                results.push(
                    { type: 'NS', value: 'ns1.example.com', ttl: 86400 },
                    { type: 'NS', value: 'ns2.example.com', ttl: 86400 }
                );
                break;
            case 'TXT':
                results.push(
                    { type: 'TXT', value: '"v=spf1 include:_spf.example.com ~all"', ttl: 3600 }
                );
                break;
            case 'SOA':
                results.push(
                    { type: 'SOA', value: 'ns1.example.com admin.example.com 2023010101 7200 3600 1209600 3600', ttl: 86400 }
                );
                break;
            case 'PTR':
                results.push(
                    { type: 'PTR', value: 'example.com', ttl: 300 }
                );
                break;
        }
        
        return results;
    }

    /**
     * Start bandwidth test
     */
    async startBandwidthTest() {
        try {
            const server = document.getElementById('bandwidth-server')?.value;
            const duration = parseInt(document.getElementById('bandwidth-duration')?.value) || 10;
            const protocol = document.querySelector('input[name="bandwidth-protocol"]:checked')?.value || 'tcp';
            const parallel = parseInt(document.getElementById('bandwidth-parallel')?.value) || 5;
            const bidirectional = document.getElementById('bandwidth-bidirectional')?.checked || false;
            const reverse = document.getElementById('bandwidth-reverse')?.checked || false;
            
            // Switch to running UI
            document.getElementById('bandwidth-selection-ui').style.display = 'none';
            document.getElementById('bandwidth-running-ui').style.display = 'block';
            
            // Copy configuration to running UI
            document.getElementById('bandwidth-server-running').value = server;
            document.getElementById('bandwidth-duration-running').value = duration;
            document.querySelector(`input[name="bandwidth-protocol-running"][value="${protocol}"]`).checked = true;
            
            // Clear previous results
            document.getElementById('bandwidth-results-tbody').innerHTML = '';
            
            // Run bandwidth test
            for (let second = 1; second <= duration; second++) {
                // Generate mock metrics
                const downloadSpeed = Math.floor(Math.random() * 200) + 100;
                const uploadSpeed = Math.floor(Math.random() * 100) + 50;
                const latency = Math.floor(Math.random() * 20) + 5;
                const jitter = Math.floor(Math.random() * 5) + 1;
                
                // Update metrics
                if (window.networkBenchmarkLoadingUI) {
                    window.networkBenchmarkLoadingUI.updateBandwidthMetrics(
                        downloadSpeed, uploadSpeed, latency, jitter, second, duration
                    );
                }
                
                // Add result row
                const row = `
                    <tr>
                        <td class="px-4 py-3 text-sm text-neutral-900">${second}</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${downloadSpeed} Mbps</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${uploadSpeed} Mbps</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${latency} ms</td>
                        <td class="px-4 py-3 text-sm text-neutral-900">${jitter} ms</td>
                    </tr>
                `;
                document.getElementById('bandwidth-results-tbody').insertAdjacentHTML('afterbegin', row);
                
                await this.simulateApiCall(1000);
            }
            
        } catch (error) {
            console.error('Bandwidth test failed:', error);
            this.stopBandwidthTest();
        }
    }

    /**
     * Stop bandwidth test
     */
    stopBandwidthTest() {
        // Switch back to selection UI
        document.getElementById('bandwidth-selection-ui').style.display = 'block';
        document.getElementById('bandwidth-running-ui').style.display = 'none';
    }

    /**
     * Simulate API call with delay
     */
    simulateApiCall(delay) {
        return new Promise(resolve => setTimeout(resolve, delay));
    }

    /**
     * Format time in MM:SS format
     */
    formatTime(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = seconds % 60;
        return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
    }

    /**
     * Export test results
     */
    exportResults(testType) {
        const results = this.testResults.filter(r => r.type === testType);
        const csv = this.convertToCSV(results);
        
        const blob = new Blob([csv], { type: 'text/csv' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${testType}-results-${new Date().toISOString().split('T')[0]}.csv`;
        a.click();
        window.URL.revokeObjectURL(url);
    }

    /**
     * Convert results to CSV format
     */
    convertToCSV(results) {
        if (results.length === 0) return '';
        
        const headers = Object.keys(results[0]);
        const csvRows = [headers.join(',')];
        
        for (const result of results) {
            const values = headers.map(header => {
                const value = result[header];
                return typeof value === 'string' ? `"${value}"` : value;
            });
            csvRows.push(values.join(','));
        }
        
        return csvRows.join('\n');
    }

    /**
     * Destroy the component
     */
    destroy() {
        // Clean up UI components
        if (window.networkBenchmarkUI) {
            window.networkBenchmarkUI.destroy();
        }
        
        if (window.networkBenchmarkLoadingUI) {
            window.networkBenchmarkLoadingUI.destroy();
        }
        
        // Reset data
        this.servers = [];
        this.filteredServers = [];
        this.currentTest = null;
        this.testResults = [];
        this.isInitialized = false;
    }
}

// Initialize global instance
window.networkBenchmark = new NetworkBenchmark();

// Auto-initialize when DOM is ready and container exists
document.addEventListener('DOMContentLoaded', () => {
    if (window.networkBenchmark) {
        // Check if container exists, if not, wait for it
        const checkContainer = () => {
            const container = document.getElementById('network-benchmark-container');
            if (container) {
                window.networkBenchmark.initialize();
            } else {
                // Container not ready yet, try again in 100ms
                setTimeout(checkContainer, 100);
            }
        };
        checkContainer();
    }
});

/**
 * Network Benchmark Runtime UI Component
 * Handles runtime operations, data updates, and popup content for network diagnostic tools
 */

class NetworkBenchmarkRuntimeUI {
    constructor() {
        this.loadingUI = null;
        this.websocket = null;
        this.isDataLoaded = false;
        this.servers = [];
        this.testResults = {
            traceroute: [],
            ping: [],
            dns: [],
            bandwidth: []
        };
        this.isInitialized = false;
    }

    /**
     * Initialize the runtime UI
     */
    initialize(loadingUI) {
        this.loadingUI = loadingUI;
        this.initializeWebSocket();
        this.attachEventListeners();
        this.isInitialized = true;
    }

    /**
     * Initialize WebSocket connection for server status
     */
    initializeWebSocket() {
        try {
            // Create WebSocket connection
            const wsUrl = `ws://${window.location.host}/ws/network-benchmark`;
            this.websocket = new WebSocket(wsUrl);

            this.websocket.onopen = () => {
                console.log('Network benchmark WebSocket connected');
                this.requestServerStatus();
            };

            this.websocket.onmessage = (event) => {
                const data = JSON.parse(event.data);
                this.handleWebSocketMessage(data);
            };

            this.websocket.onerror = (error) => {
                console.error('Network benchmark WebSocket error:', error);
                this.handleConnectionError();
            };

            this.websocket.onclose = () => {
                console.log('Network benchmark WebSocket disconnected');
                this.handleConnectionClose();
            };

        } catch (error) {
            console.error('Failed to initialize WebSocket:', error);
            this.handleConnectionError();
        }
    }

    /**
     * Request server status from backend
     */
    requestServerStatus() {
        if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            this.websocket.send(JSON.stringify({
                type: 'request_server_status',
                timestamp: Date.now()
            }));
        }
    }

    /**
     * Handle WebSocket messages
     */
    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'server_status':
                this.handleServerStatus(data);
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
                console.log('Unknown message type:', data.type);
        }
    }

    /**
     * Handle server status response
     */
    handleServerStatus(data) {
        this.servers = data.servers || [];
        this.isDataLoaded = true;
        
        // Hide loading state
        if (this.loadingUI) {
            this.loadingUI.hideLoadingState();
        }
        
        // Update UI with server data
        this.updateServerList();
        this.updateStatistics();
        this.updateCharts();
    }

    /**
     * Handle test results
     */
    handleTestResult(data) {
        const { testType, result } = data;
        
        switch (testType) {
            case 'traceroute':
                this.testResults.traceroute.push(result);
                this.updateTracerouteResults(this.testResults.traceroute);
                break;
            case 'ping':
                this.testResults.ping.push(result);
                this.updatePingResults(this.testResults.ping);
                break;
            case 'dns':
                this.testResults.dns.push(result);
                this.updateDnsResults(this.testResults.dns);
                break;
            case 'bandwidth':
                this.testResults.bandwidth.push(result);
                this.updateBandwidthResults(this.testResults.bandwidth);
                break;
        }
    }

    /**
     * Handle test progress updates
     */
    handleTestUpdate(data) {
        const { testType, status, progress } = data;
        
        switch (testType) {
            case 'traceroute':
                this.updateTracerouteStatus(status, progress.currentHop, progress.totalHops, progress.elapsedTime);
                break;
            case 'ping':
                this.updatePingStatus(status, progress);
                break;
            case 'bandwidth':
                this.updateBandwidthStatus(status, progress);
                break;
        }
    }

    /**
     * Handle WebSocket connection error
     */
    handleConnectionError() {
        if (this.loadingUI) {
            this.loadingUI.hideLoadingState();
        }
        // Show empty state instead of error message
        if (this.loadingUI) {
            this.loadingUI.showServersEmpty();
        }
    }

    /**
     * Handle WebSocket connection close
     */
    handleConnectionClose() {
        // Show empty state instead of error message
        if (this.loadingUI) {
            this.loadingUI.showServersEmpty();
        }
    }

    /**
     * Handle general errors
     */
    handleError(data) {
        // Log error to console but don't show to user
        console.error('[NETWORK-BENCHMARK-RUNTIME-UI] Error:', data.message || 'An error occurred');
    }

    /**
     * Show error message to user (disabled for better UX)
     */
    showErrorMessage(message) {
        // Error messages are now suppressed to improve user experience
        // Log to console instead for debugging
        console.error('[NETWORK-BENCHMARK-RUNTIME-UI] Suppressed error:', message);
    }

    /**
     * Attach event listeners
     */
    attachEventListeners() {
        // Traceroute popup events
        this.attachTracerouteEvents();
        
        // Ping test popup events
        this.attachPingTestEvents();
        
        // DNS lookup popup events
        this.attachDnsLookupEvents();
        
        // Bandwidth test popup events
        this.attachBandwidthTestEvents();
    }

    /**
     * Attach traceroute popup events
     */
    attachTracerouteEvents() {
        // Start button
        document.getElementById('start-traceroute-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.startTraceroute();
            }
        });
        
        // Stop button
        document.getElementById('stop-traceroute-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.stopTraceroute();
            }
        });
        
        // Reset button
        document.getElementById('reset-traceroute-btn')?.addEventListener('click', () => {
            this.resetTracerouteForm();
        });
    }

    /**
     * Attach ping test popup events
     */
    attachPingTestEvents() {
        // Start button
        document.getElementById('start-ping-test-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.startPingTest();
            }
        });
        
        // Stop button
        document.getElementById('stop-ping-test-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.stopPingTest();
            }
        });
    }

    /**
     * Attach DNS lookup popup events
     */
    attachDnsLookupEvents() {
        // Lookup button
        document.getElementById('lookup-dns-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.performDnsLookup();
            }
        });
        
        // Clear button
        document.getElementById('clear-dns-btn')?.addEventListener('click', () => {
            this.clearDnsResults();
        });
    }

    /**
     * Attach bandwidth test popup events
     */
    attachBandwidthTestEvents() {
        // Start button
        document.getElementById('start-bandwidth-test-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.startBandwidthTest();
            }
        });
        
        // Stop button
        document.getElementById('stop-bandwidth-test-btn')?.addEventListener('click', () => {
            if (window.networkBenchmark) {
                window.networkBenchmark.stopBandwidthTest();
            }
        });
    }

    /**
     * Initialize popup content (called by loading UI)
     */
    initializePopupContent(popupId, popupElement) {
        switch (popupId) {
            case 'traceroute':
                this.initializeTraceroutePopup(popupElement);
                break;
            case 'pingTest':
                this.initializePingTestPopup(popupElement);
                break;
            case 'dnsLookup':
                this.initializeDnsLookupPopup(popupElement);
                break;
            case 'bandwidthTest':
                this.initializeBandwidthTestPopup(popupElement);
                break;
        }
    }

    /**
     * Initialize traceroute popup with full content
     */
    initializeTraceroutePopup(popupElement) {
        const mainContent = popupElement.querySelector('main');
        if (mainContent) {
            mainContent.innerHTML = `
                <div class="space-y-8">
                    <!-- Test Configuration -->
                    <section class="space-y-6">
                        <h3 class="text-lg text-neutral-800 mb-4">Test Configuration</h3>
                        
                        <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
                            <!-- Left Column - Form Inputs -->
                            <div class="space-y-4">
                                <div class="space-y-2">
                                    <label class="block text-sm text-neutral-700">Target Host</label>
                                    <input type="text" id="traceroute-target-host" placeholder="Enter IP address or hostname" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-400">
                                </div>

                                <div class="space-y-2">
                                    <label class="block text-sm text-neutral-700">Maximum Hops</label>
                                    <input type="number" id="traceroute-max-hops" value="30" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-400">
                                </div>

                                <div class="space-y-2">
                                    <label class="block text-sm text-neutral-700">Timeout (seconds)</label>
                                    <input type="number" id="traceroute-timeout" value="5" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-400">
                                </div>

                                <div class="space-y-3">
                                    <label class="block text-sm text-neutral-700">Protocol</label>
                                    <div class="space-y-2">
                                        <label class="flex items-center">
                                            <input type="radio" name="traceroute-protocol" value="icmp" checked class="mr-2 text-neutral-600">
                                            <span class="text-sm">ICMP</span>
                                        </label>
                                        <label class="flex items-center">
                                            <input type="radio" name="traceroute-protocol" value="udp" class="mr-2 text-neutral-600">
                                            <span class="text-sm">UDP</span>
                                        </label>
                                        <label class="flex items-center">
                                            <input type="radio" name="traceroute-protocol" value="tcp" class="mr-2 text-neutral-600">
                                            <span class="text-sm">TCP</span>
                                        </label>
                                    </div>
                                </div>
                            </div>

                            <!-- Right Column - Status and Buttons -->
                            <div class="space-y-4">
                                <div class="space-y-4">
                                    <button id="start-traceroute-btn" class="w-full flex items-center justify-center gap-2 bg-neutral-900 text-white px-6 py-3 rounded-md hover:bg-neutral-800 transition-colors">
                                        <i class="fa-solid fa-play"></i>
                                        <span>Start Traceroute</span>
                                    </button>
                                    <button id="stop-traceroute-btn" class="flex items-center gap-2 bg-neutral-600 text-white px-6 py-2 rounded-md hover:bg-neutral-700 transition-colors" style="display: none;">
                                        <i class="fa-solid fa-stop text-sm"></i>
                                        <span>Stop Test</span>
                                    </button>
                                    <button id="reset-traceroute-btn" class="w-full flex items-center justify-center gap-2 bg-white text-neutral-700 px-6 py-2 rounded-md border border-neutral-300 hover:bg-neutral-50 transition-colors">
                                        <i class="fa-solid fa-rotate-right text-sm"></i>
                                        <span>Reset</span>
                                    </button>
                                </div>

                                <!-- Test Status -->
                                <div id="traceroute-status-section" class="hidden">
                                    <div class="bg-neutral-50 rounded-lg p-4 border border-neutral-200">
                                        <div class="flex items-center justify-between mb-3">
                                            <span class="text-sm text-neutral-600">Status:</span>
                                            <span id="traceroute-status-text" class="text-sm font-medium text-neutral-900">Ready</span>
                                        </div>
                                        <div class="flex items-center justify-between mb-2">
                                            <span class="text-sm text-neutral-600">Progress:</span>
                                            <span id="traceroute-current-hop" class="text-sm text-neutral-700">0 of 30</span>
                                        </div>
                                        <div class="flex items-center justify-between mb-3">
                                            <span class="text-sm text-neutral-600">Elapsed Time:</span>
                                            <span id="traceroute-elapsed-time" class="text-sm text-neutral-700">00:00</span>
                                        </div>
                                        <div class="w-full bg-neutral-200 rounded-full h-2">
                                            <div id="traceroute-progress-bar" class="bg-blue-600 h-2 rounded-full transition-all duration-300" style="width: 0%"></div>
                                        </div>
                                        <div class="text-center mt-1">
                                            <span id="traceroute-progress-text" class="text-xs text-neutral-600">0% Complete</span>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </section>

                    <!-- Traceroute Results -->
                    <section id="traceroute-results" class="mt-8" style="display: none;">
                        <h3 class="text-lg text-neutral-800 mb-4">Traceroute Results</h3>
                        <div class="border border-neutral-300 rounded-lg overflow-hidden">
                            <div class="overflow-x-auto">
                                <table class="w-full">
                                    <thead class="bg-neutral-100 sticky top-0 z-10">
                                        <tr>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">Hop</th>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">IP Address</th>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">Hostname</th>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">RTT 1</th>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">RTT 2</th>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">RTT 3</th>
                                            <th class="px-4 py-3 text-left text-sm text-neutral-700 border-b border-neutral-300">Avg RTT</th>
                                        </tr>
                                    </thead>
                                    <tbody id="traceroute-results-tbody" class="bg-white">
                                        <tr>
                                            <td colspan="7" class="py-8 text-center text-neutral-500">
                                                Click "Start Traceroute" to begin network path analysis
                                            </td>
                                        </tr>
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    </section>
                </div>
            `;
        }
        
        // Enable popup interactions
        if (this.loadingUI) {
            this.loadingUI.enablePopupInteractions('traceroute');
        }
        
        // Re-attach events
        this.attachTracerouteEvents();
    }

    /**
     * Initialize ping test popup with full content
     */
    initializePingTestPopup(popupElement) {
        // Implementation for ping test popup content
        // Similar to traceroute but with ping-specific fields
    }

    /**
     * Initialize DNS lookup popup with full content
     */
    initializeDnsLookupPopup(popupElement) {
        // Implementation for DNS lookup popup content
    }

    /**
     * Initialize bandwidth test popup with full content
     */
    initializeBandwidthTestPopup(popupElement) {
        // Implementation for bandwidth test popup content
    }

    /**
     * Update server list display
     */
    updateServerList() {
        const serverListContainer = document.querySelector('#network-benchmark-container .server-list');
        if (serverListContainer && this.servers.length > 0) {
            serverListContainer.innerHTML = this.servers.map(server => `
                <div class="server-item bg-white rounded-lg border border-neutral-200 p-4 mb-3">
                    <div class="flex items-center justify-between">
                        <div class="flex items-center gap-3">
                            <div class="w-10 h-10 bg-${server.status === 'online' ? 'green' : 'red'}-100 rounded-full flex items-center justify-center">
                                <i class="fa-solid fa-server text-${server.status === 'online' ? 'green' : 'red'}-600"></i>
                            </div>
                            <div>
                                <h4 class="font-medium text-neutral-900">${server.name}</h4>
                                <p class="text-sm text-neutral-600">${server.ip}</p>
                            </div>
                        </div>
                        <div class="text-right">
                            <div class="text-sm font-medium text-${server.status === 'online' ? 'green' : 'red'}-600">
                                ${server.status === 'online' ? 'Online' : 'Offline'}
                            </div>
                            <div class="text-xs text-neutral-500">
                                ${server.ping ? `${server.ping}ms` : 'N/A'}
                            </div>
                        </div>
                    </div>
                </div>
            `).join('');
        } else if (serverListContainer) {
            if (this.loadingUI) {
                this.loadingUI.showServersEmpty();
            }
        }
    }

    /**
     * Update statistics display
     */
    updateStatistics() {
        const statsContainer = document.querySelector('#network-benchmark-container .stats-container');
        if (statsContainer) {
            const stats = {
                total: this.servers.length,
                online: this.servers.filter(s => s.status === 'online').length,
                offline: this.servers.filter(s => s.status === 'offline').length,
                avgPing: this.calculateAveragePing()
            };

            statsContainer.innerHTML = `
                <div class="grid grid-cols-1 md:grid-cols-4 gap-4">
                    <div class="bg-white rounded-lg border border-neutral-200 p-4">
                        <div class="text-2xl font-bold text-neutral-900">${stats.total}</div>
                        <div class="text-sm text-neutral-600">Total Servers</div>
                    </div>
                    <div class="bg-white rounded-lg border border-neutral-200 p-4">
                        <div class="text-2xl font-bold text-green-600">${stats.online}</div>
                        <div class="text-sm text-neutral-600">Online</div>
                    </div>
                    <div class="bg-white rounded-lg border border-neutral-200 p-4">
                        <div class="text-2xl font-bold text-red-600">${stats.offline}</div>
                        <div class="text-sm text-neutral-600">Offline</div>
                    </div>
                    <div class="bg-white rounded-lg border border-neutral-200 p-4">
                        <div class="text-2xl font-bold text-blue-600">${stats.avgPing}ms</div>
                        <div class="text-sm text-neutral-600">Avg Ping</div>
                    </div>
                </div>
            `;
        }
    }

    /**
     * Update charts display
     */
    updateCharts() {
        // Implementation for updating charts with server data
    }

    /**
     * Calculate average ping
     */
    calculateAveragePing() {
        const onlineServers = this.servers.filter(s => s.status === 'online' && s.ping);
        if (onlineServers.length === 0) return 0;
        
        const totalPing = onlineServers.reduce((sum, server) => sum + server.ping, 0);
        return Math.round(totalPing / onlineServers.length);
    }

    /**
     * Update traceroute results
     */
    updateTracerouteResults(results) {
        const tbody = document.getElementById('traceroute-results-tbody');
        if (tbody && results.length > 0) {
            // Show results section
            const resultsSection = document.getElementById('traceroute-results');
            if (resultsSection) {
                resultsSection.style.display = 'block';
            }
            
            // Display only the most recent 10 results
            const recentResults = results.slice(-10).reverse();
            
            tbody.innerHTML = recentResults.map(result => `
                <tr>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.hop}</td>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.ip}</td>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.hostname}</td>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.rtt1}</td>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.rtt2}</td>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.rtt3}</td>
                    <td class="px-4 py-3 text-sm text-neutral-900">${result.avgRtt}</td>
                </tr>
            `).join('');
        }
    }

    /**
     * Update traceroute status
     */
    updateTracerouteStatus(status, currentHop = 0, totalHops = 30, elapsedTime = '00:00') {
        // Show status section
        const statusSection = document.getElementById('traceroute-status-section');
        if (statusSection) {
            statusSection.classList.remove('hidden');
        }
        
        // Update status text
        const statusElement = document.getElementById('traceroute-status-text');
        if (statusElement) statusElement.textContent = status;
        
        // Update progress
        const currentHopElement = document.getElementById('traceroute-current-hop');
        if (currentHopElement) currentHopElement.textContent = `${currentHop} of ${totalHops}`;
        
        // Update elapsed time
        const elapsedTimeElement = document.getElementById('traceroute-elapsed-time');
        if (elapsedTimeElement) elapsedTimeElement.textContent = elapsedTime;
        
        // Update progress bar
        const progressBar = document.getElementById('traceroute-progress-bar');
        const progressText = document.getElementById('traceroute-progress-text');
        
        if (progressBar && progressText) {
            const progress = (currentHop / totalHops) * 100;
            progressBar.style.width = `${progress}%`;
            progressText.textContent = `${Math.round(progress)}% Complete`;
        }
    }

    /**
     * Reset traceroute form
     */
    resetTracerouteForm() {
        // Clear form fields
        const targetHost = document.getElementById('traceroute-target-host');
        if (targetHost) targetHost.value = '';
        
        const maxHops = document.getElementById('traceroute-max-hops');
        if (maxHops) maxHops.value = '30';
        
        const timeout = document.getElementById('traceroute-timeout');
        if (timeout) timeout.value = '5';
        
        const protocolICMP = document.querySelector('input[name="traceroute-protocol"][value="icmp"]');
        if (protocolICMP) protocolICMP.checked = true;
        
        // Reset status
        const statusText = document.getElementById('traceroute-status-text');
        if (statusText) statusText.textContent = 'Ready';
        
        const currentHop = document.getElementById('traceroute-current-hop');
        if (currentHop) currentHop.textContent = '0 of 30';
        
        const elapsedTime = document.getElementById('traceroute-elapsed-time');
        if (elapsedTime) elapsedTime.textContent = '00:00';
        
        const progressBar = document.getElementById('traceroute-progress-bar');
        if (progressBar) progressBar.style.width = '0%';
        
        const progressText = document.getElementById('traceroute-progress-text');
        if (progressText) progressText.textContent = '0% Complete';
        
        // Hide results
        const resultsSection = document.getElementById('traceroute-results');
        if (resultsSection) resultsSection.style.display = 'none';
        
        // Clear results table
        const tbody = document.getElementById('traceroute-results-tbody');
        if (tbody) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="7" class="py-8 text-center text-neutral-500">
                        Click "Start Traceroute" to begin network path analysis
                    </td>
                </tr>
            `;
        }
        
        // Reset buttons
        const startBtn = document.getElementById('start-traceroute-btn');
        if (startBtn) startBtn.style.display = 'flex';
        
        const stopBtn = document.getElementById('stop-traceroute-btn');
        if (stopBtn) stopBtn.style.display = 'none';
        
        // Hide status section
        const statusSection = document.getElementById('traceroute-status-section');
        if (statusSection) statusSection.classList.add('hidden');
    }

    /**
     * Update ping results
     */
    updatePingResults(results) {
        // Implementation for updating ping results
    }

    /**
     * Update DNS results
     */
    updateDnsResults(results) {
        // Implementation for updating DNS results
    }

    /**
     * Update bandwidth results
     */
    updateBandwidthResults(results) {
        // Implementation for updating bandwidth results
    }

    /**
     * Clear DNS results
     */
    clearDnsResults() {
        // Implementation for clearing DNS results
    }

    /**
     * Update ping status
     */
    updatePingStatus(status, progress) {
        // Implementation for updating ping status
    }

    /**
     * Update bandwidth status
     */
    updateBandwidthStatus(status, progress) {
        // Implementation for updating bandwidth status
    }

    /**
     * Destroy the runtime UI
     */
    destroy() {
        // Close WebSocket connection
        if (this.websocket) {
            this.websocket.close();
            this.websocket = null;
        }
        
        // Clear data
        this.servers = [];
        this.testResults = {
            traceroute: [],
            ping: [],
            dns: [],
            bandwidth: []
        };
        
        // Reset state
        this.isDataLoaded = false;
        this.isInitialized = false;
    }
}

// Initialize the runtime UI
window.networkBenchmarkRuntimeUI = new NetworkBenchmarkRuntimeUI();

/**
 * Network Benchmark UI Component
 * Handles the main UI structure and interactions for the network benchmark tab
 */

class NetworkBenchmarkUI {
    constructor() {
        this.container = null;
        this.sections = {
            serverStatus: null,
            diagnosticTools: null
        };
        this.popups = {
            traceroute: null,
            pingTest: null,
            dnsLookup: null,
            bandwidthTest: null
        };
        this.loadingUI = null;
        this.runtimeUI = null;
        this.isInitialized = false;
        this.isLoading = false;
        this.lastUpdateTime = null;
        this.isConnected = false;
        this.websocket = null;
        this.connectionTimeout = null;
    }

    /**
     * Initialize the network benchmark UI
     */
    async initialize(containerId = 'network-benchmark-container') {
        this.container = document.getElementById(containerId);
        if (!this.container) {
            console.warn(`[NETWORK-BENCHMARK-UI] Container element not found: ${containerId}. Will retry when available.`);
            return false;
        }

        // Initialize loading UI first
        if (window.networkBenchmarkLoadingUI) {
            this.loadingUI = window.networkBenchmarkLoadingUI;
            this.loadingUI.initialize();
        }

        // Initialize runtime UI
        if (window.networkBenchmarkRuntimeUI) {
            this.runtimeUI = window.networkBenchmarkRuntimeUI;
            this.runtimeUI.initialize(this.loadingUI);
        }

        // Show initial loading state
        this.showInitialLoadingState();

        // Render main UI
        this.render();
        
        // Initialize WebSocket connection
        this.initializeWebSocket();
        
        // Attach event listeners
        this.attachEventListeners();
        
        this.isInitialized = true;
        console.log('[NETWORK-BENCHMARK-UI] UI initialized successfully');
        return true;
    }

    /**
     * Initialize WebSocket connection
     */
    initializeWebSocket() {
        try {
            // Create WebSocket connection for server status
            const wsUrl = `ws://${window.location.host}/ws/network-benchmark`;
            this.websocket = new WebSocket(wsUrl);

            this.websocket.onopen = () => {
                console.log('[NETWORK-BENCHMARK-UI] WebSocket connected');
                this.isConnected = true;
                this.updateConnectionStatus(true);
                this.requestServerStatus();
            };

            this.websocket.onmessage = (event) => {
                const data = JSON.parse(event.data);
                this.handleWebSocketMessage(data);
            };

            this.websocket.onerror = (error) => {
                console.error('[NETWORK-BENCHMARK-UI] WebSocket error:', error);
                this.handleConnectionError();
            };

            this.websocket.onclose = () => {
                console.log('[NETWORK-BENCHMARK-UI] WebSocket disconnected');
                this.isConnected = false;
                this.updateConnectionStatus(false);
                this.handleConnectionClose();
            };

        } catch (error) {
            console.error('[NETWORK-BENCHMARK-UI] Failed to initialize WebSocket:', error);
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
        console.log('[NETWORK-BENCHMARK-UI] Handling WebSocket message:', data);
        
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
                console.log('[NETWORK-BENCHMARK-UI] Unknown message type:', data.type);
        }
    }

    /**
     * Handle server status response
     */
    handleServerStatus(data) {
        console.log('[NETWORK-BENCHMARK-UI] Received server status:', data);
        
        // Hide loading state
        if (this.loadingUI) {
            this.loadingUI.hideLoadingState();
        }
        
        // Update UI with server data
        this.updateServerStats(data.stats || {});
        this.updateServerTable(data.servers || []);
        
        // Update last update time
        this.lastUpdateTime = data.timestamp || Date.now();
        this.updateSectionLastUpdate('server-status', this.lastUpdateTime);
        
        // Show connection indicator
        this.showConnectionIndicator();
        
        // Delegate to runtime UI for additional processing
        if (this.runtimeUI) {
            this.runtimeUI.handleServerStatus(data);
        }
    }

    /**
     * Handle test results
     */
    handleTestResult(data) {
        // Delegate to runtime UI
        if (this.runtimeUI) {
            this.runtimeUI.handleTestResult(data);
        }
    }

    /**
     * Handle test progress updates
     */
    handleTestUpdate(data) {
        // Delegate to runtime UI
        if (this.runtimeUI) {
            this.runtimeUI.handleTestUpdate(data);
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
        this.updateConnectionStatus(false);
    }

    /**
     * Handle WebSocket connection close
     */
    handleConnectionClose() {
        // Show empty state instead of error message
        if (this.loadingUI) {
            this.loadingUI.showServersEmpty();
        }
        this.updateConnectionStatus(false);
    }

    /**
     * Handle general errors
     */
    handleError(data) {
        // Log error to console but don't show to user
        console.error('[NETWORK-BENCHMARK-UI] Error:', data.message || 'An error occurred');
    }

    /**
     * Show error message to user (disabled for better UX)
     */
    showErrorMessage(message) {
        // Error messages are now suppressed to improve user experience
        // Log to console instead for debugging
        console.error('[NETWORK-BENCHMARK-UI] Suppressed error:', message);
    }

    /**
     * Render the main UI structure
     */
    render() {
        this.container.innerHTML = `
            <div class="max-w-7xl mx-auto px-6 py-8">
                <!-- Header -->
                <div class="mb-6">
                    <div class="flex items-center justify-between">
                        <div>
                            <h2 class="text-2xl text-neutral-900 mb-2">Network Utility & Diagnostics</h2>
                            <p class="text-neutral-600">Test network performance and diagnose connectivity issues</p>
                        </div>
                        <div class="flex items-center gap-4">
                            <div id="benchmark-connection-status" class="flex items-center space-x-1 text-xs text-neutral-400">
                                <div class="w-2 h-2 bg-red-500 rounded-full" id="benchmark-status-dot"></div>
                                <span id="benchmark-connection-text">Disconnected</span>
                            </div>
                            <i class="fa-solid fa-network-wired text-neutral-600 text-xl"></i>
                        </div>
                    </div>
                </div>

                <!-- Server Status Section -->
                ${this.renderServerStatusSection()}

                <!-- Diagnostic Tools Section -->
                ${this.renderDiagnosticToolsSection()}
            </div>
        `;

        // Store section references
        this.sections.serverStatus = document.getElementById('server-status-content');
        this.sections.diagnosticTools = document.getElementById('diagnostic-tools-content');
    }

    /**
     * Render server status section
     */
    renderServerStatusSection() {
        return `
            <section id="server-status" class="mb-8">
                <div class="bg-white rounded-lg shadow-sm border border-neutral-200">
                    <div class="p-6 cursor-pointer" onclick="networkBenchmarkUI.toggleSection('server-status-content', 'server-status-chevron')">
                        <div class="flex items-center justify-between">
                            <h2 class="text-2xl text-neutral-800 flex items-center">
                                <i class="fa-solid fa-server mr-3 text-neutral-600"></i>
                                Server Status & Information
                            </h2>
                            <i id="server-status-chevron" class="fa-solid fa-chevron-down text-neutral-600 transition-transform duration-300 rotate-180"></i>
                        </div>
                    </div>

                    <div id="server-status-content" class="hidden">
                        <div id="main-wrapper" class="w-full bg-white">
                            <!-- Action Bar -->
                            <div id="action-bar" class="bg-neutral-50 border-b border-neutral-300 px-6 py-4">
                                <div class="flex items-center justify-between">
                                    <div class="flex items-center gap-3">
                                        <button id="refresh-status-btn" class="px-4 py-2 bg-neutral-200 text-neutral-800 rounded hover:bg-neutral-300 flex items-center gap-2">
                                            <i class="fa-solid fa-rotate text-sm"></i>
                                            <span>Refresh Status</span>
                                        </button>
                                        <button id="test-all-servers-btn" class="px-4 py-2 bg-neutral-600 text-white rounded hover:bg-neutral-700 flex items-center gap-2">
                                            <i class="fa-solid fa-play text-sm"></i>
                                            <span>Test All Servers</span>
                                        </button>
                                    </div>
                                    <div class="flex items-center gap-2 text-sm text-neutral-600">
                                        <i class="fa-regular fa-clock"></i>
                                        <span id="last-updated">Last updated: --</span>
                                    </div>
                                </div>
                            </div>

                            <!-- Main Content -->
                            <main id="main-content" class="px-6 py-6">
                                <!-- Filters Section -->
                                <div id="filters-section" class="bg-white border border-neutral-300 rounded-lg mb-6">
                                    <button id="filters-toggle-btn" class="w-full px-6 py-4 flex items-center justify-between hover:bg-neutral-50" onclick="networkBenchmarkUI.toggleFilters()">
                                        <div class="flex items-center gap-2">
                                            <i class="fa-solid fa-filter text-neutral-700"></i>
                                            <span class="text-neutral-900">Filters</span>
                                        </div>
                                        <i id="filters-chevron" class="fa-solid fa-chevron-down text-neutral-600 transition-transform duration-300"></i>
                                    </button>
                                    
                                    <div id="filters-content" class="hidden px-6 pb-6 border-t border-neutral-200">
                                        <div class="grid grid-cols-4 gap-4 mt-4">
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Keyword</label>
                                                <input type="text" id="filter-keyword" placeholder="Search..." class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm">
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Continent</label>
                                                <select id="filter-continent" class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm text-neutral-600">
                                                    <option>All Continents</option>
                                                </select>
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Country</label>
                                                <select id="filter-country" class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm text-neutral-600">
                                                    <option>All Countries</option>
                                                </select>
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Site</label>
                                                <select id="filter-site" class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm text-neutral-600">
                                                    <option>All Sites</option>
                                                </select>
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Provider</label>
                                                <select id="filter-provider" class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm text-neutral-600">
                                                    <option>All Providers</option>
                                                </select>
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Host</label>
                                                <input type="text" id="filter-host" placeholder="Enter host..." class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm">
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Port</label>
                                                <input type="text" id="filter-port" placeholder="Enter port..." class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm">
                                            </div>
                                            <div>
                                                <label class="block text-sm text-neutral-700 mb-2">Min Speed (Mbps)</label>
                                                <input type="number" id="filter-speed" placeholder="0" class="w-full px-3 py-2 bg-neutral-50 border border-neutral-300 rounded text-sm">
                                            </div>
                                        </div>
                                        
                                        <div class="flex items-center gap-3 mt-6">
                                            <button id="reset-filters-btn" class="px-4 py-2 bg-neutral-200 text-neutral-800 rounded hover:bg-neutral-300">
                                                Reset
                                            </button>
                                            <button id="apply-filters-btn" class="px-4 py-2 bg-neutral-600 text-white rounded hover:bg-neutral-700">
                                                Apply Filters
                                            </button>
                                        </div>
                                    </div>
                                </div>

                                <!-- Summary Cards -->
                                <div id="summary-cards" class="grid grid-cols-4 gap-4 mb-6">
                                    <div class="bg-white border border-neutral-300 rounded-lg p-6">
                                        <div class="flex items-center justify-between mb-2">
                                            <span class="text-sm text-neutral-600">Total Servers</span>
                                            <i class="fa-solid fa-server text-neutral-400"></i>
                                        </div>
                                        <div class="text-3xl text-neutral-900" id="total-servers">0</div>
                                    </div>
                                    <div class="bg-white border border-neutral-300 rounded-lg p-6">
                                        <div class="flex items-center justify-between mb-2">
                                            <span class="text-sm text-neutral-600">Online</span>
                                            <i class="fa-solid fa-circle-check text-green-400"></i>
                                        </div>
                                        <div class="text-3xl text-neutral-900" id="online-servers">0</div>
                                    </div>
                                    <div class="bg-white border border-neutral-300 rounded-lg p-6">
                                        <div class="flex items-center justify-between mb-2">
                                            <span class="text-sm text-neutral-600">Offline</span>
                                            <i class="fa-solid fa-circle-xmark text-red-400"></i>
                                        </div>
                                        <div class="text-3xl text-neutral-900" id="offline-servers">0</div>
                                    </div>
                                    <div class="bg-white border border-neutral-300 rounded-lg p-6">
                                        <div class="flex items-center justify-between mb-2">
                                            <span class="text-sm text-neutral-600">Avg Ping</span>
                                            <i class="fa-solid fa-clock text-neutral-400"></i>
                                        </div>
                                        <div class="text-3xl text-neutral-900" id="avg-ping">--ms</div>
                                    </div>
                                </div>

                                <!-- Server List Table -->
                                <div id="server-table-section" class="bg-white border border-neutral-300 rounded-lg">
                                    <div class="px-6 py-4 border-b border-neutral-200 flex items-center justify-between">
                                        <h2 class="text-lg text-neutral-900">Server List</h2>
                                        <div class="flex items-center gap-2" id="testing-status" style="display: none;">
                                            <div class="w-4 h-4 border-2 border-neutral-600 border-t-transparent rounded-full animate-spin"></div>
                                            <span class="text-sm text-neutral-600">Testing servers...</span>
                                        </div>
                                    </div>

                                    <div class="overflow-x-auto">
                                        <table class="w-full">
                                            <thead>
                                                <tr class="bg-neutral-50 border-b border-neutral-200">
                                                    <th class="px-6 py-3 text-left text-xs text-neutral-700 uppercase tracking-wider">Server Name</th>
                                                    <th class="px-6 py-3 text-left text-xs text-neutral-700 uppercase tracking-wider">Region</th>
                                                    <th class="px-6 py-3 text-left text-xs text-neutral-700 uppercase tracking-wider">Host / IP</th>
                                                    <th class="px-6 py-3 text-left text-xs text-neutral-700 uppercase tracking-wider">Ports</th>
                                                    <th class="px-6 py-3 text-left text-xs text-neutral-700 uppercase tracking-wider">Current Status</th>
                                                    <th class="px-6 py-3 text-left text-xs text-neutral-700 uppercase tracking-wider">Actions</th>
                                                </tr>
                                            </thead>
                                            <tbody id="server-table-body">
                                                <tr>
                                                    <td colspan="6" class="px-6 py-8 text-center text-neutral-500">
                                                        No servers available. Click "Refresh Status" to load servers.
                                                    </td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class="px-6 py-4 border-t border-neutral-200 flex items-center justify-between" id="pagination" style="display: none;">
                                        <div class="text-sm text-neutral-600" id="pagination-info">Showing 0 to 0 of 0 servers</div>
                                        <div class="flex items-center gap-2" id="pagination-controls">
                                            <!-- Pagination buttons will be inserted here -->
                                        </div>
                                    </div>
                                </div>
                            </main>
                        </div>
                    </div>
                </div>
            </section>
        `;
    }

    /**
     * Render diagnostic tools section
     */
    renderDiagnosticToolsSection() {
        return `
            <section id="diagnostic-tools">
                <div class="bg-white rounded-lg shadow-sm border border-neutral-200">
                    <div class="p-6 cursor-pointer" onclick="networkBenchmarkUI.toggleSection('diagnostic-tools-content', 'diagnostic-tools-chevron')">
                        <div class="flex items-center justify-between">
                            <h2 class="text-2xl text-neutral-800 flex items-center">
                                <i class="fa-solid fa-tools mr-3 text-neutral-600"></i>
                                Network Diagnostic Tools
                            </h2>
                            <i id="diagnostic-tools-chevron" class="fa-solid fa-chevron-down text-neutral-600 transition-transform duration-300"></i>
                        </div>
                    </div>

                    <div id="diagnostic-tools-content" class="">
                        <div class="px-6 pb-6">
                            <div class="grid grid-cols-1 lg:grid-cols-3 gap-6">
                                <div class="space-y-4">
                                    <h3 class="text-lg text-neutral-700">Network Tests</h3>

                                    <button id="traceroute-btn" class="w-full bg-neutral-50 hover:bg-neutral-100 text-neutral-700 border border-neutral-200 px-4 py-3 rounded-md text-left flex items-center justify-between transition-colors">
                                        <div class="flex items-center">
                                            <i class="fa-solid fa-network-wired mr-3 text-neutral-600"></i>
                                            <div>
                                                <div class="font-medium">Traceroute</div>
                                                <div class="text-sm text-neutral-500">Trace network path to destination</div>
                                            </div>
                                        </div>
                                        <i class="fa-solid fa-chevron-right text-neutral-400"></i>
                                    </button>

                                    <button id="ping-test-btn" class="w-full bg-neutral-50 hover:bg-neutral-100 text-neutral-700 border border-neutral-200 px-4 py-3 rounded-md text-left flex items-center justify-between transition-colors">
                                        <div class="flex items-center">
                                            <i class="fa-solid fa-wifi mr-3 text-neutral-600"></i>
                                            <div>
                                                <div class="font-medium">Ping Test</div>
                                                <div class="text-sm text-neutral-500">Test connectivity and latency</div>
                                            </div>
                                        </div>
                                        <i class="fa-solid fa-chevron-right text-neutral-400"></i>
                                    </button>

                                    <button id="dns-lookup-btn" class="w-full bg-neutral-50 hover:bg-neutral-100 text-neutral-700 border border-neutral-200 px-4 py-3 rounded-md text-left flex items-center justify-between transition-colors">
                                        <div class="flex items-center">
                                            <i class="fa-solid fa-globe mr-3 text-neutral-600"></i>
                                            <div>
                                                <div class="font-medium">DNS Lookup</div>
                                                <div class="text-sm text-neutral-500">Resolve domain names</div>
                                            </div>
                                        </div>
                                        <i class="fa-solid fa-chevron-right text-neutral-400"></i>
                                    </button>
                                </div>

                                <div class="space-y-4">
                                    <h3 class="text-lg text-neutral-700">Bandwidth Tests</h3>

                                    <button id="bandwidth-test-btn" class="w-full bg-neutral-50 hover:bg-neutral-100 text-neutral-700 border border-neutral-200 px-4 py-3 rounded-md text-left flex items-center justify-between transition-colors">
                                        <div class="flex items-center">
                                            <i class="fa-solid fa-tachometer-alt mr-3 text-neutral-600"></i>
                                            <div>
                                                <div class="font-medium">Bandwidth Test</div>
                                                <div class="text-sm text-neutral-500">Test upload/download speeds</div>
                                            </div>
                                        </div>
                                        <i class="fa-solid fa-chevron-right text-neutral-400"></i>
                                    </button>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </section>
        `;
    }

    /**
     * Toggle section visibility
     */
    toggleSection(contentId, chevronId) {
        const content = document.getElementById(contentId);
        const chevron = document.getElementById(chevronId);
        
        if (content && chevron) {
            content.classList.toggle('hidden');
            chevron.classList.toggle('rotate-180');
        }
    }

    /**
     * Toggle filters section
     */
    toggleFilters() {
        const content = document.getElementById('filters-content');
        const chevron = document.getElementById('filters-chevron');
        
        if (content && chevron) {
            content.classList.toggle('hidden');
            chevron.classList.toggle('rotate-180');
        }
    }

    /**
     * Attach event listeners
     */
    attachEventListeners() {
        // Server status buttons
        const refreshBtn = document.getElementById('refresh-status-btn');
        const testAllBtn = document.getElementById('test-all-servers-btn');
        
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.refreshServerStatus());
        }
        
        if (testAllBtn) {
            testAllBtn.addEventListener('click', () => this.testAllServers());
        }

        // Filter buttons
        const resetFiltersBtn = document.getElementById('reset-filters-btn');
        const applyFiltersBtn = document.getElementById('apply-filters-btn');
        
        if (resetFiltersBtn) {
            resetFiltersBtn.addEventListener('click', () => this.resetFilters());
        }
        
        if (applyFiltersBtn) {
            applyFiltersBtn.addEventListener('click', () => this.applyFilters());
        }

        // Diagnostic tool buttons
        const tracerouteBtn = document.getElementById('traceroute-btn');
        const pingTestBtn = document.getElementById('ping-test-btn');
        const dnsLookupBtn = document.getElementById('dns-lookup-btn');
        const bandwidthTestBtn = document.getElementById('bandwidth-test-btn');
        
        if (tracerouteBtn) {
            tracerouteBtn.addEventListener('click', () => this.openTraceroutePopup());
        }
        
        if (pingTestBtn) {
            pingTestBtn.addEventListener('click', () => this.openPingTestPopup());
        }
        
        if (dnsLookupBtn) {
            dnsLookupBtn.addEventListener('click', () => this.openDnsLookupPopup());
        }
        
        if (bandwidthTestBtn) {
            bandwidthTestBtn.addEventListener('click', () => this.openBandwidthTestPopup());
        }
    }

    /**
     * Refresh server status
     */
    async refreshServerStatus() {
        if (window.networkBenchmark) {
            await window.networkBenchmark.refreshServerStatus();
        }
    }

    /**
     * Test all servers
     */
    async testAllServers() {
        if (window.networkBenchmark) {
            await window.networkBenchmark.testAllServers();
        }
    }

    /**
     * Reset filters
     */
    resetFilters() {
        const filterInputs = [
            'filter-keyword', 'filter-continent', 'filter-country',
            'filter-site', 'filter-provider', 'filter-host', 'filter-port', 'filter-speed'
        ];
        
        filterInputs.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.value = element.tagName === 'SELECT' ? element.options[0].value : '';
            }
        });
        
        if (window.networkBenchmark) {
            window.networkBenchmark.applyFilters();
        }
    }

    /**
     * Apply filters
     */
    applyFilters() {
        const filters = {
            keyword: document.getElementById('filter-keyword')?.value || '',
            continent: document.getElementById('filter-continent')?.value || '',
            country: document.getElementById('filter-country')?.value || '',
            site: document.getElementById('filter-site')?.value || '',
            provider: document.getElementById('filter-provider')?.value || '',
            host: document.getElementById('filter-host')?.value || '',
            port: document.getElementById('filter-port')?.value || '',
            minSpeed: parseInt(document.getElementById('filter-speed')?.value) || 0
        };
        
        if (window.networkBenchmark) {
            window.networkBenchmark.applyFilters(filters);
        }
    }

    /**
     * Open traceroute popup
     */
    openTraceroutePopup() {
        if (this.loadingUI) {
            // Show popup with loading state
            this.loadingUI.showTraceroutePopup();
            
            // Transfer popup content to runtime UI after a short delay
            setTimeout(() => {
                if (this.runtimeUI) {
                    this.loadingUI.transferPopupToRuntime('traceroute', this.runtimeUI);
                }
            }, 500);
        }
    }

    /**
     * Open ping test popup
     */
    openPingTestPopup() {
        if (this.loadingUI) {
            // Show popup with loading state
            this.loadingUI.showPingTestPopup();
            
            // Transfer popup content to runtime UI after a short delay
            setTimeout(() => {
                if (this.runtimeUI) {
                    this.loadingUI.transferPopupToRuntime('pingTest', this.runtimeUI);
                }
            }, 500);
        }
    }

    /**
     * Open DNS lookup popup
     */
    openDnsLookupPopup() {
        if (this.loadingUI) {
            // Show popup with loading state
            this.loadingUI.showDnsLookupPopup();
            
            // Transfer popup content to runtime UI after a short delay
            setTimeout(() => {
                if (this.runtimeUI) {
                    this.loadingUI.transferPopupToRuntime('dnsLookup', this.runtimeUI);
                }
            }, 500);
        }
    }

    /**
     * Open bandwidth test popup
     */
    openBandwidthTestPopup() {
        if (this.loadingUI) {
            // Show popup with loading state
            this.loadingUI.showBandwidthTestPopup();
            
            // Transfer popup content to runtime UI after a short delay
            setTimeout(() => {
                if (this.runtimeUI) {
                    this.loadingUI.transferPopupToRuntime('bandwidthTest', this.runtimeUI);
                }
            }, 500);
        }
    }

    /**
     * Update server statistics
     */
    updateServerStats(stats) {
        // Use loading UI if available, otherwise fallback to direct updates
        if (window.networkBenchmarkLoadingUI && window.networkBenchmarkLoadingUI.isInitialized) {
            window.networkBenchmarkLoadingUI.updateServerStats(stats);
        } else {
            // Fallback to direct updates
            const totalElement = document.getElementById('total-servers');
            if (totalElement) totalElement.textContent = stats.total || 0;
            
            const onlineElement = document.getElementById('online-servers');
            if (onlineElement) onlineElement.textContent = stats.online || 0;
            
            const offlineElement = document.getElementById('offline-servers');
            if (offlineElement) offlineElement.textContent = stats.offline || 0;
            
            const avgPingElement = document.getElementById('avg-ping');
            if (avgPingElement) avgPingElement.textContent = stats.avgPing ? `${stats.avgPing}ms` : '--ms';
            
            const lastUpdatedElement = document.getElementById('last-updated');
            if (lastUpdatedElement) lastUpdatedElement.textContent = `Last updated: ${stats.lastUpdated || '--'}`;
        }
    }

    /**
     * Update server table
     */
    updateServerTable(servers) {
        const tbody = document.getElementById('server-table-body');
        
        if (!tbody) {
            console.warn('[NETWORK-BENCHMARK-UI] Server table body not found');
            return;
        }
        
        if (!servers || servers.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="6" class="px-6 py-8 text-center text-neutral-500">
                        No servers found matching the current filters.
                    </td>
                </tr>
            `;
            const pagination = document.getElementById('pagination');
            if (pagination) pagination.style.display = 'none';
            return;
        }

        tbody.innerHTML = servers.map(server => `
            <tr class="border-b border-neutral-200 hover:bg-neutral-50">
                <td class="px-6 py-4">
                    <div class="flex items-center gap-2">
                        <i class="fa-solid fa-server text-neutral-600"></i>
                        <span class="text-neutral-900">${server.name}</span>
                    </div>
                </td>
                <td class="px-6 py-4 text-sm text-neutral-700">${server.region}</td>
                <td class="px-6 py-4 text-sm text-neutral-700">${server.host}</td>
                <td class="px-6 py-4 text-sm text-neutral-700">${server.ports}</td>
                <td class="px-6 py-4">
                    <span class="px-3 py-1 ${this.getStatusClass(server.status)} text-white text-xs rounded">
                        ${server.status.toUpperCase()}
                    </span>
                </td>
                <td class="px-6 py-4">
                    <div class="flex items-center gap-2">
                        <button class="px-3 py-1 bg-neutral-600 text-white text-xs rounded hover:bg-neutral-700" 
                                onclick="networkBenchmark.testServer('${server.id}')">
                            Start Test
                        </button>
                        <button class="px-3 py-1 bg-neutral-200 text-neutral-700 text-xs rounded hover:bg-neutral-300"
                                onclick="networkBenchmark.viewServerConfig('${server.id}')">
                            View Config
                        </button>
                    </div>
                </td>
            </tr>
        `).join('');
    }

    /**
     * Get status CSS class
     */
    getStatusClass(status) {
        const statusClasses = {
            'excellent': 'bg-green-600',
            'good': 'bg-blue-600',
            'fair': 'bg-yellow-600',
            'poor': 'bg-orange-600',
            'unreachable': 'bg-red-600'
        };
        return statusClasses[status?.toLowerCase()] || 'bg-neutral-600';
    }

    /**
     * Show testing status
     */
    showTestingStatus(isTesting, current = 0, total = 0) {
        const statusElement = document.getElementById('testing-status');
        if (statusElement) {
            if (isTesting) {
                statusElement.style.display = 'flex';
                statusElement.querySelector('span').textContent = `Testing servers... ${current}/${total}`;
            } else {
                statusElement.style.display = 'none';
            }
        }
    }

    /**
     * Update connection status in UI
     */
    updateConnectionStatus(connected) {
        this.isConnected = connected;
        
        // Update connection status element
        const statusDot = document.getElementById('benchmark-status-dot');
        const statusText = document.getElementById('benchmark-connection-text');
        
        if (statusDot && statusText) {
            if (connected) {
                // Use setAttribute to avoid read-only property issues
                try {
                    statusDot.setAttribute('class', 'w-2 h-2 bg-green-500 rounded-full');
                } catch (error) {
                    console.warn('[NETWORK-BENCHMARK-UI] Could not set status dot class:', error);
                }
                statusText.textContent = 'Connected';
                this.lastUpdateTime = Date.now();
                
                // Clear any existing timeout
                if (this.connectionTimeout) {
                    clearTimeout(this.connectionTimeout);
                    this.connectionTimeout = null;
                }
                
                // Set timeout to check for connection staleness
                this.connectionTimeout = setTimeout(() => {
                    this.checkConnectionStaleness();
                }, 10000); // Check after 10 seconds of no data
            } else {
                // Use setAttribute to avoid read-only property issues
                try {
                    statusDot.setAttribute('class', 'w-2 h-2 bg-red-500 rounded-full');
                } catch (error) {
                    console.warn('[NETWORK-BENCHMARK-UI] Could not set status dot class:', error);
                }
                statusText.textContent = 'Disconnected';
                
                // Clear timeout when disconnected
                if (this.connectionTimeout) {
                    clearTimeout(this.connectionTimeout);
                    this.connectionTimeout = null;
                }
            }
        }
    }

    /**
     * Check connection staleness
     */
    checkConnectionStaleness() {
        if (!this.isConnected) return;
        
        const now = Date.now();
        const timeSinceLastData = now - (this.lastUpdateTime || 0);
        
        if (timeSinceLastData > 15000) { // 15 seconds without data
            console.log('[NETWORK-BENCHMARK-UI] Connection appears stale, marking as disconnected');
            this.updateConnectionStatus(false);
        } else {
            // Still receiving data, reset the timeout
            this.connectionTimeout = setTimeout(() => {
                this.checkConnectionStaleness();
            }, 10000);
        }
    }

    /**
     * Show connection indicator briefly
     */
    showConnectionIndicator() {
        const indicator = document.getElementById('benchmark-connection-indicator');
        if (indicator) {
            indicator.style.display = 'flex';
            setTimeout(() => {
                indicator.style.display = 'none';
            }, 2000);
        }
    }

    /**
     * Update last update time for a section
     */
    updateSectionLastUpdate(sectionId, timestamp) {
        const element = document.querySelector(`[data-last-update="${sectionId}"]`);
        if (element) {
            if (timestamp) {
                const date = new Date(timestamp);
                element.textContent = `Last updated: ${date.toLocaleString()}`;
            } else {
                element.textContent = `Last updated: ${new Date().toLocaleString()}`;
            }
        }
    }

    /**
     * Show notification for status changes
     */
    showNotification(message, type = 'info') {
        console.log(`[NETWORK-BENCHMARK-UI] Notification (${type}): ${message}`);
        
        // If popup manager is available, use it
        if (window.popupManager) {
            window.popupManager.showNotification(message, type);
        } else {
            // Fallback to console
            console.log(message);
        }
    }

    /**
     * Handle WebSocket data update
     */
    handleWebSocketUpdate(data) {
        console.log('[NETWORK-BENCHMARK-UI] Handling WebSocket update:', data);
        
        // Update connection status
        this.updateConnectionStatus(true);
        
        // Show connection indicator
        this.showConnectionIndicator();
        
        // Update last update time
        this.updateSectionLastUpdate('server-status', data.lastUpdated);
        
        // Show notification for status changes if any
        if (data.statusChanges && data.statusChanges.length > 0) {
            data.statusChanges.forEach(change => {
                this.showNotification(`Server ${change.name} status changed to ${change.status}`, 'warning');
            });
        }
    }

    /**
     * Show loading state for servers
     */
    showServersLoading() {
        // Use loading UI if available, otherwise fallback to direct implementation
        if (window.networkBenchmarkLoadingUI && window.networkBenchmarkLoadingUI.isInitialized) {
            window.networkBenchmarkLoadingUI.showServersLoading();
        } else {
            // Fallback to direct implementation
            const tbody = document.getElementById('server-list-tbody');
            if (tbody) {
                tbody.innerHTML = `
                    <tr>
                        <td colspan="8" class="px-6 py-8 text-center">
                            <div class="flex items-center justify-center">
                                <div class="animate-spin rounded-full h-6 w-6 border-b-2 border-neutral-600 mr-3"></div>
                                <span class="text-neutral-500">Loading server data...</span>
                            </div>
                        </td>
                    </tr>
                `;
            }
        }
    }

    /**
     * Show empty state for servers
     */
    showServersEmpty() {
        // Use loading UI if available, otherwise fallback to direct implementation
        if (window.networkBenchmarkLoadingUI && window.networkBenchmarkLoadingUI.isInitialized) {
            window.networkBenchmarkLoadingUI.showServersEmpty();
        } else {
            // Fallback to direct implementation
            const tbody = document.getElementById('server-list-tbody');
            if (tbody) {
                tbody.innerHTML = `
                    <tr>
                        <td colspan="8" class="px-6 py-8 text-center">
                            <div class="text-center">
                                <i class="fa-solid fa-server text-4xl text-neutral-300 mb-4"></i>
                                <p class="text-neutral-500">No servers available</p>
                            </div>
                        </td>
                    </tr>
                `;
            }
        }
    }

    /**
     * Show initial loading state when no data is available
     */
    showInitialLoadingState() {
        // Use loading UI if available, otherwise fallback to direct implementation
        if (this.loadingUI) {
            this.loadingUI.showInitialLoadingState();
        } else {
            // Fallback to direct implementation
            console.log('[NETWORK-BENCHMARK-UI] Loading UI not available, using fallback loading states');
            this.showServersLoading();
            
            // Show loading stats
            const statsElements = ['total-servers', 'online-servers', 'offline-servers', 'avg-ping'];
            statsElements.forEach(id => {
                const element = document.getElementById(id);
                if (element) {
                    element.innerHTML = '<div class="animate-pulse bg-neutral-200 h-6 w-12 rounded"></div>';
                }
            });
            
            const lastUpdatedElement = document.getElementById('last-updated');
            if (lastUpdatedElement) {
                lastUpdatedElement.textContent = 'Loading...';
            }
        }
    }

    /**
     * Destroy the component
     */
    destroy() {
        // Close WebSocket connection
        if (this.websocket) {
            this.websocket.close();
            this.websocket = null;
        }
        
        // Clear connection timeout
        if (this.connectionTimeout) {
            clearTimeout(this.connectionTimeout);
            this.connectionTimeout = null;
        }
        
        // Destroy loading UI
        if (this.loadingUI) {
            this.loadingUI.destroy();
        }
        
        // Destroy runtime UI
        if (this.runtimeUI) {
            this.runtimeUI.destroy();
        }
        
        if (this.container) {
            this.container.innerHTML = '';
        }
        this.isInitialized = false;
        this.isConnected = false;
        this.isLoading = false;
        this.lastUpdateTime = null;
    }
}

// Initialize global instance
window.networkBenchmarkUI = new NetworkBenchmarkUI();

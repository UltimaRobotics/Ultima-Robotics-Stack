/**
 * Source Page UI Controller
 * Manages the dashboard UI, navigation, and user interactions
 * Creates the entire UI dynamically
 */

// Import dashboard modules
import './dashboard/dashboard-ui.js';
import './dashboard/dashboard.js';
import { DashboardLoadingUI } from './dashboard/dashboard-loading-ui.js';
import { NetworkPriorityManager, NetworkPriorityUI, DataLoadingUI } from './network-priority/index.js';

// Import network benchmark modules
import './network-benchmark/network-benchmark-ui.js';
import './network-benchmark/network-benchmark-loading-ui.js';
import './network-benchmark/network-benchmark.js';

// Import licence modules
import { LicenceUI } from './licence/licence-ui.js';

// Import firmware upgrade modules
import { initializeFirmwareUpgrade, cleanupFirmwareUpgrade } from './firmware-upgrade/index.js';

// Import backup & restore modules
import { initializeBackupRestore, cleanupBackupRestore } from './backup-restore/index.js';

// Import MAVLink extension modules
import { initializeMavlinkExtension, cleanupMavlinkExtension, getMavlinkExtensionContentHTML } from './mavlink-extension/index.js';

class SourceUI {
    constructor(sourceManager) {
        this.sourceManager = sourceManager;
        this.currentSection = 'system-dashboard';
        this.sidebarOpen = false;
        this.dashboard = null;
        this.activeTabContent = null;
        this.tabComponents = new Map(); // Store active components for each tab
        
        this.init();
    }
    
    init() {
        console.log('[SOURCE-UI] Initializing UI controller...');
        
        // Wait for source manager to be initialized
        if (this.sourceManager.isInitialized()) {
            this.setupUI();
        } else {
            document.addEventListener('sourceInitialized', () => {
                this.setupUI();
            });
        }
    }
    
    setupUI() {
        this.createAppStructure();
        this.setupEventListeners();
        this.setupNavigation();
        this.setupUserInterface();
        this.loadDefaultSection();
        
        console.log('[SOURCE-UI] UI controller initialized');
    }
    
    createAppStructure() {
        const appContainer = document.getElementById('app-container');
        if (!appContainer) return;
        
        // Create the complete app structure
        appContainer.innerHTML = `
            <!-- Header -->
            <header id="header" class="fixed top-0 w-full bg-white border-b border-neutral-200 z-50">
                <div class="flex items-center justify-between px-6 py-3">
                    <div class="flex items-center">
                        <button id="sidebar-toggle" class="mr-4 bg-neutral-600 hover:bg-neutral-700 text-white p-2 rounded-lg transition-all duration-300">
                            <i class="fa-solid fa-bars"></i>
                        </button>
                        <div class="mr-4">
                            <span class="text-black text-lg flex items-center cursor-pointer">
                                Ultima Robotics Link Management Platform
                            </span>
                        </div>
                    </div>
                    <div class="flex items-center space-x-4">
                        <!-- Connection Status -->
                        <div id="connection-status" class="flex items-center space-x-2 px-3 py-1 rounded-full text-xs font-medium bg-green-100 text-green-800">
                            <div class="flex items-center">
                                <i id="http-status-icon" class="fas fa-circle text-green-500 mr-1"></i>
                                <span>HTTP</span>
                            </div>
                            <div class="flex items-center">
                                <i id="ws-status-icon" class="fas fa-circle text-green-500 mr-1"></i>
                                <span>WS</span>
                            </div>
                        </div>
                        <button id="settings-btn" class="text-neutral-600 hover:text-neutral-900">
                            <i class="fa-solid fa-cog"></i>
                        </button>
                        <button id="logout-btn" class="text-neutral-600 hover:text-red-600 transition-colors" title="Logout">
                            <i class="fa-solid fa-sign-out-alt"></i>
                        </button>
                    </div>
                </div>
            </header>

            <!-- Main Layout -->
            <div class="flex h-[800px] pt-16">
                <!-- Sidebar -->
                <div id="sidebar" class="w-64 bg-white border-r border-neutral-200 fixed h-full overflow-y-auto transition-transform duration-300 transform -translate-x-full">
                    <div class="p-4">
                        <nav class="space-y-1">
                            ${this.createNavigationHTML()}
                        </nav>
                    </div>
                </div>

                <!-- Main Content -->
                <main id="main-content" class="flex-1 ml-0 bg-neutral-50 p-8 transition-all duration-300">
                    <!-- Content will be loaded here -->
                    <div class="flex items-center justify-center h-64">
                        <div class="text-center">
                            <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                            <p class="text-neutral-600">Loading dashboard...</p>
                        </div>
                    </div>
                </main>
            </div>
        `;
    }
    
    createNavigationHTML() {
        return `
            <div class="relative">
                <span id="nav-system-dashboard" class="block px-3 py-2 text-sm text-neutral-600 bg-neutral-50 border-l-2 border-neutral-600 rounded-r cursor-pointer hover:bg-neutral-100" data-section="system-dashboard">
                    System Dashboard
                </span>
            </div>
            
            <div class="relative">
                <div id="nav-network-header" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg cursor-pointer hover:bg-neutral-50">
                    <div class="flex items-center">
                        <span>Network Management</span>
                    </div>
                    <button id="nav-network-toggle" type="button" class="text-xs text-neutral-400 p-1 rounded hover:bg-neutral-100 focus:outline-none">
                        <i class="fa-solid fa-chevron-down"></i>
                    </button>
                </div>
                <div id="nav-network-list" class="ml-6 mt-1 space-y-1 hidden">
                    <span id="nav-wired-config" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="wired-config">Wired Configuration</span>
                    <span id="nav-wireless-config" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="wireless-config">Wireless Configuration</span>
                    <span id="nav-cellular-config" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="cellular-config">Cellular Configuration</span>
                    <span id="nav-vpn-config" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="vpn-config">VPN Configuration</span>
                    <span id="nav-advanced-network" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="advanced-network">Advanced Network Options</span>
                    <span id="nav-network-benchmark" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="network-benchmark">Network Benchmark</span>
                    <span id="nav-network-priority" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="network-priority">Network Priority</span>
                </div>
            </div>
            
            <div class="relative">
                <div id="nav-devices-header" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg cursor-pointer hover:bg-neutral-50">
                    <div class="flex items-center">
                        <span>Devices Management</span>
                    </div>
                    <button id="devices-toggle" type="button" class="text-xs text-neutral-400 p-1 rounded hover:bg-neutral-100 focus:outline-none">
                        <i class="fa-solid fa-chevron-down"></i>
                    </button>
                </div>
                <div id="devices-list" class="ml-6 mt-1 space-y-1 hidden">
                    <span id="nav-mavlink-overview" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="mavlink-overview">Mavlink System Overview</span>
                    <span id="nav-attached-ip-devices" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="attached-ip-devices">Attached IP Devices</span>
                </div>
            </div>
            
            <div class="relative">
                <div id="nav-features-header" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg cursor-pointer hover:bg-neutral-50">
                    <div class="flex items-center">
                        <span>Features</span>
                    </div>
                    <button id="features-toggle" type="button" class="text-xs text-neutral-400 p-1 rounded hover:bg-neutral-100 focus:outline-none">
                        <i class="fa-solid fa-chevron-down"></i>
                    </button>
                </div>
                <div id="features-list" class="ml-6 mt-1 space-y-1 hidden">
                    <span id="nav-mavlink-extension" class="block px-3 py-2 text-sm text-neutral-500 hover:bg-neutral-100 rounded cursor-pointer" data-section="mavlink-extension">Mavlink Extension System</span>
                </div>
            </div>
            
            <div class="relative">
                <span id="nav-firmware-upgrade" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg hover:bg-neutral-100 cursor-pointer" data-section="firmware-upgrade">
                    <div class="flex items-center">
                        <span>Firmware Upgrade</span>
                    </div>
                </span>
            </div>
            
            <div class="relative">
                <span id="nav-license-management" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg hover:bg-neutral-100 cursor-pointer" data-section="license-management">
                    <div class="flex items-center">
                        <span>License Management</span>
                    </div>
                </span>
            </div>
            
            <div class="relative">
                <span id="nav-backup-restore" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg hover:bg-neutral-100 cursor-pointer" data-section="backup-restore">
                    <div class="flex items-center">
                        <span>Backup and Restore</span>
                    </div>
                </span>
            </div>
            
            <div class="relative">
                <span id="nav-about-us" class="flex items-center justify-between px-3 py-2 text-neutral-700 rounded-lg hover:bg-neutral-100 cursor-pointer" data-section="about-us">
                    <div class="flex items-center">
                        <span>About Us</span>
                    </div>
                </span>
            </div>
        `;
    }
    
    setupEventListeners() {
        // Sidebar toggle
        const sidebarToggle = document.getElementById('sidebar-toggle');
        if (sidebarToggle) {
            sidebarToggle.addEventListener('click', () => this.toggleSidebar());
        }
        
        // Logout button
        const logoutBtn = document.getElementById('logout-btn');
        if (logoutBtn) {
            logoutBtn.addEventListener('click', () => this.handleLogout());
        }
        
        // Settings button
        const settingsBtn = document.getElementById('settings-btn');
        if (settingsBtn) {
            settingsBtn.addEventListener('click', () => this.handleSettings());
        }
        
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.key === 'b') {
                e.preventDefault();
                this.toggleSidebar();
            }
        });
    }
    
    setupNavigation() {
        console.log('[SOURCE-UI] Setting up enhanced navigation...');
        
        // Navigation items
        const navItems = document.querySelectorAll('[data-section]');
        navItems.forEach(item => {
            item.addEventListener('click', (e) => {
                e.preventDefault();
                
                // Find the element with data-section attribute (handle nested clicks)
                let target = e.target;
                while (target && !target.getAttribute('data-section')) {
                    target = target.parentElement;
                }
                
                const section = target ? target.getAttribute('data-section') : null;
                console.log('[SOURCE-UI] Navigation clicked:', section, 'target:', target);
                
                if (section) {
                    this.switchToTab(section);
                } else {
                    console.error('[SOURCE-UI] No data-section attribute found in clicked element or its parents');
                }
            });
        });
        
        // Expandable navigation headers
        this.setupExpandableNav('network');
        this.setupExpandableNav('devices');
        this.setupExpandableNav('features');
        
        // Setup keyboard navigation
        this.setupKeyboardNavigation();
        
        console.log('[SOURCE-UI] Navigation setup complete');
    }
    
    setupKeyboardNavigation() {
        document.addEventListener('keydown', (e) => {
            // Ctrl/Cmd + number keys for quick navigation
            if ((e.ctrlKey || e.metaKey) && e.key >= '1' && e.key <= '9') {
                e.preventDefault();
                const navItems = document.querySelectorAll('[data-section]');
                const index = parseInt(e.key) - 1;
                if (navItems[index]) {
                    const section = navItems[index].getAttribute('data-section');
                    this.switchToTab(section);
                }
            }
        });
    }
    
    /**
     * Enhanced tab switching mechanism
     */
    switchToTab(sectionId) {
        console.log('[SOURCE-UI] Switching to tab:', sectionId);
        
        // Validate section exists
        if (!this.isValidSection(sectionId)) {
            console.warn('[SOURCE-UI] Invalid section:', sectionId);
            return;
        }
        
        // Cleanup current tab
        this.cleanupCurrentTab();
        
        // Update navigation state
        this.updateActiveNavigation(sectionId);
        
        // Load new tab content
        this.loadTabContent(sectionId);
        
        // Update current section
        this.currentSection = sectionId;
        
        // Emit tab change event
        this.emitTabChangeEvent(sectionId);
        
        console.log('[SOURCE-UI] Successfully switched to tab:', sectionId);
    }
    
    isValidSection(sectionId) {
        const validSections = [
            'system-dashboard',
            'wired-config', 'wireless-config', 'cellular-config', 'vpn-config',
            'advanced-network', 'network-benchmark', 'network-priority',
            'mavlink-overview', 'attached-ip-devices', 'mavlink-extension',
            'firmware-upgrade', 'license-management', 'backup-restore', 'about-us'
        ];
        return validSections.includes(sectionId);
    }
    
    cleanupCurrentTab() {
        console.log('[SOURCE-UI] Cleaning up current tab:', this.currentSection);
        
        // Cleanup dashboard if it's the current tab
        if (this.currentSection === 'system-dashboard' && this.dashboard) {
            if (typeof this.dashboard.destroy === 'function') {
                this.dashboard.destroy();
            }
            this.dashboard = null;
            // Clear global dashboard reference
            if (window.dashboard) {
                delete window.dashboard;
            }
        }
        
        // Cleanup any stored components for current tab
        if (this.tabComponents.has(this.currentSection)) {
            const component = this.tabComponents.get(this.currentSection);
            if (component && typeof component.destroy === 'function') {
                component.destroy();
            }
            this.tabComponents.delete(this.currentSection);
        }
        
        // Cleanup UI components for current tab
        const uiComponentKey = `${this.currentSection}-ui`;
        if (this.tabComponents.has(uiComponentKey)) {
            const uiComponent = this.tabComponents.get(uiComponentKey);
            if (uiComponent && typeof uiComponent.destroy === 'function') {
                uiComponent.destroy();
            }
            this.tabComponents.delete(uiComponentKey);
        }
        
        // Clear global references for specific tabs
        if (this.currentSection === 'network-priority') {
            if (window.networkPriorityManager) {
                delete window.networkPriorityManager;
            }
            if (window.networkPriorityUI) {
                delete window.networkPriorityUI;
            }
        }
        
        // Cleanup licence management
        if (this.currentSection === 'license-management') {
            if (window.licenceUI) {
                window.licenceUI.destroy();
                delete window.licenceUI;
            }
        }
        
        // Cleanup firmware upgrade
        if (this.currentSection === 'firmware-upgrade') {
            if (this.tabComponents.has('firmware-upgrade')) {
                const firmwareUpgradeComponents = this.tabComponents.get('firmware-upgrade');
                cleanupFirmwareUpgrade(firmwareUpgradeComponents);
            }
            // Clear global references
            if (window.firmwareUpgradeManager) {
                delete window.firmwareUpgradeManager;
            }
            if (window.firmwareUpgradeUI) {
                delete window.firmwareUpgradeUI;
            }
        }
        
        // Cleanup backup & restore
        if (this.currentSection === 'backup-restore') {
            if (this.tabComponents.has('backup-restore')) {
                const backupRestoreComponents = this.tabComponents.get('backup-restore');
                cleanupBackupRestore(backupRestoreComponents);
            }
            // Clear global references
            if (window.backupRestoreManager) {
                delete window.backupRestoreManager;
            }
            if (window.backupRestoreUI) {
                delete window.backupRestoreUI;
            }
        }
        
        // Cleanup MAVLink extension
        if (this.currentSection === 'mavlink-extension') {
            if (this.tabComponents.has('mavlink-extension')) {
                const mavlinkExtensionComponents = this.tabComponents.get('mavlink-extension');
                cleanupMavlinkExtension(mavlinkExtensionComponents);
            }
            // Clear global references
            if (window.mavlinkExtensionManager) {
                delete window.mavlinkExtensionManager;
            }
            if (window.mavlinkExtensionUI) {
                delete window.mavlinkExtensionUI;
            }
        }
        
        // Clear active tab content reference
        this.activeTabContent = null;
    }
    
    loadTabContent(sectionId) {
        const mainContent = document.getElementById('main-content');
        if (!mainContent) {
            console.error('[SOURCE-UI] Main content container not found');
            return;
        }
        
        console.log('[SOURCE-UI] Loading content for section:', sectionId);
        
        // Show loading state
        mainContent.innerHTML = this.createLoadingContent();
        
        // Load section content with slight delay for smooth transition
        setTimeout(() => {
            try {
                if (sectionId === 'system-dashboard' && this.dashboard) {
                    // Render the dashboard
                    this.dashboard.render();
                    this.activeTabContent = 'dashboard';
                } else {
                    // Load regular section content
                    const content = this.getSectionContent(sectionId);
                    mainContent.innerHTML = content;
                    this.activeTabContent = sectionId;
                    
                    // Initialize section-specific functionality
                    this.initializeSection(sectionId);
                }
            } catch (error) {
                console.error('[SOURCE-UI] Error loading section content:', error);
                mainContent.innerHTML = this.createErrorContent(error.message);
            }
        }, 200);
    }
    
    initializeSection(sectionId) {
        console.log('[SOURCE-UI] Initializing section:', sectionId);
        
        switch (sectionId) {
            case 'system-dashboard':
                this.initializeDashboard();
                break;
            case 'network-priority':
                this.initializeNetworkPriority();
                break;
            case 'network-benchmark':
                this.initializeNetworkBenchmark();
                break;
            case 'firmware-upgrade':
                this.initializeFirmwareUpgrade();
                break;
            case 'license-management':
                this.initializeLicenseManagement();
                break;
            case 'backup-restore':
                this.initializeBackupRestore();
                break;
            case 'mavlink-extension':
                this.initializeMavlinkExtension();
                break;
            // Add more section initializers as needed
            default:
                console.log('[SOURCE-UI] No specific initialization for section:', sectionId);
        }
    }
    
    emitTabChangeEvent(sectionId) {
        const event = new CustomEvent('tabChanged', {
            detail: {
                section: sectionId,
                previousSection: this.currentSection,
                timestamp: Date.now()
            }
        });
        document.dispatchEvent(event);
        console.log('[SOURCE-UI] Tab change event emitted for:', sectionId);
    }
    
    createErrorContent(errorMessage) {
        return `
            <div class="bg-red-50 border border-red-200 rounded-lg p-6">
                <div class="flex items-center">
                    <i class="fa-solid fa-exclamation-triangle text-red-500 mr-3"></i>
                    <div>
                        <h3 class="text-lg font-medium text-red-800">Error Loading Section</h3>
                        <p class="text-red-600 mt-1">${errorMessage}</p>
                        <button onclick="location.reload()" class="mt-3 px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                            Reload Page
                        </button>
                    </div>
                </div>
            </div>
        `;
    }
    
    setupExpandableNav(section) {
        const header = document.getElementById(`nav-${section}-header`);
        const toggle = document.getElementById(section === 'network' ? 'nav-network-toggle' : `${section}-toggle`);
        const list = document.getElementById(section === 'network' ? `nav-${section}-list` : `${section}-list`);
        
        if (header && toggle && list) {
            header.addEventListener('click', () => {
                const isHidden = list.classList.contains('hidden');
                
                if (isHidden) {
                    list.classList.remove('hidden');
                    toggle.innerHTML = '<i class="fa-solid fa-chevron-up"></i>';
                } else {
                    list.classList.add('hidden');
                    toggle.innerHTML = '<i class="fa-solid fa-chevron-down"></i>';
                }
            });
            
            // Also add click handler for the toggle button itself
            toggle.addEventListener('click', (e) => {
                e.stopPropagation();
                const isHidden = list.classList.contains('hidden');
                
                if (isHidden) {
                    list.classList.remove('hidden');
                    toggle.innerHTML = '<i class="fa-solid fa-chevron-up"></i>';
                } else {
                    list.classList.add('hidden');
                    toggle.innerHTML = '<i class="fa-solid fa-chevron-down"></i>';
                }
            });
        } else {
            console.error(`[SOURCE-UI] Failed to setup expandable nav for ${section}:`, {
                header: !!header,
                toggle: !!toggle,
                list: !!list
            });
        }
    }
    
    setupUserInterface() {
        const user = this.sourceManager.getCurrentUser();
        
        // Update user info in UI if needed
        if (user) {
            console.log('[SOURCE-UI] User interface setup for:', user.username);
        }
        
        // Setup connection status indicator
        this.setupConnectionStatus();
    }
    
    initializeDashboard() {
        console.log('[SOURCE-UI] Initializing dashboard...');
        
        try {
            // Clean up existing dashboard if it exists
            if (this.dashboard) {
                if (typeof this.dashboard.destroy === 'function') {
                    this.dashboard.destroy();
                }
                this.dashboard = null;
            }
            
            // Create dashboard instance
            this.dashboard = new Dashboard(this);
            
            // Make dashboard globally accessible for popup button handlers
            window.dashboard = this.dashboard;
            
            // Make dashboard loading UI globally accessible
            if (this.dashboard && this.dashboard.dashboardUI && this.dashboard.dashboardUI.loadingUI) {
                window.dashboardLoadingUI = this.dashboard.dashboardUI.loadingUI;
            }
            
            console.log('[SOURCE-UI] Dashboard initialized successfully');
            
        } catch (error) {
            console.error('[SOURCE-UI] Failed to initialize dashboard:', error);
            this.dashboard = null;
        }
    }
    
    setupConnectionStatus() {
        // Initialize both HTTP and WebSocket status
        this.updateConnectionStatus('http', 'connected');
        this.updateConnectionStatus('websocket', 'connecting');
        
        // Listen for WebSocket events
        document.addEventListener('websocketOpen', (event) => {
            this.updateConnectionStatus('websocket', 'connected');
        });
        
        document.addEventListener('websocketClose', (event) => {
            this.updateConnectionStatus('websocket', 'disconnected');
        });
        
        document.addEventListener('websocketError', (event) => {
            this.updateConnectionStatus('websocket', 'error');
        });
        
        document.addEventListener('websocketReconnecting', (event) => {
            this.updateConnectionStatus('websocket', 'connecting');
        });
        
        document.addEventListener('websocketMaxReconnectReached', (event) => {
            this.updateConnectionStatus('websocket', 'error');
        });
        
        // Monitor HTTP connection with periodic checks
        this.startHttpConnectionMonitoring();
        
        // Initial WebSocket status check
        this.checkInitialWebSocketStatus();
    }
    
    updateConnectionStatus(connectionType, status) {
        if (connectionType === 'http') {
            const httpIcon = document.getElementById('http-status-icon');
            if (!httpIcon) return;
            
            switch (status) {
                case 'connected':
                    httpIcon.setAttribute('class', 'fas fa-circle text-green-500 mr-1');
                    break;
                case 'disconnected':
                case 'error':
                    httpIcon.setAttribute('class', 'fas fa-circle text-red-500 mr-1');
                    break;
                case 'connecting':
                    httpIcon.setAttribute('class', 'fas fa-circle text-yellow-500 mr-1 animate-pulse');
                    break;
            }
        } else if (connectionType === 'websocket') {
            const wsIcon = document.getElementById('ws-status-icon');
            if (!wsIcon) return;
            
            switch (status) {
                case 'connected':
                    wsIcon.setAttribute('class', 'fas fa-circle text-green-500 mr-1');
                    break;
                case 'disconnected':
                    wsIcon.setAttribute('class', 'fas fa-circle text-red-500 mr-1');
                    break;
                case 'error':
                    wsIcon.setAttribute('class', 'fas fa-circle text-red-500 mr-1');
                    break;
                case 'connecting':
                    wsIcon.setAttribute('class', 'fas fa-circle text-yellow-500 mr-1 animate-pulse');
                    break;
            }
        }
        
        // Update overall container status based on both connections
        this.updateOverallConnectionStatus();
    }
    
    updateOverallConnectionStatus() {
        const container = document.getElementById('connection-status');
        const httpIcon = document.getElementById('http-status-icon');
        const wsIcon = document.getElementById('ws-status-icon');
        
        if (!container || !httpIcon || !wsIcon) return;
        
        // Check status using class attributes (compatible with SVG elements)
        const httpClass = httpIcon.getAttribute('class') || '';
        const wsClass = wsIcon.getAttribute('class') || '';
        
        const httpConnected = httpClass.includes('text-green-500');
        const httpConnecting = httpClass.includes('text-yellow-500');
        const httpError = httpClass.includes('text-red-500');
        
        const wsConnected = wsClass.includes('text-green-500');
        const wsConnecting = wsClass.includes('text-yellow-500');
        const wsError = wsClass.includes('text-red-500');
        
        // Determine overall status based on individual connections
        if (httpConnected && wsConnected) {
            // Both connected - green
            container.setAttribute('class', 'flex items-center space-x-2 px-3 py-1 rounded-full text-xs font-medium bg-green-100 text-green-800');
            this.updateConnectionTooltip('All systems operational');
        } else if (httpConnecting || wsConnecting) {
            // At least one connecting - yellow with animation
            container.setAttribute('class', 'flex items-center space-x-2 px-3 py-1 rounded-full text-xs font-medium bg-yellow-100 text-yellow-800 animate-pulse');
            this.updateConnectionTooltip('Establishing connections...');
        } else if (httpError || wsError) {
            // At least one has errors - red
            container.setAttribute('class', 'flex items-center space-x-2 px-3 py-1 rounded-full text-xs font-medium bg-red-100 text-red-800');
            this.updateConnectionTooltip('Connection errors detected');
        } else if (httpConnected || wsConnected) {
            // Partial connection - yellow
            container.setAttribute('class', 'flex items-center space-x-2 px-3 py-1 rounded-full text-xs font-medium bg-yellow-100 text-yellow-800');
            this.updateConnectionTooltip('Partial connection - some services unavailable');
        } else {
            // Both disconnected - red
            container.setAttribute('class', 'flex items-center space-x-2 px-3 py-1 rounded-full text-xs font-medium bg-red-100 text-red-800');
            this.updateConnectionTooltip('No connection - services unavailable');
        }
    }
    
    updateConnectionTooltip(message) {
        const container = document.getElementById('connection-status');
        if (container) {
            container.setAttribute('title', message);
        }
    }
    
    checkInitialWebSocketStatus() {
        // Check if WebSocket client exists and get its current status
        if (this.sourceManager && this.sourceManager.getWebSocketClient) {
            const wsClient = this.sourceManager.getWebSocketClient();
            if (wsClient) {
                const stats = wsClient.getStats();
                if (stats.isConnected) {
                    this.updateConnectionStatus('websocket', 'connected');
                } else if (stats.isConnecting) {
                    this.updateConnectionStatus('websocket', 'connecting');
                } else {
                    this.updateConnectionStatus('websocket', 'disconnected');
                }
            }
        }
    }
    
    startHttpConnectionMonitoring() {
        // Monitor HTTP connection every 30 seconds
        const monitorInterval = setInterval(async () => {
            try {
                // Simple health check to verify HTTP connection
                const response = await fetch('/api/auth/validate', {
                    method: 'GET',
                    headers: {
                        'Authorization': `Bearer ${this.sourceManager.getTokenManager().getAccessToken()}`
                    }
                });
                
                if (response.ok || response.status === 401) {
                    // 401 is OK - it means server is responding, just token is invalid
                    this.updateConnectionStatus('http', 'connected');
                    // Dispatch event for other components
                    document.dispatchEvent(new CustomEvent('httpConnectionSuccess', {
                        detail: { status: 'connected', response: response.status }
                    }));
                } else {
                    this.updateConnectionStatus('http', 'error');
                    document.dispatchEvent(new CustomEvent('httpConnectionError', {
                        detail: { status: 'error', response: response.status }
                    }));
                }
            } catch (error) {
                this.updateConnectionStatus('http', 'disconnected');
                document.dispatchEvent(new CustomEvent('httpConnectionError', {
                    detail: { status: 'disconnected', error: error.message }
                }));
            }
        }, 30000);
        
        // Store interval ID for cleanup
        this.httpMonitorInterval = monitorInterval;
        
        // Initial check
        setTimeout(() => {
            // Trigger first check immediately
            this.checkHttpConnection();
        }, 1000);
    }
    
    async checkHttpConnection() {
        try {
            const response = await fetch('/api/auth/validate', {
                method: 'GET',
                headers: {
                    'Authorization': `Bearer ${this.sourceManager.getTokenManager().getAccessToken()}`
                }
            });
            
            if (response.ok || response.status === 401) {
                this.updateConnectionStatus('http', 'connected');
            } else {
                this.updateConnectionStatus('http', 'error');
            }
        } catch (error) {
            this.updateConnectionStatus('http', 'disconnected');
        }
    }
    
    toggleSidebar() {
        const sidebar = document.getElementById('sidebar');
        const mainContent = document.getElementById('main-content');
        
        if (!sidebar || !mainContent) return;
        
        this.sidebarOpen = !this.sidebarOpen;
        
        if (this.sidebarOpen) {
            sidebar.classList.remove('-translate-x-full');
            mainContent.classList.add('ml-64');
        } else {
            sidebar.classList.add('-translate-x-full');
            mainContent.classList.remove('ml-64');
        }
    }
    
    navigateToSection(sectionId) {
        // Cleanup previous section if needed
        this.cleanupPreviousSection();
        
        // Update active navigation
        this.updateActiveNavigation(sectionId);
        
        // Load section content
        this.loadSectionContent(sectionId);
        
        this.currentSection = sectionId;
        console.log('[SOURCE-UI] Navigated to section:', sectionId);
    }
    
    cleanupPreviousSection() {
        // Cleanup the previous section's components if they exist
        const previousComponent = this.tabComponents.get(this.currentSection);
        
        if (previousComponent) {
            try {
                // Call destroy method if it exists
                if (typeof previousComponent.destroy === 'function') {
                    previousComponent.destroy();
                }
                
                // Clear any running tests or intervals
                if (this.currentSection === 'network-benchmark' && window.networkBenchmark) {
                    // Stop any running tests
                    if (window.networkBenchmark.currentTest) {
                        window.networkBenchmark.stopTest();
                    }
                }
                
                console.log(`[SOURCE-UI] Cleaned up section: ${this.currentSection}`);
            } catch (error) {
                console.error(`[SOURCE-UI] Error cleaning up section ${this.currentSection}:`, error);
            }
        }
    }
    
    updateActiveNavigation(sectionId) {
        // Remove active class from all nav items
        const navItems = document.querySelectorAll('[data-section]');
        navItems.forEach(item => {
            item.classList.remove('bg-neutral-50', 'border-l-2', 'border-neutral-600', 'text-neutral-600');
            item.classList.add('text-neutral-500');
        });
        
        // Add active class to current nav item
        const activeItem = document.querySelector(`[data-section="${sectionId}"]`);
        if (activeItem) {
            activeItem.classList.remove('text-neutral-500');
            activeItem.classList.add('bg-neutral-50', 'border-l-2', 'border-neutral-600', 'text-neutral-600');
        }
    }
    
    loadSectionContent(sectionId) {
        const mainContent = document.getElementById('main-content');
        if (!mainContent) return;
        
        // Show loading state
        mainContent.innerHTML = this.createLoadingContent();
        
        // Load section content based on ID
        setTimeout(() => {
            if (sectionId === 'system-dashboard' && this.dashboard) {
                // Render the dashboard
                this.dashboard.render();
            } else {
                // Load regular section content
                mainContent.innerHTML = this.getSectionContent(sectionId);
                
                // Initialize section-specific components after content is loaded
                this.initializeSection(sectionId);
            }
        }, 300);
    }
    
    createLoadingContent() {
        return `
            <div class="flex items-center justify-center h-64">
                <div class="text-center">
                    <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                    <p class="text-neutral-600">Loading...</p>
                </div>
            </div>
        `;
    }
    
    getSectionContent(sectionId) {
        const contents = {
            'system-dashboard': this.createSystemDashboardContent(),
            'wired-config': this.createWiredConfigContent(),
            'wireless-config': this.createWirelessConfigContent(),
            'cellular-config': this.createCellularConfigContent(),
            'vpn-config': this.createVPNConfigContent(),
            'advanced-network': this.createAdvancedNetworkContent(),
            'network-benchmark': this.createNetworkBenchmarkContent(),
            'network-priority': this.createNetworkPriorityContent(),
            'mavlink-overview': this.createMavlinkOverviewContent(),
            'attached-ip-devices': this.createAttachedDevicesContent(),
            'mavlink-extension': this.createMavlinkExtensionContent(),
            'firmware-upgrade': this.createFirmwareUpgradeContent(),
            'license-management': this.createLicenseManagementContent(),
            'backup-restore': this.createBackupRestoreContent(),
            'about-us': this.createAboutUsContent()
        };
        
        return contents[sectionId] || this.createDefaultContent();
    }
    
    createSystemDashboardContent() {
        // Return empty content - dashboard will be rendered by Dashboard class
        return '';
    }
    
    createDefaultContent() {
        return `
            <div class="bg-white rounded-lg shadow p-6">
                <h2 class="text-2xl font-bold text-neutral-900 mb-4">Section</h2>
                <p class="text-neutral-600">This section is under development.</p>
            </div>
        `;
    }
    // Placeholder methods for other sections
    createWiredConfigContent() { 
        return this.createDefaultContent(); 
    }
    
    createWirelessConfigContent() { 
        return this.createDefaultContent(); 
    }
    
    createCellularConfigContent() { 
        return this.createDefaultContent(); 
    }
    
    createVPNConfigContent() { 
        return this.createDefaultContent(); 
    }
    
    createAdvancedNetworkContent() { 
        return this.createDefaultContent(); 
    }
    
    createNetworkBenchmarkContent() {
        // Return the network benchmark UI container
        // The actual UI will be rendered by the network-benchmark-ui.js component
        return `
            <div class="bg-white rounded-lg shadow p-6">
                <div class="flex items-center justify-center h-64">
                    <div class="text-center">
                        <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                        <p class="text-neutral-600">Loading Network Benchmark...</p>
                    </div>
                </div>
            </div>
        `;
    }
    
    createNetworkPriorityContent() {
        // Return the network priority UI based on the original design
        return `
            <div class="bg-white rounded-lg shadow p-6">
                <div class="flex items-center justify-center h-64">
                    <div class="text-center">
                        <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                        <p class="text-neutral-600">Loading Network Priority...</p>
                    </div>
                </div>
            </div>
        `;
    }
    
    createMavlinkOverviewContent() { 
        return this.createDefaultContent(); 
    }
    
    createAttachedDevicesContent() { 
        return `
            <div class="bg-white rounded-lg shadow p-6">
                <h2 class="text-2xl font-bold text-neutral-900 mb-4">Attached Devices</h2>
                <p class="text-neutral-600">This section is under development.</p>
            </div>
        `;
    }
    
    createMavlinkExtensionContent() {
        // Return the MAVLink extension UI container
        // The actual UI will be rendered by the mavlink-extension-ui.js component
        return getMavlinkExtensionContentHTML();
    }
    
    async initializeMavlinkExtension() {
        console.log('[SOURCE-UI] Initializing MAVLink Extension section...');
        
        try {
            // Only initialize if this is the currently active section
            if (this.currentSection !== 'mavlink-extension') {
                console.log('[SOURCE-UI] MAVLink Extension is not the active section, skipping initialization');
                return;
            }
            
            // Check if MAVLink extension already exists
            if (this.tabComponents.has('mavlink-extension')) {
                console.log('[SOURCE-UI] MAVLink Extension already exists, reusing existing instance');
                return;
            }
            
            // Initialize the MAVLink extension module
            const mavlinkExtensionComponents = await initializeMavlinkExtension(this.sourceManager);
            
            // Store the component references for cleanup
            this.tabComponents.set('mavlink-extension', mavlinkExtensionComponents);
            this.tabComponents.set('mavlink-extension-manager', mavlinkExtensionComponents.manager);
            this.tabComponents.set('mavlink-extension-ui', mavlinkExtensionComponents.ui);
            
            // Make globally accessible for external handlers
            window.mavlinkExtensionManager = mavlinkExtensionComponents.manager;
            window.mavlinkExtensionUI = mavlinkExtensionComponents.ui;
            
            console.log('[SOURCE-UI] MAVLink Extension section initialized successfully');
        } catch (error) {
            console.error('[SOURCE-UI] Error in initializeMavlinkExtension:', error);
            
            // Show error message in main content
            const mainContent = document.getElementById('main-content');
            if (mainContent) {
                mainContent.innerHTML = this.createErrorContent(`Failed to initialize MAVLink extension: ${error.message}`);
            }
        }
    }
    
    createLicenseManagementContent() {
        // Return the licence UI container
        // The actual UI will be rendered by the licence-ui.js component
        return `
            <div id="licence-container" class="w-full">
                <!-- Licence UI will be rendered here -->
                <div class="flex items-center justify-center h-64">
                    <div class="text-center">
                        <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                        <p class="text-neutral-600">Loading Licence Management...</p>
                    </div>
                </div>
            </div>
        `;
    }
    
    createFirmwareUpgradeContent() {
        // Return empty container - firmware upgrade UI will be rendered by FirmwareUpgradeUI class
        return '';
    }
    
    createBackupRestoreContent() {
        // Initialize backup & restore module
        this.initializeBackupRestore();
        return ''; // Content will be rendered by the backup-restore UI
    }
    createAboutUsContent() { return this.createDefaultContent(); }
    
    loadDefaultSection() {
        this.switchToTab('system-dashboard');
    }
    
    /**
     * Initialize Network Priority section
     */
    initializeNetworkPriority() {
        console.log('[SOURCE-UI] Initializing Network Priority section...');
        
        // Initialize network benchmark loading UI for shared diagnostic tools
        if (window.networkBenchmarkLoadingUI && !window.networkBenchmarkLoadingUI.isInitialized) {
            window.networkBenchmarkLoadingUI.initialize();
            console.log('[SOURCE-UI] Network Benchmark Loading UI initialized for shared diagnostics');
        }
        
        // Create data loading UI component first
        const dataLoadingUIComponent = new DataLoadingUI();
        this.tabComponents.set('data-loading-ui', dataLoadingUIComponent);
        
        // Show loading states immediately
        dataLoadingUIComponent.showAllLoadingStates();
        
        // Make data loading UI globally accessible
        window.dataLoadingUI = dataLoadingUIComponent;
        
        // Create network priority component and store reference
        const networkPriorityComponent = new NetworkPriorityManager(this.sourceManager);
        this.tabComponents.set('network-priority', networkPriorityComponent);
        
        // Create UI component
        const networkPriorityUIComponent = new NetworkPriorityUI(networkPriorityComponent);
        this.tabComponents.set('network-priority-ui', networkPriorityUIComponent);
        
        // Make components globally accessible for HTML onclick handlers
        window.networkPriorityManager = networkPriorityComponent;
        window.networkPriorityUI = networkPriorityUIComponent;
        
        // Add diagnostic tools button handlers for network priority tab
        this.addNetworkPriorityDiagnosticHandlers();
        
        // Initialize collapsable sections with proper initial states
        this.initializeCollapsibleSections();
        
        console.log('[SOURCE-UI] Network Priority section initialized with loading UI and shared diagnostics');
    }
    
    /**
     * Add diagnostic tools handlers for network priority tab
     */
    addNetworkPriorityDiagnosticHandlers() {
        // Add event listeners for diagnostic tool buttons if they exist in the network priority UI
        setTimeout(() => {
            // Traceroute button
            const tracerouteBtn = document.getElementById('network-priority-traceroute-btn');
            if (tracerouteBtn) {
                tracerouteBtn.addEventListener('click', () => {
                    if (window.networkBenchmarkLoadingUI) {
                        window.networkBenchmarkLoadingUI.showTraceroutePopup();
                    }
                });
            }
            
            // Ping test button
            const pingTestBtn = document.getElementById('network-priority-ping-btn');
            if (pingTestBtn) {
                pingTestBtn.addEventListener('click', () => {
                    if (window.networkBenchmarkLoadingUI) {
                        window.networkBenchmarkLoadingUI.showPingTestPopup();
                    }
                });
            }
            
            // DNS lookup button
            const dnsLookupBtn = document.getElementById('network-priority-dns-btn');
            if (dnsLookupBtn) {
                dnsLookupBtn.addEventListener('click', () => {
                    if (window.networkBenchmarkLoadingUI) {
                        window.networkBenchmarkLoadingUI.showDnsLookupPopup();
                    }
                });
            }
            
            // Bandwidth test button
            const bandwidthTestBtn = document.getElementById('network-priority-bandwidth-btn');
            if (bandwidthTestBtn) {
                bandwidthTestBtn.addEventListener('click', () => {
                    if (window.networkBenchmarkLoadingUI) {
                        window.networkBenchmarkLoadingUI.showBandwidthTestPopup();
                    }
                });
            }
            
            console.log('[SOURCE-UI] Network priority diagnostic handlers added');
        }, 500); // Wait for UI to be rendered
    }
    
    /**
     * Initialize Network Benchmark section
     */
    initializeNetworkBenchmark() {
        console.log('[SOURCE-UI] Initializing Network Benchmark section...');
        
        try {
            // Initialize the network benchmark components if they exist
            if (window.networkBenchmark && !window.networkBenchmark.isInitialized) {
                window.networkBenchmark.initialize().then(success => {
                    if (success) {
                        console.log('[SOURCE-UI] Network Benchmark initialized successfully');
                    } else {
                        console.error('[SOURCE-UI] Failed to initialize Network Benchmark');
                    }
                }).catch(error => {
                    console.error('[SOURCE-UI] Error initializing Network Benchmark:', error);
                });
            }
            
            // Store the component reference for cleanup
            this.tabComponents.set('network-benchmark', window.networkBenchmark);
            
        } catch (error) {
            console.error('[SOURCE-UI] Error in initializeNetworkBenchmark:', error);
        }
    }
    
    /**
     * Initialize Firmware Upgrade section
     */
    async initializeFirmwareUpgrade() {
        console.log('[SOURCE-UI] Initializing Firmware Upgrade section...');
        
        try {
            // Only initialize if this is the currently active section
            if (this.currentSection !== 'firmware-upgrade') {
                console.log('[SOURCE-UI] Firmware Upgrade is not the active section, skipping initialization');
                return;
            }
            
            // Check if firmware upgrade already exists
            if (this.tabComponents.has('firmware-upgrade')) {
                console.log('[SOURCE-UI] Firmware Upgrade already exists, reusing existing instance');
                return;
            }
            
            // Initialize the firmware upgrade module
            const firmwareUpgradeComponents = await initializeFirmwareUpgrade(this.sourceManager);
            
            // Store the component references for cleanup
            this.tabComponents.set('firmware-upgrade', firmwareUpgradeComponents);
            this.tabComponents.set('firmware-upgrade-manager', firmwareUpgradeComponents.manager);
            this.tabComponents.set('firmware-upgrade-ui', firmwareUpgradeComponents.ui);
            
            // Make globally accessible for external handlers
            window.firmwareUpgradeManager = firmwareUpgradeComponents.manager;
            window.firmwareUpgradeUI = firmwareUpgradeComponents.ui;
            
            console.log('[SOURCE-UI] Firmware Upgrade section initialized successfully');
        } catch (error) {
            console.error('[SOURCE-UI] Error in initializeFirmwareUpgrade:', error);
            
            // Show error message in main content
            const mainContent = document.getElementById('main-content');
            if (mainContent) {
                mainContent.innerHTML = this.createErrorContent(`Failed to initialize firmware upgrade: ${error.message}`);
            }
        }
    }
    
    /**
     * Initialize License Management section
     */
    initializeLicenseManagement() {
        console.log('[SOURCE-UI] Initializing License Management section...');
        
        try {
            // Only initialize if this is the currently active section
            if (this.currentSection !== 'license-management') {
                console.log('[SOURCE-UI] License Management is not the active section, skipping initialization');
                return;
            }
            
            // Check if licence UI already exists
            if (window.licenceUI) {
                console.log('[SOURCE-UI] Licence UI already exists, reusing existing instance');
                return;
            }
            
            // Create and initialize the licence UI
            const licenceContainer = document.getElementById('licence-container');
            if (licenceContainer) {
                // Initialize the Licence UI component with sourceManager
                window.licenceUI = new LicenceUI(this.sourceManager);
                
                // Store the component reference for cleanup
                this.tabComponents.set('license-management', window.licenceUI);
                
                console.log('[SOURCE-UI] License Management section initialized successfully');
            } else {
                console.error('[SOURCE-UI] Licence container not found');
            }
        } catch (error) {
            console.error('[SOURCE-UI] Error in initializeLicenseManagement:', error);
        }
    }
    
    async initializeBackupRestore() {
        console.log('[SOURCE-UI] Initializing Backup & Restore section...');
        
        try {
            // Only initialize if this is the currently active section
            if (this.currentSection !== 'backup-restore') {
                console.log('[SOURCE-UI] Backup & Restore is not the active section, skipping initialization');
                return;
            }
            
            // Check if backup restore already exists
            if (this.tabComponents.has('backup-restore')) {
                console.log('[SOURCE-UI] Backup & Restore already initialized, skipping');
                return;
            }
            
            // Initialize backup restore module
            const backupRestoreComponents = await initializeBackupRestore(this.sourceManager);
            
            // Store the component references for cleanup
            this.tabComponents.set('backup-restore', backupRestoreComponents);
            this.tabComponents.set('backup-restore-manager', backupRestoreComponents.manager);
            this.tabComponents.set('backup-restore-ui', backupRestoreComponents.ui);
            
            // Make globally accessible for external handlers
            window.backupRestoreManager = backupRestoreComponents.manager;
            window.backupRestoreUI = backupRestoreComponents.ui;
            
            console.log('[SOURCE-UI] Backup & Restore section initialized successfully');
            
        } catch (error) {
            console.error('[SOURCE-UI] Error in initializeBackupRestore:', error);
            
            // Show error message in main content
            const mainContent = document.getElementById('main-content');
            if (mainContent) {
                mainContent.innerHTML = this.createErrorContent(`Failed to initialize backup & restore: ${error.message}`);
            }
        }
    }
    
    async handleLogout() {
        if (window.popupManager) {
            const result = await window.popupManager.show({
                title: 'Confirm Logout',
                description: 'Are you sure you want to logout?',
                buttonText: 'Logout',
                cancelButtonText: 'Cancel',
                buttonClass: 'bg-red-600 hover:bg-red-700 text-white'
            });
            
            if (result === 'confirm') {
                this.sourceManager.logout();
            }
        } else {
            if (confirm('Are you sure you want to logout?')) {
                this.sourceManager.logout();
            }
        }
    }
    
    handleSettings() {
        if (window.popupManager) {
            window.popupManager.showCustom({
                title: '', // No title since it's in the custom content
                content: `
                    <div id="settings-modal" class="bg-white rounded-lg shadow-2xl w-[700px] max-w-[90vw]">
                        <!-- Modal Header -->
                        <div id="modal-header" class="flex items-center justify-between px-6 py-4 border-b border-neutral-200">
                            <h2 class="text-xl text-neutral-900">Settings</h2>
                            <button id="close-btn" class="p-1 hover:bg-neutral-100 rounded-md transition-colors">
                                <i class="text-neutral-500 text-lg">✕</i>
                            </button>
                        </div>

                        <!-- Tab Navigation -->
                        <div id="tab-navigation" class="border-b border-neutral-200">
                            <nav class="flex">
                                <button id="password-tab" class="px-6 py-3 text-sm text-neutral-900 border-b-2 border-neutral-900 bg-neutral-50" data-tab="password">
                                    Manage Password
                                </button>
                                <button id="token-tab" class="px-6 py-3 text-sm text-neutral-600 hover:text-neutral-900 hover:bg-neutral-50 transition-colors" data-tab="token">
                                    Manage Access Token
                                </button>
                                <button id="cloud-tab" class="px-6 py-3 text-sm text-neutral-600 hover:text-neutral-900 hover:bg-neutral-50 transition-colors" data-tab="cloud">
                                    Ultima Cloud
                                </button>
                            </nav>
                        </div>

                        <!-- Modal Content -->
                        <div id="modal-content" class="px-6 py-6">
                            <!-- Password Management Section -->
                            <div id="password-section">
                                <div class="mb-6">
                                    <h3 class="text-lg text-neutral-900 mb-2">Change your password</h3>
                                    <p class="text-sm text-neutral-600">Create a strong password with at least 12 characters.</p>
                                </div>

                                <!-- Password Form -->
                                <form id="password-form" class="space-y-5">
                                    <!-- Current Password Field -->
                                    <div class="form-group">
                                        <label for="current-password" class="block text-sm text-neutral-700 mb-2">
                                            Current password
                                        </label>
                                        <div class="relative">
                                            <input type="password" id="current-password" name="current-password" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-500 focus:border-transparent" required>
                                        </div>
                                    </div>

                                    <!-- New Password Field -->
                                    <div class="form-group">
                                        <label for="new-password" class="block text-sm text-neutral-700 mb-2">
                                            New password
                                        </label>
                                        <div class="relative">
                                            <input type="password" id="new-password" name="new-password" placeholder="Use 12+ characters, numbers & symbols" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-500 focus:border-transparent" required>
                                        </div>
                                    </div>

                                    <!-- Confirm Password Field -->
                                    <div class="form-group">
                                        <label for="confirm-password" class="block text-sm text-neutral-700 mb-2">
                                            Confirm new password
                                        </label>
                                        <div class="relative">
                                            <input type="password" id="confirm-password" name="confirm-password" placeholder="Re-enter new password" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-500 focus:border-transparent" required>
                                        </div>
                                    </div>
                                </form>
                            </div>

                            <!-- Access Token Management Section -->
                            <div id="token-section" class="hidden">
                                <div class="mb-6">
                                    <h3 class="text-lg text-neutral-900 mb-2">Manage login keys</h3>
                                    <p class="text-sm text-neutral-600">Generate and manage API keys for secure access to your account.</p>
                                </div>

                                <!-- Key Generation Section -->
                                <div class="bg-neutral-50 rounded-lg p-4 mb-6">
                                    <div class="mb-4">
                                        <label for="key-name" class="block text-sm text-neutral-700 mb-2">
                                            Key name
                                        </label>
                                        <div class="flex gap-3">
                                            <input type="text" id="key-name" name="key-name" placeholder="Enter a descriptive name for your key" class="flex-1 px-3 py-2 bg-white border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-neutral-500 focus:border-transparent">
                                            <button id="generate-btn" type="button" class="px-4 py-2 text-sm text-white bg-neutral-900 rounded-md hover:bg-neutral-800 focus:outline-none focus:ring-2 focus:ring-neutral-500 transition-colors">
                                                Generate
                                            </button>
                                        </div>
                                    </div>
                                    <p class="text-xs text-neutral-500">
                                        Generated keys will be automatically downloaded. Store them securely as they cannot be recovered.
                                    </p>
                                </div>

                                <!-- Keys Table -->
                                <div class="border border-neutral-200 rounded-lg overflow-hidden">
                                    <table class="w-full">
                                        <thead class="bg-neutral-100">
                                            <tr>
                                                <th class="px-4 py-3 text-left text-xs text-neutral-600 uppercase tracking-wider">Name</th>
                                                <th class="px-4 py-3 text-left text-xs text-neutral-600 uppercase tracking-wider">Created</th>
                                                <th class="px-4 py-3 text-left text-xs text-neutral-600 uppercase tracking-wider">Last Used</th>
                                                <th class="px-4 py-3 text-left text-xs text-neutral-600 uppercase tracking-wider">Actions</th>
                                            </tr>
                                        </thead>
                                        <tbody class="bg-white divide-y divide-neutral-200">
                                            <tr>
                                                <td class="px-4 py-3 text-sm text-neutral-900">Production API Key</td>
                                                <td class="px-4 py-3 text-sm text-neutral-600">Jan 15, 2025</td>
                                                <td class="px-4 py-3 text-sm text-neutral-600">2 hours ago</td>
                                                <td class="px-4 py-3 text-sm">
                                                    <div class="flex gap-2">
                                                        <button class="px-3 py-1 text-xs text-neutral-700 bg-neutral-100 rounded hover:bg-neutral-200 transition-colors">
                                                            Download
                                                        </button>
                                                        <button class="px-3 py-1 text-xs text-neutral-700 bg-neutral-50 rounded hover:bg-neutral-100 transition-colors">
                                                            Revoke
                                                        </button>
                                                    </div>
                                                </td>
                                            </tr>
                                            <tr>
                                                <td class="px-4 py-3 text-sm text-neutral-900">Development Key</td>
                                                <td class="px-4 py-3 text-sm text-neutral-600">Jan 12, 2025</td>
                                                <td class="px-4 py-3 text-sm text-neutral-600">Yesterday</td>
                                                <td class="px-4 py-3 text-sm">
                                                    <div class="flex gap-2">
                                                        <button class="px-3 py-1 text-xs text-neutral-700 bg-neutral-100 rounded hover:bg-neutral-200 transition-colors">
                                                            Download
                                                        </button>
                                                        <button class="px-3 py-1 text-xs text-neutral-700 bg-neutral-50 rounded hover:bg-neutral-100 transition-colors">
                                                            Revoke
                                                        </button>
                                                    </div>
                                                </td>
                                            </tr>
                                        </tbody>
                                    </table>
                                </div>
                            </div>

                            <!-- Cloud Section -->
                            <div id="cloud-section" class="hidden">
                                <div class="flex flex-col items-center justify-center text-center py-12">
                                    <div class="mb-4">
                                        <span class="inline-block px-3 py-1 text-xs text-neutral-600 bg-neutral-100 rounded-full">
                                            Coming soon
                                        </span>
                                    </div>
                                    
                                    <h3 class="text-xl text-neutral-900 mb-3">
                                        Ultima Cloud integration is on the way
                                    </h3>
                                    
                                    <p class="text-sm text-neutral-600 max-w-md">
                                        We're building secure workspace sync and backups. You'll be able to connect your account and manage cloud sessions here once available.
                                    </p>
                                </div>
                            </div>
                        </div>

                        <!-- Modal Footer -->
                        <div id="modal-footer" class="flex justify-end gap-3 px-6 py-4 border-t border-neutral-200">
                            <button id="cancel-btn" type="button" class="px-4 py-2 text-sm text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50 focus:outline-none focus:ring-2 focus:ring-neutral-500 transition-colors">
                                Cancel
                            </button>
                            <button id="update-btn" type="submit" class="px-4 py-2 text-sm text-white bg-neutral-900 rounded-md hover:bg-neutral-800 focus:outline-none focus:ring-2 focus:ring-neutral-500 transition-colors">
                                Update Password
                            </button>
                        </div>
                    </div>
                `,
                buttons: [], // No buttons since we handle them in custom content
                closeOnBackdrop: true,
                closeOnEscape: true
            }).then(() => {
                // Initialize tab functionality after modal is shown
                this.initializeSettingsTabs();
            });
        } else {
            // Fallback for when popupManager is not available
            this.showFallbackSettings();
        }
    }

    initializeSettingsTabs() {
        // Wait a moment for the popup content to be fully rendered
        setTimeout(() => {
            // Get tab buttons and sections from within the modal
            const modal = document.querySelector('#settings-modal');
            if (!modal) {
                console.warn('Settings modal not found');
                return;
            }

            const tabButtons = modal.querySelectorAll('[data-tab]');
            const passwordSection = modal.querySelector('#password-section');
            const tokenSection = modal.querySelector('#token-section');
            const cloudSection = modal.querySelector('#cloud-section');
            const updateBtn = modal.querySelector('#update-btn');
            const closeBtn = modal.querySelector('#close-btn');
            const cancelBtn = modal.querySelector('#cancel-btn');

            console.log('Found tab buttons:', tabButtons.length);

            // Tab switching functionality
            tabButtons.forEach(button => {
                button.addEventListener('click', (e) => {
                    e.preventDefault();
                    const tabName = button.getAttribute('data-tab');
                    console.log('Tab clicked:', tabName);
                    this.switchSettingsTab(tabName);
                });
            });

            // Close button functionality
            if (closeBtn) {
                closeBtn.addEventListener('click', () => {
                    if (window.popupManager) {
                        window.popupManager.close();
                    }
                });
            }

            // Cancel button functionality
            if (cancelBtn) {
                cancelBtn.addEventListener('click', () => {
                    if (window.popupManager) {
                        window.popupManager.close();
                    }
                });
            }

            // Update button functionality
            if (updateBtn) {
                updateBtn.addEventListener('click', () => {
                    this.handlePasswordUpdate();
                });
            }

            // Generate button functionality (for token tab)
            const generateBtn = modal.querySelector('#generate-btn');
            if (generateBtn) {
                generateBtn.addEventListener('click', () => {
                    this.handleKeyGeneration();
                });
            }

        }, 100); // Small delay to ensure DOM is ready
    }

    switchSettingsTab(tabName) {
        // Get the modal context
        const modal = document.querySelector('#settings-modal');
        if (!modal) {
            console.warn('Settings modal not found for tab switching');
            return;
        }

        // Get all tab buttons and sections from within the modal
        const tabButtons = modal.querySelectorAll('[data-tab]');
        const passwordSection = modal.querySelector('#password-section');
        const tokenSection = modal.querySelector('#token-section');
        const cloudSection = modal.querySelector('#cloud-section');
        const updateBtn = modal.querySelector('#update-btn');

        console.log('Switching to tab:', tabName);

        // Hide all sections
        if (passwordSection) passwordSection.classList.add('hidden');
        if (tokenSection) tokenSection.classList.add('hidden');
        if (cloudSection) cloudSection.classList.add('hidden');

        // Remove active state from all buttons and reset to inactive state
        tabButtons.forEach(button => {
            button.classList.remove('text-neutral-900', 'border-b-2', 'border-neutral-900', 'bg-neutral-50');
            button.classList.add('text-neutral-600', 'hover:text-neutral-900', 'hover:bg-neutral-50', 'transition-colors');
        });

        // Show selected section and activate button
        switch (tabName) {
            case 'password':
                if (passwordSection) {
                    passwordSection.classList.remove('hidden');
                    console.log('Password section shown');
                }
                if (updateBtn) {
                    updateBtn.textContent = 'Update Password';
                    updateBtn.style.display = 'block';
                }
                break;
            case 'token':
                if (tokenSection) {
                    tokenSection.classList.remove('hidden');
                    console.log('Token section shown');
                }
                if (updateBtn) {
                    updateBtn.style.display = 'none'; // Hide update button for token tab
                }
                break;
            case 'cloud':
                if (cloudSection) {
                    cloudSection.classList.remove('hidden');
                    console.log('Cloud section shown');
                }
                if (updateBtn) {
                    updateBtn.style.display = 'none'; // Hide update button for cloud tab
                }
                break;
        }

        // Activate selected button
        const activeButton = modal.querySelector(`[data-tab="${tabName}"]`);
        if (activeButton) {
            activeButton.classList.remove('text-neutral-600', 'hover:text-neutral-900', 'hover:bg-neutral-50');
            activeButton.classList.add('text-neutral-900', 'border-b-2', 'border-neutral-900', 'bg-neutral-50');
            console.log('Active button styled:', tabName);
        }
    }

    handlePasswordUpdate() {
        const currentPassword = document.getElementById('current-password').value;
        const newPassword = document.getElementById('new-password').value;
        const confirmPassword = document.getElementById('confirm-password').value;

        // Validation
        if (!currentPassword || !newPassword || !confirmPassword) {
            this.showSettingsError('Please fill in all password fields');
            return;
        }

        if (newPassword !== confirmPassword) {
            this.showSettingsError('New passwords do not match');
            return;
        }

        if (newPassword.length < 12) {
            this.showSettingsError('Password must be at least 12 characters long');
            return;
        }

        // Simulate password update (in real app, this would call an API)
        console.log('Password update requested:', { currentPassword, newPassword });
        
        // Show success message
        if (window.popupManager) {
            window.popupManager.showSuccess({
                title: 'Password Updated',
                description: 'Your password has been successfully updated.',
                buttonText: 'OK'
            }).then(() => {
                // Close settings modal after success
                window.popupManager.close();
            });
        }
    }

    handleKeyGeneration() {
        const keyName = document.getElementById('key-name').value;
        
        if (!keyName) {
            this.showSettingsError('Please enter a key name');
            return;
        }

        // Simulate key generation (in real app, this would call an API)
        console.log('Key generation requested:', { keyName });
        
        // Show success message
        if (window.popupManager) {
            window.popupManager.showSuccess({
                title: 'Key Generated',
                description: `API key "${keyName}" has been generated and downloaded.`,
                buttonText: 'OK'
            });
        }

        // Clear the input
        document.getElementById('key-name').value = '';
    }

    showSettingsError(message) {
        if (window.popupManager) {
            window.popupManager.showError({
                title: 'Error',
                message: message,
                buttonText: 'OK'
            });
        } else {
            alert('Error: ' + message);
        }
    }

    resetSettingsToDefaults() {
        // Reset form values to defaults
        document.getElementById('ws-auto-reconnect').checked = true;
        document.getElementById('ws-heartbeat').checked = false;
        document.getElementById('ws-reconnect-interval').value = '3000';
        document.getElementById('ws-max-attempts').value = '5';
        
        document.getElementById('http-timeout').value = '15000';
        document.getElementById('http-retry').checked = true;
        document.getElementById('http-max-retries').value = '3';
        
        document.getElementById('app-debug').checked = false;
        document.getElementById('app-dark-mode').checked = false;
        document.getElementById('app-notifications').checked = true;
        document.getElementById('app-language').value = 'en';
        
        console.log('Settings reset to defaults');
    }

    saveSettings() {
        // Collect settings from form
        const settings = {
            websocket: {
                autoReconnect: document.getElementById('ws-auto-reconnect').checked,
                heartbeat: document.getElementById('ws-heartbeat').checked,
                reconnectInterval: parseInt(document.getElementById('ws-reconnect-interval').value),
                maxAttempts: parseInt(document.getElementById('ws-max-attempts').value)
            },
            http: {
                timeout: parseInt(document.getElementById('http-timeout').value),
                retry: document.getElementById('http-retry').checked,
                maxRetries: parseInt(document.getElementById('http-max-retries').value)
            },
            application: {
                debug: document.getElementById('app-debug').checked,
                darkMode: document.getElementById('app-dark-mode').checked,
                notifications: document.getElementById('app-notifications').checked,
                language: document.getElementById('app-language').value
            }
        };

        // Save to localStorage
        localStorage.setItem('app_settings', JSON.stringify(settings));
        
        // Apply settings
        this.applySettings(settings);
        
        console.log('Settings saved:', settings);
        
        // Show success message
        if (window.popupManager) {
            window.popupManager.showSuccess({
                title: 'Settings Saved',
                description: 'Your settings have been saved and applied successfully.',
                buttonText: 'OK'
            });
        }
    }

    applySettings(settings) {
        // Apply WebSocket settings
        if (this.sourceManager && this.sourceManager.getWebSocketClient) {
            const wsClient = this.sourceManager.getWebSocketClient();
            if (wsClient && settings.websocket) {
                // Update WebSocket client configuration
                console.log('Applying WebSocket settings:', settings.websocket);
            }
        }

        // Apply application settings
        if (settings.application) {
            // Apply dark mode
            if (settings.application.darkMode) {
                document.body.classList.add('dark-mode');
            } else {
                document.body.classList.remove('dark-mode');
            }

            // Apply debug mode
            if (settings.application.debug) {
                document.body.classList.add('debug-mode');
            } else {
                document.body.classList.remove('debug-mode');
            }
        }
    }

    showFallbackSettings() {
        // Simple alert fallback when popupManager is not available
        const settings = `
WebSocket Settings:
- Auto Reconnect: Enabled
- Heartbeat: Disabled
- Reconnect Interval: 3000ms
- Max Attempts: 5

HTTP Settings:
- Timeout: 15000ms
- Auto Retry: Enabled
- Max Retries: 3

Application Settings:
- Debug Mode: Disabled
- Dark Mode: Disabled
- Notifications: Enabled
- Language: English
        `;
        alert('Current Settings:\n\n' + settings + '\n\nPopup manager not available. Use popupManager for full settings interface.');
    }
    
    /**
     * Toggle collapsable section
     */
    toggleSection(sectionId) {
        const section = document.getElementById(sectionId);
        const content = document.getElementById(sectionId + '-content');
        const chevron = document.getElementById(sectionId + '-chevron');
        
        if (!section || !content || !chevron) {
            console.warn(`[SOURCE-UI] Section elements not found for: ${sectionId}`);
            return;
        }
        
        const isHidden = content.classList.contains('hidden');
        
        if (isHidden) {
            // Open section
            content.classList.remove('hidden');
            chevron.classList.remove('fa-chevron-right');
            chevron.classList.add('fa-chevron-down');
            console.log(`[SOURCE-UI] Opened section: ${sectionId}`);
        } else {
            // Close section
            content.classList.add('hidden');
            chevron.classList.remove('fa-chevron-down');
            chevron.classList.add('fa-chevron-right');
            console.log(`[SOURCE-UI] Closed section: ${sectionId}`);
        }
    }
    
    /**
     * Initialize collapsable sections with proper initial states
     */
    initializeCollapsibleSections() {
        // Priority section starts closed
        const priorityContent = document.getElementById('priority-section-content');
        const priorityChevron = document.getElementById('priority-section-chevron');
        if (priorityContent && priorityChevron) {
            priorityContent.classList.add('hidden');
            priorityChevron.classList.remove('fa-chevron-down');
            priorityChevron.classList.add('fa-chevron-right');
        }
        
        // Routing rules section starts open
        const routingContent = document.getElementById('routing-rules-section-content');
        const routingChevron = document.getElementById('routing-rules-section-chevron');
        if (routingContent && routingChevron) {
            routingContent.classList.remove('hidden');
            routingChevron.classList.remove('fa-chevron-right');
            routingChevron.classList.add('fa-chevron-down');
        }
        
        // Temporary routing rules section starts closed
        const tempRoutingContent = document.getElementById('temp-routing-rules-section-content');
        const tempRoutingChevron = document.getElementById('temp-routing-rules-section-chevron');
        if (tempRoutingContent && tempRoutingChevron) {
            tempRoutingContent.classList.add('hidden');
            tempRoutingChevron.classList.remove('fa-chevron-down');
        }
        
        console.log('[SOURCE-UI] Initialized collapsable sections');
    }
}

// Export for use in other modules
export { SourceUI };
export default SourceUI;

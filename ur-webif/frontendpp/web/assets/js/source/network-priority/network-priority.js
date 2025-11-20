/**
 * Network Priority Manager
 * Handles network priority UI interactions and data management
 */

class NetworkPriorityManager {
    constructor(sourceManager) {
        this.sourceManager = sourceManager;
        this.interfaces = [];
        this.routingRules = [];
        this.tempRoutingRules = []; // New array for temporary rules
        this.currentStatistics = null;
        this.selectedRules = new Set();
        this.selectedTempRules = new Set(); // New set for temporary rule selection
        this.loading = false;
        this.isConnected = false;
        this.lastDataReceived = null;
        this.connectionTimeout = null;
        
        this.init();
    }
    
    init() {
        console.log('[NETWORK-PRIORITY] Initializing Network Priority Manager...');
        this.setupEventListeners();
        this.loadInitialData();
    }
    
    setupEventListeners() {
        // Listen for WebSocket messages
        document.addEventListener('websocketMessage', (e) => {
            const message = e.detail;
            if (message.type === 'network_priority_update') {
                this.handleNetworkPriorityUpdate(message.data);
            }
        });
        
        // Listen for network priority data requests
        document.addEventListener('networkPriorityDataReceived', (e) => {
            this.handleDataReceived(e.detail.data);
        });
        
        // Temporary routing rules buttons
        document.addEventListener('click', (e) => {
            if (e.target.id === 'convert-to-static-btn' || e.target.closest('#convert-to-static-btn')) {
                this.convertTempRulesToStatic();
            }
        });
        
        console.log('[NETWORK-PRIORITY] Event listeners setup complete');
    }
    
    async loadInitialData() {
        try {
            console.log('[NETWORK-PRIORITY] Loading initial network priority data...');
            this.setLoading(true);
            
            // Don't load mock data - wait for real WebSocket data
            // The loading UI will show skeleton screens until data arrives
            console.log('[NETWORK-PRIORITY] Waiting for real WebSocket data...');
            
            // Initialize empty data arrays - will be populated by WebSocket messages
            this.interfaces = [];
            this.routingRules = [];
            this.tempRoutingRules = [];
            
            // Update connection status to show we're waiting for real data
            this.updateConnectionStatus(false);
            
            // Show loading states in the UI
            if (window.dataLoadingUI) {
                window.dataLoadingUI.showAllLoadingStates();
            }
            
            console.log('[NETWORK-PRIORITY] Initial data setup complete - waiting for WebSocket data');
            
        } catch (error) {
            console.error('[NETWORK-PRIORITY] Failed to initialize:', error);
            this.updateConnectionStatus(false);
            
            // Show error state in loading UI
            if (window.dataLoadingUI) {
                window.dataLoadingUI.showError('interfaces', error.message);
                window.dataLoadingUI.showError('routingRules', error.message);
                window.dataLoadingUI.showError('tempRoutingRules', error.message);
            }
        } finally {
            this.setLoading(false);
        }
    }
    
    async loadInterfacesData() {
        try {
            // This method is no longer used - data comes from WebSocket
            console.log('[NETWORK-PRIORITY] loadInterfacesData() deprecated - using WebSocket data');
            return this.interfaces;
        } catch (error) {
            console.error('[NETWORK-PRIORITY] Failed to load interfaces data:', error);
            throw error;
        }
    }
    
    async loadRoutingRules() {
        try {
            // This method is no longer used - data comes from WebSocket
            console.log('[NETWORK-PRIORITY] loadRoutingRules() deprecated - using WebSocket data');
            return this.routingRules;
        } catch (error) {
            console.error('[NETWORK-PRIORITY] Failed to load routing rules:', error);
            throw error;
        }
    }
    
    /**
     * Check if DOM elements are ready for updates
     */
    isDOMReady() {
        const interfacesContainer = document.getElementById('interfaces-container');
        const routingRulesTbody = document.getElementById('routing-rules-tbody');
        const prioritySection = document.getElementById('priority-section');
        
        return !!(interfacesContainer && routingRulesTbody && prioritySection);
    }
    
    handleNetworkPriorityUpdate(data) {
        console.log('[NETWORK-PRIORITY] Received network priority update:', data);
        
        // Ensure DOM is ready before processing
        if (!this.isDOMReady()) {
            console.log('[NETWORK-PRIORITY] DOM not ready, retrying in 50ms...');
            setTimeout(() => {
                this.handleNetworkPriorityUpdate(data);
            }, 50);
            return;
        }
        
        let hasUpdates = false;
        
        // Update connection status to connected since we're receiving data
        this.updateConnectionStatus(true);
        this.setLoading(false);
        
        // Update interfaces if provided
        if (data.interfaces) {
            console.log('[NETWORK-PRIORITY] Processing interfaces update:', data.interfaces);
            const oldInterfaces = [...this.interfaces];
            
            // Check if interfaces data actually changed
            const interfacesChanged = this.hasInterfacesChanged(oldInterfaces, data.interfaces);
            
            if (interfacesChanged) {
                this.interfaces = data.interfaces;
                console.log('[NETWORK-PRIORITY] Interfaces data changed, updating display');
                
                // Update interfaces section only
                this.updateInterfacesDisplay();
                this.updateSectionLastUpdate('interfaces', data.lastUpdated);
                
                // Hide loading state for interfaces
                if (window.dataLoadingUI) {
                    window.dataLoadingUI.hideLoading('interfaces');
                }
                
                // Check for status changes
                const statusChanges = this.detectInterfaceStatusChanges(oldInterfaces, this.interfaces);
                if (statusChanges.length > 0) {
                    statusChanges.forEach(change => {
                        this.showNotification(`Interface ${change.name} status changed to ${change.status}`, 'warning');
                    });
                }
                
                console.log('[NETWORK-PRIORITY] Interfaces updated via WebSocket:', this.interfaces.length, 'interfaces');
                hasUpdates = true;
            } else {
                console.log('[NETWORK-PRIORITY] Interfaces data unchanged, skipping update');
            }
        } else {
            console.log('[NETWORK-PRIORITY] No interfaces data in this update');
        }
        
        // Update routing rules if provided
        if (data.routingRules) {
            console.log('[NETWORK-PRIORITY] Processing routing rules update:', data.routingRules);
            const oldRules = [...this.routingRules];
            
            // Check if routing rules data actually changed
            const rulesChanged = this.hasRoutingRulesChanged(oldRules, data.routingRules);
            
            if (rulesChanged) {
                this.routingRules = data.routingRules;
                console.log('[NETWORK-PRIORITY] Routing rules data changed, updating display');
                
                // Update routing rules section only
                this.updateRoutingRulesDisplay();
                this.updateSectionLastUpdate('routing-rules', data.lastUpdated);
                
                // Hide loading state for routing rules
                if (window.dataLoadingUI) {
                    window.dataLoadingUI.hideLoading('routingRules');
                }
                
                // Check for rule changes
                const ruleChanges = this.detectRuleChanges(oldRules, this.routingRules);
                if (ruleChanges.length > 0) {
                    ruleChanges.forEach(change => {
                        this.showNotification(`Routing rule ${change.id} ${change.action}`, 'info');
                    });
                }
                
                console.log('[NETWORK-PRIORITY] Routing rules updated via WebSocket:', this.routingRules.length, 'rules');
                hasUpdates = true;
            } else {
                console.log('[NETWORK-PRIORITY] Routing rules data unchanged, skipping update');
            }
        } else {
            console.log('[NETWORK-PRIORITY] No routing rules data in this update');
        }
        
        // Update statistics if provided
        if (data.statistics) {
            console.log('[NETWORK-PRIORITY] Processing statistics update:', data.statistics);
            
            // Check if statistics data actually changed
            const statsChanged = this.hasStatisticsChanged(this.currentStatistics, data.statistics);
            
            if (statsChanged) {
                console.log('[NETWORK-PRIORITY] Statistics data changed, updating display');
                this.currentStatistics = data.statistics;
                
                // Update statistics section only
                this.updateStatistics(data.statistics);
                this.updateSectionLastUpdate('statistics', data.lastUpdated);
                hasUpdates = true;
            } else {
                console.log('[NETWORK-PRIORITY] Statistics data unchanged, skipping update');
            }
        }
        
        // Show connection indicator if we have updates
        if (hasUpdates) {
            this.showConnectionIndicator();
        }
    }
    
    /**
     * Update last update timestamp for a specific section
     */
    updateSectionLastUpdate(section, timestamp) {
        const lastUpdateElement = document.querySelector(`[data-last-update="${section}"]`);
        if (lastUpdateElement) {
            const formattedTime = timestamp || new Date().toLocaleString();
            lastUpdateElement.textContent = `Last updated: ${formattedTime}`;
            
            // Add brief highlight animation
            lastUpdateElement.classList.add('text-green-600', 'font-medium');
            setTimeout(() => {
                lastUpdateElement.classList.remove('text-green-600', 'font-medium');
                lastUpdateElement.classList.add('text-neutral-500');
            }, 2000);
            
            console.log(`[NETWORK-PRIORITY] ${section} section last updated: ${formattedTime}`);
        } else {
            console.warn(`[NETWORK-PRIORITY] Last update element not found for section: ${section}`);
        }
    }
    
    /**
     * Check if interfaces data has changed
     */
    hasInterfacesChanged(oldInterfaces, newInterfaces) {
        if (oldInterfaces.length !== newInterfaces.length) {
            return true;
        }
        
        // Compare interfaces by ID and key properties (excluding 'id' field from comparison)
        for (let i = 0; i < oldInterfaces.length; i++) {
            const oldIface = oldInterfaces[i];
            const newIface = newInterfaces.find(iface => iface.id === oldIface.id);
            
            if (!newIface) {
                return true; // Interface was removed
            }
            
            // Check key properties for changes (ignoring 'id' field)
            if (oldIface.status !== newIface.status ||
                oldIface.priority !== newIface.priority ||
                oldIface.ip !== newIface.ip ||
                oldIface.gateway !== newIface.gateway ||
                oldIface.speed !== newIface.speed ||
                oldIface.isDefault !== newIface.isDefault ||
                oldIface.name !== newIface.name ||
                oldIface.type !== newIface.type ||
                oldIface.metric !== newIface.metric ||
                oldIface.netmask !== newIface.netmask) {
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Check if routing rules data has changed
     */
    hasRoutingRulesChanged(oldRules, newRules) {
        if (oldRules.length !== newRules.length) {
            return true;
        }
        
        // Compare rules by ID and key properties (excluding 'id' field from comparison)
        for (let i = 0; i < oldRules.length; i++) {
            const oldRule = oldRules[i];
            const newRule = newRules.find(rule => rule.id === oldRule.id);
            
            if (!newRule) {
                return true; // Rule was removed
            }
            
            // Check key properties for changes (ignoring 'id' field)
            if (oldRule.priority !== newRule.priority ||
                oldRule.destination !== newRule.destination ||
                oldRule.gateway !== newRule.gateway ||
                oldRule.interface !== newRule.interface ||
                oldRule.metric !== newRule.metric ||
                oldRule.status !== newRule.status ||
                oldRule.type !== newRule.type ||
                oldRule.table !== newRule.table ||
                oldRule.isDefault !== newRule.isDefault) {
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Check if statistics data has changed
     */
    hasStatisticsChanged(oldStats, newStats) {
        if (!oldStats || !newStats) {
            return true;
        }
        
        return oldStats.total !== newStats.total ||
               oldStats.online !== newStats.online ||
               oldStats.offline !== newStats.offline ||
               oldStats.activeRules !== newStats.activeRules ||
               oldStats.lastUpdated !== newStats.lastUpdated;
    }
    
    /**
     * Detect interface status changes
     */
    detectInterfaceStatusChanges(oldInterfaces, newInterfaces) {
        const changes = [];
        
        newInterfaces.forEach(newIface => {
            const oldIface = oldInterfaces.find(iface => iface.id === newIface.id);
            if (oldIface && oldIface.status !== newIface.status) {
                changes.push({
                    name: newIface.name,
                    status: newIface.status
                });
            }
        });
        return changes;
    }
    
    /**
     * Detect routing rule changes
     */
    detectRuleChanges(oldRules, newRules) {
        const oldIds = new Set(oldRules.map(r => r.id));
        const newIds = new Set(newRules.map(r => r.id));
        
        const added = newRules.filter(r => !oldIds.has(r.id)).length;
        const removed = oldRules.filter(r => !newIds.has(r.id)).length;
        
        let modified = 0;
        newRules.forEach(newRule => {
            const oldRule = oldRules.find(r => r.id === newRule.id);
            if (oldRule && JSON.stringify(oldRule) !== JSON.stringify(newRule)) {
                modified++;
            }
        });
        
        return { added, removed, modified };
    }
    
    /**
     * Show real-time update indicator
     */
    showRealtimeIndicator() {
        const indicator = document.getElementById('realtime-indicator');
        if (indicator) {
            indicator.classList.add('animate-pulse');
            indicator.innerHTML = '<i class="fa-solid fa-wifi mr-1"></i> Live';
            
            setTimeout(() => {
                indicator.classList.remove('animate-pulse');
                indicator.innerHTML = '<i class="fa-solid fa-wifi mr-1"></i> Connected';
            }, 2000);
        }
    }
    
    handleDataReceived(data) {
        console.log('[NETWORK-PRIORITY] Data received:', data);
        
        // Update connection status to connected since we're receiving data
        this.updateConnectionStatus(true);
        this.setLoading(false);
        
        if (data.interfaces) {
            this.interfaces = data.interfaces;
            this.updateInterfacesDisplay();
        }
        
        if (data.routingRules) {
            this.routingRules = data.routingRules;
            this.updateRoutingRulesDisplay();
        }
        
        if (data.statistics) {
            this.updateStatistics(data.statistics);
        }
        
        // Show connection indicator
        this.showConnectionIndicator();
    }
    
    updateInterfacesDisplay() {
        const container = document.getElementById('interfaces-container');
        if (!container) {
            console.warn('[NETWORK-PRIORITY] Interfaces container not found');
            return;
        }
        
        // Store scroll position before update
        const scrollPosition = window.pageYOffset || document.documentElement.scrollTop;
        
        // Show empty state if no interfaces
        if (this.interfaces.length === 0) {
            container.innerHTML = `
                <div class="text-center py-8">
                    <i class="fa-solid fa-network-wired text-4xl text-neutral-300 mb-4"></i>
                    <p class="text-neutral-500">No interfaces available</p>
                </div>
            `;
            // Restore scroll position
            window.scrollTo(0, scrollPosition);
            return;
        }
        
        const totalInterfaces = this.interfaces.length;
        const onlineInterfaces = this.interfaces.filter(i => i.status === 'online').length;
        const offlineInterfaces = totalInterfaces - onlineInterfaces;
        
        // Update statistics
        this.updateStat('total', totalInterfaces);
        this.updateStat('online', onlineInterfaces);
        this.updateStat('offline', offlineInterfaces);
        
        // Update interfaces list without fade effect to prevent blinking
        container.innerHTML = this.interfaces.map((iface, index) => this.createInterfaceHTML(iface, index)).join('');
        
        // Re-attach event listeners for new elements
        this.attachInterfaceEventListeners();
        
        // Restore scroll position after DOM update
        window.scrollTo(0, scrollPosition);
        
        console.log('[NETWORK-PRIORITY] Interface display updated:', totalInterfaces, 'interfaces');
    }
    
    createInterfaceHTML(iface, index) {
        const statusColor = iface.status === 'online' ? 'green' : 'red';
        const statusIcon = iface.status === 'online' ? 'check-circle' : 'times-circle';
        const dataRate = iface.dataRate || iface.speed || 'N/A';
        const latency = iface.latency || 'N/A';
        const priorityBadge = this.getPriorityBadge(iface.priority);
        const connectionQuality = this.getConnectionQuality(iface);
        
        return `
            <div class="bg-white border border-neutral-200 rounded-lg p-4 hover:shadow-md transition-all duration-200 hover:border-blue-300" data-interface-id="${iface.id}">
                <div class="flex flex-col lg:flex-row lg:items-center lg:justify-between gap-4">
                    <div class="flex items-center space-x-4 min-w-0 flex-1">
                        <div class="w-10 h-10 rounded-full bg-${statusColor}-100 flex items-center justify-center flex-shrink-0">
                            <i class="fa-solid fa-${this.getInterfaceIcon(iface.type)} text-${statusColor}-600"></i>
                        </div>
                        <div class="flex-1 min-w-0">
                            <div class="font-medium text-neutral-900 flex items-center space-x-2 flex-wrap">
                                <span class="truncate">${iface.name}</span>
                                ${priorityBadge}
                                <i class="fa-solid fa-${statusIcon} text-xs text-${statusColor}-500" title="Status: ${iface.status}"></i>
                                ${connectionQuality}
                            </div>
                            <div class="text-sm text-neutral-500">${iface.type}</div>
                        </div>
                    </div>
                    <div class="flex items-center space-x-4 flex-shrink-0">
                        <div class="text-right">
                            <div class="text-sm font-medium text-neutral-900">Priority: ${iface.priority}</div>
                            <div class="text-sm text-neutral-500">${iface.ip || 'N/A'}</div>
                        </div>
                        <div class="flex flex-col space-y-1">
                            <button class="p-2 text-neutral-400 hover:text-blue-600 hover:bg-blue-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.moveInterfaceUp(${index})" ${index === 0 ? 'disabled' : ''} title="Move up">
                                <i class="fa-solid fa-chevron-up"></i>
                            </button>
                            <button class="p-2 text-neutral-400 hover:text-blue-600 hover:bg-blue-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.moveInterfaceDown(${index})" ${index === this.interfaces.length - 1 ? 'disabled' : ''} title="Move down">
                                <i class="fa-solid fa-chevron-down"></i>
                            </button>
                        </div>
                        <div class="flex flex-col space-y-1">
                            <button class="p-2 text-neutral-400 hover:text-blue-600 hover:bg-blue-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.showInterfaceDetails('${iface.id}')" title="View details">
                                <i class="fa-solid fa-info-circle"></i>
                            </button>
                            <button class="p-2 text-neutral-400 hover:text-orange-600 hover:bg-orange-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.editInterface('${iface.id}')" title="Edit interface">
                                <i class="fa-solid fa-edit"></i>
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Get priority badge HTML based on priority value
     */
    getPriorityBadge(priority) {
        if (priority === 1) {
            return '<i class="fa-solid fa-crown text-xs text-yellow-500" title="Highest Priority"></i>';
        } else if (priority <= 3) {
            return '<i class="fa-solid fa-star text-xs text-orange-500" title="High Priority"></i>';
        } else if (priority <= 10) {
            return '<i class="fa-solid fa-arrow-up text-xs text-blue-500" title="Medium Priority"></i>';
        }
        return '';
    }
    
    /**
     * Get connection quality indicator based on interface metrics
     */
    getConnectionQuality(iface) {
        if (iface.status !== 'online') return '';
        
        const latency = parseFloat(iface.latency) || 0;
        const dataRate = parseFloat(iface.dataRate) || 0;
        
        if (latency < 10 && dataRate > 100) {
            return '<i class="fa-solid fa-signal text-xs text-green-500" title="Excellent Connection"></i>';
        } else if (latency < 50 && dataRate > 10) {
            return '<i class="fa-solid fa-signal text-xs text-yellow-500" title="Good Connection"></i>';
        } else if (latency < 100) {
            return '<i class="fa-solid fa-signal text-xs text-orange-500" title="Fair Connection"></i>';
        }
        return '<i class="fa-solid fa-signal text-xs text-red-500" title="Poor Connection"></i>';
    }
    
    /**
     * Get formatted interface speed from WebSocket data
     */
    getInterfaceSpeed(iface) {
        if (iface.speed) return iface.speed;
        if (iface.dataRate) return iface.dataRate;
        if (iface.bandwidth) return iface.bandwidth;
        return 'N/A';
    }
    
    /**
     * Get formatted interface uptime from WebSocket data
     */
    getInterfaceUptime(iface) {
        if (iface.uptime) return `Uptime: ${iface.uptime}`;
        if (iface.lastSeen) return `Last seen: ${iface.lastSeen}`;
        return 'Just now';
    }
    
    /**
     * Edit interface configuration
     */
    editInterface(interfaceId) {
        console.log('[NETWORK-PRIORITY] Editing interface:', interfaceId);
        const iface = this.interfaces.find(i => i.id === interfaceId);
        if (iface) {
            // Show interface edit popup
            this.showInterfaceEditPopup(iface);
        }
    }
    
    /**
     * Show interface edit popup
     */
    showInterfaceEditPopup(iface) {
        console.log('[NETWORK-PRIORITY] Showing interface edit popup for:', iface);
        // This will be implemented in network-priority-ui.js
        if (window.networkPriorityUI) {
            window.networkPriorityUI.showInterfaceEditPopup(iface);
        }
    }
    
    /**
     * Force refresh routing rules display
     */
    forceRefreshRoutingRules() {
        console.log('[NETWORK-PRIORITY] Force refreshing routing rules display...');
        
        // Ensure we have data
        if (this.routingRules.length === 0) {
            console.log('[NETWORK-PRIORITY] No routing rules data to refresh');
            return;
        }
        
        // Force DOM check and update
        if (!this.isDOMReady()) {
            console.log('[NETWORK-PRIORITY] DOM not ready for force refresh, retrying...');
            setTimeout(() => {
                this.forceRefreshRoutingRules();
            }, 100);
            return;
        }
        
        // Update display
        this.updateRoutingRulesDisplay();
    }
    
    updateRoutingRulesDisplay() {
        console.log('[NETWORK-PRIORITY] Updating routing rules display...');
        
        const tbody = document.getElementById('routing-rules-tbody');
        if (!tbody) {
            console.warn('[NETWORK-PRIORITY] Routing rules tbody not found - retrying in 100ms...');
            // Retry after a short delay to allow DOM to be ready
            setTimeout(() => {
                this.updateRoutingRulesDisplay();
            }, 100);
            return;
        }
        
        // Store scroll position before update
        const scrollPosition = window.pageYOffset || document.documentElement.scrollTop;
        
        // Show empty state if no rules
        if (this.routingRules.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="6" class="text-center py-8 text-neutral-500">
                        <i class="fa-solid fa-route text-4xl text-neutral-300 mb-4 block"></i>
                        No routing rules configured
                    </td>
                </tr>
            `;
            // Restore scroll position
            window.scrollTo(0, scrollPosition);
            return;
        }
        
        console.log('[NETWORK-PRIORITY] Rendering', this.routingRules.length, 'routing rules');
        
        // Update routing rules without fade effect to prevent blinking
        const rulesHTML = this.routingRules.map(rule => this.createRoutingRuleHTML(rule)).join('');
        console.log('[NETWORK-PRIORITY] Generated HTML for rules:', rulesHTML.substring(0, 200) + '...');
        tbody.innerHTML = rulesHTML;
        
        // Re-attach event listeners for new elements
        this.attachRoutingRuleEventListeners();
        
        // Restore scroll position after DOM update
        window.scrollTo(0, scrollPosition);
        
        console.log('[NETWORK-PRIORITY] Routing rules display updated successfully');
    }
    
    attachInterfaceEventListeners() {
        // Add click handlers to interface cards
        document.querySelectorAll('[data-interface-id]').forEach(element => {
            element.addEventListener('click', (e) => {
                if (!e.target.closest('button')) {
                    const interfaceId = element.dataset.interfaceId;
                    this.showInterfaceDetails(interfaceId);
                }
            });
        });
    }
    
    /**
     * Attach event listeners to routing rule elements
     */
    attachRoutingRuleEventListeners() {
        // Add change handlers to checkboxes
        document.querySelectorAll('[data-rule-id]').forEach(checkbox => {
            checkbox.addEventListener('change', (e) => {
                const ruleId = parseInt(e.target.dataset.ruleId);
                this.toggleRuleSelection(ruleId, e.target.checked);
            });
        });
    }
    
    /**
     * Show detailed information about an interface
     */
    showInterfaceDetails(interfaceId) {
        const iface = this.interfaces.find(i => i.id === interfaceId);
        if (iface) {
            console.log('[NETWORK-PRIORITY] Interface details requested:', iface);
            // Could show a modal or expand details
            this.showNotification(`Interface ${iface.name} (${iface.type}) - Status: ${iface.status}`, 'info');
        }
    }
    
    /**
     * Toggle rule selection for bulk operations
     */
    toggleRuleSelection(ruleId, selected) {
        if (selected) {
            if (!this.selectedRules) this.selectedRules = new Set();
            this.selectedRules.add(ruleId);
        } else {
            if (this.selectedRules) this.selectedRules.delete(ruleId);
        }
        console.log('[NETWORK-PRIORITY] Rule selection updated:', ruleId, selected);
    }
    
    createRoutingRuleHTML(rule) {
        const statusColor = rule.status === 'active' ? 'green' : 'red';
        const statusIcon = rule.status === 'active' ? 'check' : 'times';
        const lastMatched = rule.lastMatched || 'Never';
        const matchCount = rule.matchCount || 0;
        const priorityBadge = this.getRulePriorityBadge(rule);
        const ruleType = this.getRuleType(rule);
        const ruleEfficiency = this.getRuleEfficiency(rule);
        
        return `
            <tr class="hover:bg-neutral-50 transition-colors duration-150" data-rule-id="${rule.id}">
                <td class="px-4 py-3">
                    <input type="checkbox" class="rounded border-neutral-300" data-rule-id="${rule.id}">
                </td>
                <td class="px-4 py-3 text-sm font-medium text-neutral-900">
                    <div class="flex items-center space-x-2">
                        <span>${rule.priority}</span>
                        ${priorityBadge}
                        ${ruleType}
                    </div>
                    ${ruleEfficiency}
                </td>
                <td class="px-4 py-3 text-sm text-neutral-600 font-mono">
                    <div class="flex items-center space-x-2">
                        <span>${rule.destination}</span>
                        ${this.isDefaultRoute(rule.destination) ? '<i class="fa-solid fa-globe text-xs text-blue-500" title="Default Route"></i>' : ''}
                    </div>
                </td>
                <td class="px-4 py-3 text-sm text-neutral-600 font-mono">
                    <div class="flex items-center space-x-2">
                        <span>${rule.gateway}</span>
                        ${this.isLocalGateway(rule.gateway) ? '<i class="fa-solid fa-home text-xs text-green-500" title="Local Gateway"></i>' : ''}
                    </div>
                </td>
                <td class="px-4 py-3 text-sm text-neutral-600">
                    <div class="flex items-center space-x-2">
                        <span>${rule.interface}</span>
                        ${this.getInterfaceStatus(rule.interface)}
                    </div>
                </td>
                <td class="px-4 py-3 text-sm text-neutral-600">
                    <div class="flex items-center space-x-2">
                        <span>${rule.metric}</span>
                        ${rule.metric < 100 ? '<i class="fa-solid fa-bolt text-xs text-green-500" title="High Priority"></i>' : ''}
                        ${rule.metric > 500 ? '<i class="fa-solid fa-turtle text-xs text-orange-500" title="Low Priority"></i>' : ''}
                    </div>
                </td>
                <td class="px-4 py-3">
                    <div class="flex items-center space-x-2">
                        <span class="px-2 py-1 text-xs font-medium rounded-full bg-${statusColor}-100 text-${statusColor}-800">
                            <i class="fa-solid fa-${statusIcon} mr-1"></i>
                            ${rule.status}
                        </span>
                        ${matchCount > 0 ? `<span class="text-xs text-neutral-500">${matchCount} hits</span>` : ''}
                        ${rule.isDefault ? '<span class="text-xs text-yellow-600">Default</span>' : ''}
                    </div>
                    ${lastMatched !== 'Never' ? `<div class="text-xs text-neutral-400 mt-1">Last: ${lastMatched}</div>` : ''}
                </td>
                <td class="px-4 py-3">
                    <div class="flex items-center space-x-1">
                        <button class="p-1 text-neutral-400 hover:text-blue-600 hover:bg-blue-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.editRule(${rule.id})" title="Edit rule">
                            <i class="fa-solid fa-edit"></i>
                        </button>
                        <button class="p-1 text-neutral-400 hover:text-green-600 hover:bg-green-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.duplicateRule(${rule.id})" title="Duplicate rule">
                            <i class="fa-solid fa-copy"></i>
                        </button>
                        <button class="p-1 text-neutral-400 hover:text-red-600 hover:bg-red-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.deleteRule(${rule.id})" title="Delete rule">
                            <i class="fa-solid fa-trash"></i>
                        </button>
                        <button class="p-1 text-neutral-400 hover:text-purple-600 hover:bg-purple-50 rounded transition-all duration-200" onclick="window.networkPriorityManager.toggleRuleStatus(${rule.id})" title="Toggle status">
                            <i class="fa-solid fa-power-off"></i>
                        </button>
                    </div>
                </td>
            </tr>
        `;
    }
    
    /**
     * Get rule priority badge HTML based on priority value
     */
    getRulePriorityBadge(rule) {
        if (rule.priority === 1) {
            return '<i class="fa-solid fa-crown text-xs text-yellow-500" title="Highest Priority"></i>';
        } else if (rule.priority <= 3) {
            return '<i class="fa-solid fa-star text-xs text-orange-500" title="High Priority"></i>';
        } else if (rule.priority <= 10) {
            return '<i class="fa-solid fa-arrow-up text-xs text-blue-500" title="Medium Priority"></i>';
        }
        return '';
    }
    
    /**
     * Get rule type HTML based on rule type
     */
    getRuleType(rule) {
        if (rule.type === 'static') {
            return '<i class="fa-solid fa-lock text-xs text-blue-500" title="Static Rule"></i>';
        } else if (rule.type === 'dynamic') {
            return '<i class="fa-solid fa-sync text-xs text-green-500" title="Dynamic Rule"></i>';
        }
        return '';
    }
    
    /**
     * Get rule efficiency HTML based on rule efficiency
     */
    getRuleEfficiency(rule) {
        if (rule.efficiency > 80) {
            return '<i class="fa-solid fa-rocket text-xs text-green-500" title="High Efficiency"></i>';
        } else if (rule.efficiency > 50) {
            return '<i class="fa-solid fa-trophy text-xs text-orange-500" title="Medium Efficiency"></i>';
        }
        return '';
    }
    
    /**
     * Check if route is default route
     */
    isDefaultRoute(destination) {
        return destination === '0.0.0.0/0' || destination === 'default';
    }
    
    /**
     * Check if gateway is local
     */
    isLocalGateway(gateway) {
        if (!gateway) return false;
        // Check if gateway is in local network ranges
        const localRanges = ['192.168.', '10.', '172.16.', '172.17.', '172.18.', '172.19.', 
                           '172.20.', '172.21.', '172.22.', '172.23.', '172.24.', '172.25.',
                           '172.26.', '172.27.', '172.28.', '172.29.', '172.30.', '172.31.'];
        return localRanges.some(range => gateway.startsWith(range));
    }
    
    /**
     * Get interface status indicator
     */
    getInterfaceStatus(interfaceName) {
        const iface = this.interfaces.find(i => i.name === interfaceName || i.id === interfaceName);
        if (!iface) return '';
        
        if (iface.status === 'online') {
            return '<i class="fa-solid fa-circle text-xs text-green-500" title="Interface Online"></i>';
        } else {
            return '<i class="fa-solid fa-circle text-xs text-red-500" title="Interface Offline"></i>';
        }
    }
    
    /**
     * Toggle rule status
     */
    toggleRuleStatus(ruleId) {
        console.log('[NETWORK-PRIORITY] Toggling rule status:', ruleId);
        const rule = this.routingRules.find(r => r.id === ruleId);
        if (rule) {
            rule.status = rule.status === 'active' ? 'inactive' : 'active';
            this.updateRoutingRulesDisplay();
            this.showNotification(`Rule ${ruleId} status changed to ${rule.status}`, 'info');
        }
    }
    
    /**
     * Update statistics display
     */
    updateStatistics(statistics) {
        console.log('[NETWORK-PRIORITY] Updating statistics display:', statistics);
        
        // Update statistics with animation
        this.updateStat('total', statistics.total || 0);
        this.updateStat('online', statistics.online || 0);
        this.updateStat('offline', statistics.offline || 0);
        
        // Update active rules count if element exists
        const activeRulesElement = document.querySelector('[data-stat="activeRules"]');
        if (activeRulesElement) {
            activeRulesElement.textContent = statistics.activeRules || 0;
        }
    }
    
    /**
     * Update last updated time
     */
    updateLastUpdated(lastUpdated) {
        console.log('[NETWORK-PRIORITY] Updating last updated time:', lastUpdated);
        
        const lastUpdatedElement = document.querySelector('[data-last-updated]');
        if (lastUpdatedElement) {
            lastUpdatedElement.textContent = lastUpdated;
        }
    }
    
    updateStat(statName, value) {
        const element = document.querySelector(`[data-stat="${statName}"]`);
        if (element) {
            element.textContent = value;
        }
    }
    
    updateConnectionStatus(connected) {
        const statusDot = document.getElementById('priority-status-dot');
        const statusText = document.getElementById('priority-connection-text');
        
        if (statusDot && statusText) {
            if (connected) {
                // Use setAttribute to avoid read-only property issues
                try {
                    statusDot.setAttribute('class', 'w-2 h-2 bg-green-500 rounded-full');
                } catch (error) {
                    console.warn('[NETWORK-PRIORITY] Could not set status dot class:', error);
                }
                statusText.textContent = 'Connected';
                this.lastDataReceived = Date.now();
                
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
                    console.warn('[NETWORK-PRIORITY] Could not set status dot class:', error);
                }
                statusText.textContent = 'Disconnected';
                
                // Clear timeout when disconnected
                if (this.connectionTimeout) {
                    clearTimeout(this.connectionTimeout);
                    this.connectionTimeout = null;
                }
            }
        }
        
        this.isConnected = connected;
        console.log('[NETWORK-PRIORITY] Connection status updated:', connected ? 'Connected' : 'Disconnected');
    }
    
    /**
     * Check if connection is stale (no data received for too long)
     */
    checkConnectionStaleness() {
        const now = Date.now();
        const timeSinceLastData = now - this.lastDataReceived;
        
        if (timeSinceLastData > 15000) { // 15 seconds without data
            console.log('[NETWORK-PRIORITY] Connection appears stale, marking as disconnected');
            this.updateConnectionStatus(false);
            this.setLoading(true);
        } else {
            // Still receiving data, reset the timeout
            this.connectionTimeout = setTimeout(() => {
                this.checkConnectionStaleness();
            }, 10000);
        }
    }
    
    setLoading(loading) {
        this.isLoading = loading;
        const refreshIcon = document.getElementById('priority-refresh-icon');
        if (refreshIcon) {
            // Use setAttribute to avoid read-only property issues
            try {
                if (loading) {
                    refreshIcon.setAttribute('class', 'fa-solid fa-refresh animate-spin');
                } else {
                    refreshIcon.setAttribute('class', 'fa-solid fa-refresh');
                }
            } catch (error) {
                console.warn('[NETWORK-PRIORITY] Could not set loading icon class:', error);
                // Fallback: try direct className assignment
                try {
                    if (loading) {
                        refreshIcon.className = 'fa-solid fa-refresh animate-spin';
                    } else {
                        refreshIcon.className = 'fa-solid fa-refresh';
                    }
                } catch (fallbackError) {
                    console.warn('[NETWORK-PRIORITY] Could not set loading icon class with fallback:', fallbackError);
                }
            }
        }
    }
    
    getInterfaceIcon(type) {
        const icons = {
            'wired': 'ethernet',
            'wireless': 'wifi',
            'cellular': 'signal'
        };
        return icons[type] || 'network-wired';
    }
    
    moveInterfaceUp(index) {
        if (index > 0) {
            [this.interfaces[index], this.interfaces[index - 1]] = 
            [this.interfaces[index - 1], this.interfaces[index]];
            
            // Update priorities
            this.interfaces.forEach((iface, i) => {
                iface.priority = i + 1;
            });
            
            this.updateInterfacesDisplay();
        }
    }
    
    moveInterfaceDown(index) {
        if (index < this.interfaces.length - 1) {
            [this.interfaces[index], this.interfaces[index + 1]] = 
            [this.interfaces[index + 1], this.interfaces[index]];
            
            // Update priorities
            this.interfaces.forEach((iface, i) => {
                iface.priority = i + 1;
            });
            
            this.updateInterfacesDisplay();
        }
    }
    
    deleteRule(ruleId) {
        console.log('[NETWORK-PRIORITY] Deleting rule:', ruleId);
        const ruleIndex = this.routingRules.findIndex(r => r.id === ruleId);
        if (ruleIndex !== -1) {
            this.routingRules.splice(ruleIndex, 1);
            this.updateRoutingRulesDisplay();
        }
    }
    
    /**
     * Duplicate an existing routing rule
     */
    duplicateRule(ruleId) {
        console.log('[NETWORK-PRIORITY] Duplicating rule:', ruleId);
        const rule = this.routingRules.find(r => r.id === ruleId);
        if (rule) {
            const duplicatedRule = {
                ...rule,
                id: Date.now(),
                priority: rule.priority + 1,
                status: 'inactive' // Start as inactive until activated
            };
            this.routingRules.push(duplicatedRule);
            this.routingRules.sort((a, b) => a.priority - b.priority);
            this.updateRoutingRulesDisplay();
            this.showNotification('Rule duplicated successfully', 'success');
        }
    }
    
    editRule(ruleId) {
        console.log('[NETWORK-PRIORITY] Editing rule:', ruleId);
        const rule = this.routingRules.find(r => r.id === ruleId);
        if (rule) {
            this.showRulePopup(rule);
        }
    }
    
    deleteRule(ruleId) {
        console.log('[NETWORK-PRIORITY] Deleting rule:', ruleId);
        if (confirm('Are you sure you want to delete this routing rule?')) {
            this.routingRules = this.routingRules.filter(rule => rule.id !== ruleId);
            this.updateRoutingRulesDisplay();
        }
    }
    
    showRulePopup(existingRule = null) {
        console.log('[NETWORK-PRIORITY] Showing rule popup...');
        // This will be implemented in network-priority-ui.js
        if (window.networkPriorityUI) {
            window.networkPriorityUI.showRulePopup(existingRule);
        }
    }
    
    refreshData() {
        console.log('[NETWORK-PRIORITY] Refreshing data...');
        
        // Force refresh displays if we have data
        if (this.interfaces.length > 0) {
            this.updateInterfacesDisplay();
        }
        
        if (this.routingRules.length > 0) {
            this.forceRefreshRoutingRules();
        }
        
        // If no data, wait for WebSocket
        if (this.interfaces.length === 0 && this.routingRules.length === 0) {
            console.log('[NETWORK-PRIORITY] No data to refresh, waiting for WebSocket...');
            this.showNotification('Waiting for data from server...', 'info');
        }
    }
    
    exportRules() {
        console.log('[NETWORK-PRIORITY] Exporting rules...');
        const data = {
            interfaces: this.interfaces,
            routingRules: this.routingRules,
            exportDate: new Date().toISOString()
        };
        
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `network-priority-${new Date().toISOString().split('T')[0]}.json`;
        a.click();
        URL.revokeObjectURL(url);
    }
    
    applyChanges() {
        console.log('[NETWORK-PRIORITY] Applying changes...');
        
        // Send changes to backend
        if (this.sourceManager && this.sourceManager.httpClient) {
            const data = {
                interfaces: this.interfaces,
                routingRules: this.routingRules
            };
            
            this.sourceManager.httpClient.post('/api/network/priority/update', data)
                .then(response => {
                    console.log('[NETWORK-PRIORITY] Changes applied successfully:', response);
                    this.showNotification('Changes applied successfully', 'success');
                })
                .catch(error => {
                    console.error('[NETWORK-PRIORITY] Failed to apply changes:', error);
                    this.showNotification('Failed to apply changes', 'error');
                });
        }
    }
    
    resetToDefaults() {
        console.log('[NETWORK-PRIORITY] Resetting to defaults...');
        if (confirm('Are you sure you want to reset all network priority settings to defaults?')) {
            this.loadInitialData();
        }
    }
    
    showNotification(message, type = 'info') {
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `fixed top-20 right-4 p-4 rounded-lg shadow-lg z-50 ${
            type === 'success' ? 'bg-green-500 text-white' :
            type === 'error' ? 'bg-red-500 text-white' :
            'bg-blue-500 text-white'
        }`;
        notification.textContent = message;
        
        document.body.appendChild(notification);
        
        // Remove after 3 seconds
        setTimeout(() => {
            if (notification.parentNode) {
                notification.parentNode.removeChild(notification);
            }
        }, 3000);
    }
    
    /**
     * Show dialog to add temporary routing rule
     */
    showAddTempRuleDialog() {
        console.log('[NETWORK-PRIORITY] Showing add temporary rule dialog');
        
        // Create a simple prompt for now (can be enhanced with a proper modal)
        const destination = prompt('Enter destination (e.g., 192.168.1.0/24):');
        if (!destination) return;
        
        const gateway = prompt('Enter gateway (e.g., 192.168.1.1):');
        if (!gateway) return;
        
        const interfaceName = prompt('Enter interface name (e.g., eth0):');
        if (!interfaceName) return;
        
        const priority = prompt('Enter priority (lower number = higher priority):');
        if (!priority || isNaN(priority)) return;
        
        const tempRule = {
            destination: destination.trim(),
            gateway: gateway.trim(),
            interface: interfaceName.trim(),
            priority: parseInt(priority),
            status: 'active',
            metric: 100
        };
        
        this.addTempRoutingRule(tempRule);
    }
    
    /**
     * Refresh network priority data before showing modal
     */
    async refreshNetworkPriorityData() {
        console.log('[NETWORK-PRIORITY] Refreshing network-priority data for modal');
        
        if (this) {
            // Don't trigger full data reload - just ensure we have current data
            try {
                // If we already have interfaces, use them instead of clearing and reloading
                if (this.interfaces && this.interfaces.length > 0) {
                    console.log('[NETWORK-PRIORITY] Using existing interfaces data:', {
                        interfacesCount: this.interfaces.length,
                        interfaces: this.interfaces
                    });
                    return Promise.resolve(this.interfaces);
                }
                
                // Only load initial data if we don't have any interfaces
                console.log('[NETWORK-PRIORITY] No interfaces available, loading initial data...');
                if (typeof this.loadInitialData === 'function') {
                    await this.loadInitialData();
                }
                
                console.log('[NETWORK-PRIORITY] Network-priority data refreshed:', {
                    interfacesCount: this.interfaces?.length || 0,
                    interfaces: this.interfaces
                });
            } catch (error) {
                console.warn('[NETWORK-PRIORITY] Failed to refresh network-priority data:', error);
                // Return empty array on error to prevent modal from breaking
                return [];
            }
        } else {
            console.warn('[NETWORK-PRIORITY] Network priority manager not available');
            return [];
        }
    }

    destroy() {
        console.log('[NETWORK-PRIORITY] Destroying Network Priority Manager...');
        // Cleanup event listeners and resources
        this.interfaces = [];
        this.routingRules = [];
        this.tempRoutingRules = [];
        this.selectedRules.clear();
        this.selectedTempRules.clear();
        
        // Clear connection timeout
        if (this.connectionTimeout) {
            clearTimeout(this.connectionTimeout);
            this.connectionTimeout = null;
        }
        
        // Reset connection status
        this.updateConnectionStatus(false);
    }
    
    /**
     * Add temporary routing rule
     */
    addTempRoutingRule(rule) {
        console.log('[NETWORK-PRIORITY] Adding temporary routing rule:', rule);
        const tempRule = {
            ...rule,
            id: `temp_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
            isTemporary: true,
            createdAt: new Date().toISOString()
        };
        
        this.tempRoutingRules.push(tempRule);
        this.updateTempRoutingRulesDisplay();
        
        // Show notification
        this.showNotification('Temporary routing rule added successfully', 'success');
        
        console.log('[NETWORK-PRIORITY] Temporary rule added:', tempRule);
    }
    
    /**
     * Remove temporary routing rule
     */
    removeTempRoutingRule(ruleId) {
        console.log('[NETWORK-PRIORITY] Removing temporary routing rule:', ruleId);
        const index = this.tempRoutingRules.findIndex(rule => rule.id === ruleId);
        
        if (index !== -1) {
            this.tempRoutingRules.splice(index, 1);
            this.selectedTempRules.delete(ruleId);
            this.updateTempRoutingRulesDisplay();
            this.updateTempRuleSelection();
            
            this.showNotification('Temporary routing rule removed', 'info');
            console.log('[NETWORK-PRIORITY] Temporary rule removed:', ruleId);
        }
    }
    
    /**
     * Convert selected temporary rules to static rules
     */
    convertTempRulesToStatic() {
        console.log('[NETWORK-PRIORITY] Converting selected temporary rules to static');
        
        if (this.selectedTempRules.size === 0) {
            this.showNotification('No temporary rules selected for conversion', 'warning');
            return;
        }
        
        const rulesToConvert = this.tempRoutingRules.filter(rule => 
            this.selectedTempRules.has(rule.id)
        );
        
        // Convert each selected temp rule to static
        rulesToConvert.forEach(tempRule => {
            const staticRule = {
                ...tempRule,
                id: `static_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
                isTemporary: false,
                convertedFromTemp: true,
                convertedAt: new Date().toISOString()
            };
            
            // Remove temporary flag and add to static rules
            delete staticRule.isTemporary;
            delete staticRule.createdAt;
            this.routingRules.push(staticRule);
            
            // Remove from temporary rules
            this.removeTempRoutingRule(tempRule.id);
        });
        
        // Update displays
        this.updateRoutingRulesDisplay();
        this.updateTempRoutingRulesDisplay();
        
        this.showNotification(
            `Successfully converted ${rulesToConvert.length} temporary rules to static rules`,
            'success'
        );
        
        console.log('[NETWORK-PRIORITY] Converted rules to static:', rulesToConvert.length);
    }
    
    /**
     * Update temporary routing rules display
     */
    updateTempRoutingRulesDisplay() {
        const tbody = document.getElementById('temp-routing-rules-tbody');
        const countElement = document.getElementById('temp-rules-count');
        
        if (!tbody) {
            console.warn('[NETWORK-PRIORITY] Temp routing rules tbody not found');
            return;
        }
        
        // Store scroll position before update
        const scrollPosition = window.pageYOffset || document.documentElement.scrollTop;
        
        // Update count
        if (countElement) {
            countElement.textContent = `${this.tempRoutingRules.length} rules`;
        }
        
        // Show empty state if no temp rules
        if (this.tempRoutingRules.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="6" class="px-3 py-4 text-center text-sm text-orange-600">
                        <i class="fa-solid fa-info-circle mr-2"></i>
                        No temporary rules configured
                    </td>
                </tr>
            `;
            // Restore scroll position
            window.scrollTo(0, scrollPosition);
            return;
        }
        
        // Sort by priority
        const sortedRules = [...this.tempRoutingRules].sort((a, b) => (a.priority || 0) - (b.priority || 0));
        
        // Generate HTML for temp rules
        const rulesHTML = sortedRules.map(rule => this.createTempRoutingRuleHTML(rule)).join('');
        tbody.innerHTML = rulesHTML;
        
        // Re-attach event listeners
        this.attachTempRuleEventListeners();
        
        // Restore scroll position after DOM update
        window.scrollTo(0, scrollPosition);
        
        console.log('[NETWORK-PRIORITY] Temp routing rules display updated:', this.tempRoutingRules.length, 'rules');
    }
    
    /**
     * Create HTML for temporary routing rule
     */
    createTempRoutingRuleHTML(rule) {
        const isSelected = this.selectedTempRules.has(rule.id);
        const statusColor = rule.status === 'active' ? 'green' : 'red';
        const statusIcon = rule.status === 'active' ? 'check-circle' : 'times-circle';
        
        return `
            <tr class="hover:bg-orange-50 transition-colors ${isSelected ? 'bg-orange-100' : ''}">
                <td class="px-3 py-2">
                    <input type="checkbox" 
                           class="rounded border-orange-300" 
                           data-temp-rule-id="${rule.id}"
                           ${isSelected ? 'checked' : ''}>
                </td>
                <td class="px-3 py-2 text-sm text-orange-800 font-medium">${rule.priority || 'N/A'}</td>
                <td class="px-3 py-2 text-sm text-orange-700">${rule.destination || 'N/A'}</td>
                <td class="px-3 py-2 text-sm text-orange-700">${rule.gateway || 'N/A'}</td>
                <td class="px-3 py-2 text-sm text-orange-700">${rule.interface || 'N/A'}</td>
                <td class="px-3 py-2">
                    <div class="flex items-center space-x-2">
                        <span class="inline-flex items-center px-2 py-1 rounded-full text-xs font-medium bg-orange-100 text-orange-800">
                            <i class="fa-solid fa-${statusIcon} mr-1"></i>
                            ${rule.status || 'unknown'}
                        </span>
                        <button class="text-red-600 hover:text-red-800 text-sm" 
                                onclick="window.networkPriorityManager.removeTempRoutingRule('${rule.id}')"
                                title="Remove temporary rule">
                            <i class="fa-solid fa-trash"></i>
                        </button>
                    </div>
                </td>
            </tr>
        `;
    }
    
    /**
     * Attach event listeners for temporary rules
     */
    attachTempRuleEventListeners() {
        // Checkbox selection
        document.querySelectorAll('input[data-temp-rule-id]').forEach(checkbox => {
            checkbox.addEventListener('change', (e) => {
                const ruleId = e.target.dataset.tempRuleId;
                if (e.target.checked) {
                    this.selectedTempRules.add(ruleId);
                } else {
                    this.selectedTempRules.delete(ruleId);
                }
                this.updateTempRuleSelection();
            });
        });
        
        // Select all checkbox
        const selectAllCheckbox = document.getElementById('select-all-temp-rules');
        if (selectAllCheckbox) {
            selectAllCheckbox.addEventListener('change', (e) => {
                const checkboxes = document.querySelectorAll('input[data-temp-rule-id]');
                checkboxes.forEach(checkbox => {
                    checkbox.checked = e.target.checked;
                    const ruleId = checkbox.dataset.tempRuleId;
                    if (e.target.checked) {
                        this.selectedTempRules.add(ruleId);
                    } else {
                        this.selectedTempRules.delete(ruleId);
                    }
                });
                this.updateTempRuleSelection();
                this.updateTempRoutingRulesDisplay();
            });
        }
    }
    
    /**
     * Update temporary rule selection state
     */
    updateTempRuleSelection() {
        const convertBtn = document.getElementById('convert-to-static-btn');
        if (convertBtn) {
            convertBtn.disabled = this.selectedTempRules.size === 0;
        }
        
        // Update select all checkbox state
        const selectAllCheckbox = document.getElementById('select-all-temp-rules');
        if (selectAllCheckbox) {
            const totalCheckboxes = document.querySelectorAll('input[data-temp-rule-id]').length;
            selectAllCheckbox.checked = totalCheckboxes > 0 && this.selectedTempRules.size === totalCheckboxes;
            selectAllCheckbox.indeterminate = this.selectedTempRules.size > 0 && this.selectedTempRules.size < totalCheckboxes;
        }
        
        console.log('[NETWORK-PRIORITY] Temp rule selection updated:', this.selectedTempRules.size, 'selected');
    }
}

// ES6 Module Export
export { NetworkPriorityManager };

// Also make available globally for browser compatibility
if (typeof window !== 'undefined') {
    window.NetworkPriorityManager = NetworkPriorityManager;
}

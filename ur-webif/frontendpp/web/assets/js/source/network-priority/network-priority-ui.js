/**
 * Network Priority UI Manager
 * Handles UI interactions, popups, and visual elements for network priority
 */

class NetworkPriorityUI {
    constructor(networkPriorityManager) {
        this.networkPriorityManager = networkPriorityManager;
        this.currentPopup = null;
        this.isDragging = false;
        
        this.init();
    }
    
    init() {
        console.log('[NETWORK-PRIORITY-UI] Initializing UI Manager...');
        this.setupUIEventListeners();
        this.createRulePopupHTML();
    }
    
    setupUIEventListeners() {
        // Setup button event listeners
        this.setupButtonListeners();
        
        // Setup modal event listeners
        this.setupModalListeners();
        
        // Setup form validation
        this.setupFormValidation();
    }
    
    /**
     * Show interface edit popup
     */
    showInterfaceEditPopup(iface) {
        console.log('[NETWORK-PRIORITY-UI] Showing interface edit popup for:', iface);
        
        // Create popup HTML
        const popupHTML = `
            <div class="popup-overlay fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
                <div class="popup-content bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
                    <div class="border-b border-neutral-200 p-6">
                        <h3 class="text-lg font-semibold text-neutral-900">Edit Interface: ${iface.name}</h3>
                        <p class="text-sm text-neutral-600 mt-1">Modify interface priority and settings</p>
                    </div>
                    <div class="p-6">
                        <form id="edit-interface-form">
                            <div class="space-y-4">
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-1">Interface Name</label>
                                    <input type="text" id="interface-name" name="name" value="${iface.name}" readonly
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-lg bg-neutral-50 text-neutral-500">
                                </div>
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-1">Priority</label>
                                    <input type="number" id="interface-priority" name="priority" value="${iface.priority}" required min="1" max="100"
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                                    <span class="text-xs text-red-500 hidden" id="interface-priority-error">Priority must be between 1 and 100</span>
                                </div>
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-1">Status</label>
                                    <select id="interface-status" name="status" 
                                            class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                                        <option value="online" ${iface.status === 'online' ? 'selected' : ''}>Online</option>
                                        <option value="offline" ${iface.status === 'offline' ? 'selected' : ''}>Offline</option>
                                    </select>
                                </div>
                                <div class="grid grid-cols-2 gap-4">
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-1">IP Address</label>
                                        <input type="text" id="interface-ip" name="ip" value="${iface.ip || ''}"
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                                    </div>
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-1">Gateway</label>
                                        <input type="text" id="interface-gateway" name="gateway" value="${iface.gateway || ''}"
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                                    </div>
                                </div>
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-1">MAC Address</label>
                                    <input type="text" id="interface-mac" name="mac" value="${iface.mac || ''}"
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                                </div>
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-1">Notes</label>
                                    <textarea id="interface-notes" name="notes" rows="3"
                                              class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                                              placeholder="Add any notes about this interface...">${iface.notes || ''}</textarea>
                                </div>
                            </div>
                        </form>
                    </div>
                    <div class="border-t border-neutral-200 p-6 flex justify-end space-x-3">
                        <button type="button" class="popup-cancel px-4 py-2 bg-neutral-100 text-neutral-700 rounded-lg hover:bg-neutral-200">
                            Cancel
                        </button>
                        <button type="button" class="popup-save px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700">
                            Save Changes
                        </button>
                    </div>
                </div>
            </div>
        `;
        
        // Add popup to DOM
        document.body.insertAdjacentHTML('beforeend', popupHTML);
        this.currentPopup = document.querySelector('.popup-overlay');
        
        // Setup popup event listeners
        this.setupInterfaceEditPopupListeners(iface);
    }
    
    /**
     * Setup interface edit popup event listeners
     */
    setupInterfaceEditPopupListeners(iface) {
        const popup = this.currentPopup;
        if (!popup) return;
        
        // Cancel button
        popup.querySelector('.popup-cancel').addEventListener('click', () => {
            this.closeInterfaceEditPopup();
        });
        
        // Save button
        popup.querySelector('.popup-save').addEventListener('click', () => {
            this.saveInterfaceEdit(iface);
        });
        
        // Form validation
        const priorityInput = popup.querySelector('#interface-priority');
        priorityInput.addEventListener('input', () => {
            const priority = parseInt(priorityInput.value);
            const errorElement = popup.querySelector('#interface-priority-error');
            
            if (!priority || priority < 1 || priority > 100) {
                errorElement.classList.remove('hidden');
                priorityInput.classList.add('border-red-500');
            } else {
                errorElement.classList.add('hidden');
                priorityInput.classList.remove('border-red-500');
            }
        });
        
        // Close on overlay click
        popup.addEventListener('click', (e) => {
            if (e.target === popup) {
                this.closeInterfaceEditPopup();
            }
        });
    }
    
    /**
     * Save interface edit
     */
    saveInterfaceEdit(iface) {
        const popup = this.currentPopup;
        if (!popup) return;
        
        // Validate form
        const priority = parseInt(popup.querySelector('#interface-priority').value);
        if (!priority || priority < 1 || priority > 100) {
            this.showValidationError('interface-priority-error');
            return;
        }
        
        // Update interface data
        iface.priority = priority;
        iface.status = popup.querySelector('#interface-status').value;
        iface.ip = popup.querySelector('#interface-ip').value;
        iface.gateway = popup.querySelector('#interface-gateway').value;
        iface.mac = popup.querySelector('#interface-mac').value;
        iface.notes = popup.querySelector('#interface-notes').value;
        
        // Update display
        this.networkPriorityManager.updateInterfacesDisplay();
        
        // Close popup
        this.closeInterfaceEditPopup();
        
        // Show success message
        this.networkPriorityManager.showNotification(`Interface ${iface.name} updated successfully`, 'success');
    }
    
    /**
     * Close interface edit popup
     */
    closeInterfaceEditPopup() {
        if (this.currentPopup) {
            this.currentPopup.remove();
            this.currentPopup = null;
        }
    }
    
    /**
     * Show validation error
     */
    showValidationError(errorId) {
        const errorElement = document.getElementById(errorId);
        if (errorElement) {
            errorElement.classList.remove('hidden');
            setTimeout(() => {
                errorElement.classList.add('hidden');
            }, 3000);
        }
    }
    
    setupButtonListeners() {
        // Add rule button
        const addRuleBtn = document.getElementById('add-rule-btn');
        if (addRuleBtn) {
            addRuleBtn.addEventListener('click', () => {
                this.showRulePopup();
            });
        }
        
        // Add temporary rule button
        const addTempRuleBtn = document.getElementById('add-temp-rule-btn');
        if (addTempRuleBtn) {
            addTempRuleBtn.addEventListener('click', () => {
                this.showAddTempRuleModal();
            });
        }
        
        // Export rules button
        const exportBtn = document.getElementById('export-rules-btn');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => {
                this.networkPriorityManager.exportRules();
            });
        }
        
        // Apply changes button
        const applyBtn = document.getElementById('apply-changes-btn');
        if (applyBtn) {
            applyBtn.addEventListener('click', () => {
                this.networkPriorityManager.applyChanges();
            });
        }
        
        // Reset defaults button
        const resetBtn = document.getElementById('reset-defaults-btn');
        if (resetBtn) {
            resetBtn.addEventListener('click', () => {
                this.networkPriorityManager.resetToDefaults();
            });
        }
        
        // Refresh button
        const refreshBtn = document.getElementById('refresh-priority-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.networkPriorityManager.refreshData();
            });
        }
        
        // Preview changes button
        const previewBtn = document.getElementById('preview-changes-btn');
        if (previewBtn) {
            previewBtn.addEventListener('click', () => {
                this.previewChanges();
            });
        }
        
        // Cancel button
        const cancelBtn = document.getElementById('cancel-btn');
        if (cancelBtn) {
            cancelBtn.addEventListener('click', () => {
                this.cancelChanges();
            });
        }
    }
    
    setupModalListeners() {
        // Close popup when clicking outside
        document.addEventListener('click', (e) => {
            if (e.target.classList.contains('popup-overlay')) {
                this.closeRulePopup();
            }
        });
        
        // Close popup with Escape key
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.currentPopup) {
                this.closeRulePopup();
            }
        });
    }
    
    setupFormValidation() {
        // Form validation will be setup when popup is shown
    }
    
    createRulePopupHTML() {
        // Create popup HTML and add to body
        const popupHTML = `
            <div id="add-rule-popup" class="popup-overlay" style="position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: none; align-items: center; justify-content: center; z-index: 1000; opacity: 0; visibility: hidden; transition: opacity 0.3s ease, visibility 0.3s ease;">
                <div class="popup-content" style="background: white; border-radius: 12px; padding: 0; max-width: 800px; width: 90%; max-height: 90vh; overflow-y: auto; transform: scale(0.9); transition: transform 0.3s ease; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);">
                    <div class="bg-white w-full h-auto shadow-lg overflow-y-auto">
                        <!-- Header -->
                        <div class="border-b border-neutral-200 p-6">
                            <div class="flex items-center justify-between">
                                <div class="flex items-center space-x-3">
                                    <div class="w-10 h-10 bg-blue-100 rounded-lg flex items-center justify-center">
                                        <i class="fa-solid fa-route text-blue-600 text-lg"></i>
                                    </div>
                                    <div>
                                        <h1 class="text-2xl text-neutral-900" id="popup-title">Add New Routing Rule</h1>
                                        <p class="text-neutral-500 text-sm" id="popup-description">Configure custom routing rule for network traffic</p>
                                    </div>
                                </div>
                                <button class="popup-close w-8 h-8 rounded-full bg-neutral-100 hover:bg-neutral-200 flex items-center justify-center">
                                    <i class="fa-solid fa-xmark text-neutral-500"></i>
                                </button>
                            </div>
                        </div>
                        <!-- Content -->
                        <div class="p-6">
                            <form id="add-rule-form" class="space-y-6">
                                <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">Destination Network *</label>
                                        <input type="text" id="rule-destination" name="destination" required
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                                               placeholder="e.g., 192.168.1.0/24">
                                        <span class="text-xs text-red-500 hidden" id="destination-error">Please enter a valid destination</span>
                                    </div>
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">Gateway *</label>
                                        <input type="text" id="rule-gateway" name="gateway" required
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                                               placeholder="e.g., 192.168.1.1">
                                        <span class="text-xs text-red-500 hidden" id="gateway-error">Please enter a valid gateway</span>
                                    </div>
                                </div>
                                <div class="grid grid-cols-1 md:grid-cols-3 gap-6">
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">Interface *</label>
                                        <select id="rule-interface" name="interface" required
                                                class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                                            <option value="">Select an interface...</option>
                                        </select>
                                        <span class="text-xs text-red-500 hidden" id="interface-error">Please select an interface</span>
                                    </div>
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">Metric *</label>
                                        <input type="number" id="rule-metric" name="metric" required min="1" max="9999"
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                                               placeholder="e.g., 100">
                                        <span class="text-xs text-red-500 hidden" id="metric-error">Metric must be between 1 and 9999</span>
                                    </div>
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">Priority *</label>
                                        <input type="number" id="rule-priority" name="priority" required min="1" max="100"
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                                               placeholder="e.g., 1">
                                        <span class="text-xs text-red-500 hidden" id="priority-error">Priority must be between 1 and 100</span>
                                    </div>
                                </div>
                            </form>
                        </div>
                        <!-- Footer -->
                        <div class="border-t border-neutral-200 p-6 flex justify-end space-x-3">
                            <button type="button" class="popup-cancel px-4 py-2 bg-neutral-100 text-neutral-700 rounded-lg hover:bg-neutral-200">
                                Cancel
                            </button>
                            <button type="button" class="popup-save px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700">
                                Add Rule
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        `;
        
        document.body.insertAdjacentHTML('beforeend', popupHTML);
        
        // Setup popup event listeners
        this.setupPopupEventListeners();
    }
    
    setupPopupEventListeners() {
        const popup = document.getElementById('add-rule-popup');
        if (!popup) return;
        
        // Close button
        const closeBtn = popup.querySelector('.popup-close');
        if (closeBtn) {
            closeBtn.addEventListener('click', () => {
                this.closeRulePopup();
            });
        }
        
        // Cancel button
        const cancelBtn = popup.querySelector('.popup-cancel');
        if (cancelBtn) {
            cancelBtn.addEventListener('click', () => {
                this.closeRulePopup();
            });
        }
        
        // Save button
        const saveBtn = popup.querySelector('.popup-save');
        if (saveBtn) {
            saveBtn.addEventListener('click', () => {
                this.saveRule();
            });
        }
        
        // Form validation
        const form = document.getElementById('add-rule-form');
        if (form) {
            form.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveRule();
            });
        }
    }
    
    showRulePopup(existingRule = null) {
        console.log('[NETWORK-PRIORITY-UI] Showing rule popup:', existingRule);
        
        const popup = document.getElementById('add-rule-popup');
        if (!popup) return;
        
        // Reset form
        this.resetForm();
        
        // Update title for edit mode
        const title = document.getElementById('popup-title');
        const description = document.getElementById('popup-description');
        const saveBtn = popup.querySelector('.popup-save');
        
        if (existingRule) {
            title.textContent = 'Edit Routing Rule';
            description.textContent = 'Modify existing routing rule configuration';
            saveBtn.textContent = 'Update Rule';
            this.populateForm(existingRule);
        } else {
            title.textContent = 'Add New Routing Rule';
            description.textContent = 'Configure custom routing rule for network traffic';
            saveBtn.textContent = 'Add Rule';
        }
        
        // Populate interface options
        this.populateInterfaceOptions();
        
        // Show popup with animation
        popup.style.display = 'flex';
        setTimeout(() => {
            popup.style.opacity = '1';
            popup.style.visibility = 'visible';
            popup.querySelector('.popup-content').style.transform = 'scale(1)';
        }, 10);
        
        this.currentPopup = popup;
    }
    
    closeRulePopup() {
        console.log('[NETWORK-PRIORITY-UI] Closing rule popup...');
        
        if (!this.currentPopup) return;
        
        // Hide popup with animation
        this.currentPopup.style.opacity = '0';
        this.currentPopup.style.visibility = 'hidden';
        this.currentPopup.querySelector('.popup-content').style.transform = 'scale(0.9)';
        
        setTimeout(() => {
            this.currentPopup.style.display = 'none';
            this.currentPopup = null;
        }, 300);
    }
    
    resetForm() {
        const form = document.getElementById('add-rule-form');
        if (form) {
            form.reset();
            this.clearValidationErrors();
        }
    }
    
    populateForm(rule) {
        document.getElementById('rule-destination').value = rule.destination || '';
        document.getElementById('rule-gateway').value = rule.gateway || '';
        document.getElementById('rule-interface').value = rule.interface || '';
        document.getElementById('rule-metric').value = rule.metric || '';
        document.getElementById('rule-priority').value = rule.priority || '';
    }
    
    populateInterfaceOptions() {
        const select = document.getElementById('rule-interface');
        if (!select) return;
        
        // Clear existing options
        select.innerHTML = '<option value="">Select an interface...</option>';
        
        // Add interface options
        this.networkPriorityManager.interfaces.forEach(iface => {
            const option = document.createElement('option');
            option.value = iface.id;
            option.textContent = `${iface.name} (${iface.id})`;
            if (iface.status !== 'online') {
                option.disabled = true;
                option.textContent += ' (Offline)';
            }
            select.appendChild(option);
        });
    }
    
    validateForm() {
        let isValid = true;
        this.clearValidationErrors();
        
        // Validate destination
        const destination = document.getElementById('rule-destination').value.trim();
        if (!destination || !this.isValidNetworkAddress(destination)) {
            this.showValidationError('destination-error');
            isValid = false;
        }
        
        // Validate gateway
        const gateway = document.getElementById('rule-gateway').value.trim();
        if (!gateway || !this.isValidIPAddress(gateway)) {
            this.showValidationError('gateway-error');
            isValid = false;
        }
        
        // Validate interface
        const selectedInterface = document.getElementById('rule-interface').value;
        if (!selectedInterface) {
            this.showValidationError('interface-error');
            isValid = false;
        }
        
        // Validate metric
        const metric = parseInt(document.getElementById('rule-metric').value);
        if (!metric || metric < 1 || metric > 9999) {
            this.showValidationError('metric-error');
            isValid = false;
        }
        
        // Validate priority
        const priority = parseInt(document.getElementById('rule-priority').value);
        if (!priority || priority < 1 || priority > 100) {
            this.showValidationError('priority-error');
            isValid = false;
        }
        
        return isValid;
    }
    
    isValidNetworkAddress(address) {
        // Simple validation for CIDR notation or IP address
        const cidrRegex = /^(\d{1,3}\.){3}\d{1,3}\/\d{1,2}$/;
        const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
        return cidrRegex.test(address) || ipRegex.test(address);
    }
    
    isValidIPAddress(address) {
        const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
        if (!ipRegex.test(address)) return false;
        
        const parts = address.split('.');
        return parts.every(part => {
            const num = parseInt(part);
            return num >= 0 && num <= 255;
        });
    }
    
    showValidationError(errorId) {
        const errorElement = document.getElementById(errorId);
        if (errorElement) {
            errorElement.classList.remove('hidden');
        }
    }
    
    clearValidationErrors() {
        const errors = document.querySelectorAll('[id$="-error"]');
        errors.forEach(error => error.classList.add('hidden'));
    }
    
    saveRule() {
        console.log('[NETWORK-PRIORITY-UI] Saving rule...');
        
        if (!this.validateForm()) {
            console.log('[NETWORK-PRIORITY-UI] Form validation failed');
            return;
        }
        
        const formData = {
            destination: document.getElementById('rule-destination').value.trim(),
            gateway: document.getElementById('rule-gateway').value.trim(),
            interface: document.getElementById('rule-interface').value,
            metric: parseInt(document.getElementById('rule-metric').value),
            priority: parseInt(document.getElementById('rule-priority').value),
            status: 'active'
        };
        
        // Check if we're editing or adding
        const isEditing = document.getElementById('popup-title').textContent.includes('Edit');
        
        if (isEditing) {
            // Update existing rule
            const ruleId = parseInt(document.getElementById('add-rule-form').dataset.ruleId);
            const ruleIndex = this.networkPriorityManager.routingRules.findIndex(r => r.id === ruleId);
            if (ruleIndex !== -1) {
                this.networkPriorityManager.routingRules[ruleIndex] = { ...this.networkPriorityManager.routingRules[ruleIndex], ...formData };
            }
        } else {
            // Add new rule
            const newRule = {
                id: Date.now(), // Simple ID generation
                ...formData
            };
            this.networkPriorityManager.routingRules.push(newRule);
        }
        
        // Sort rules by priority
        this.networkPriorityManager.routingRules.sort((a, b) => a.priority - b.priority);
        
        // Update display
        this.networkPriorityManager.updateRoutingRulesDisplay();
        
        // Close popup
        this.closeRulePopup();
        
        // Show success message
        this.networkPriorityManager.showNotification(
            isEditing ? 'Rule updated successfully' : 'Rule added successfully',
            'success'
        );
    }
    
    previewChanges() {
        console.log('[NETWORK-PRIORITY-UI] Previewing changes...');
        
        const changes = {
            interfaces: this.networkPriorityManager.interfaces,
            routingRules: this.networkPriorityManager.routingRules
        };
        
        // Create preview modal
        this.showPreviewModal(changes);
    }
    
    showPreviewModal(changes) {
        const modalHTML = `
            <div id="preview-modal" class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
                <div class="bg-white rounded-lg p-6 max-w-2xl w-full max-h-[80vh] overflow-y-auto">
                    <h2 class="text-xl font-bold mb-4">Preview Changes</h2>
                    <div class="space-y-4">
                        <div>
                            <h3 class="font-semibold mb-2">Interface Priority Order</h3>
                            <pre class="bg-gray-100 p-3 rounded text-sm">${JSON.stringify(changes.interfaces, null, 2)}</pre>
                        </div>
                        <div>
                            <h3 class="font-semibold mb-2">Routing Rules</h3>
                            <pre class="bg-gray-100 p-3 rounded text-sm">${JSON.stringify(changes.routingRules, null, 2)}</pre>
                        </div>
                    </div>
                    <div class="flex justify-end space-x-3 mt-6">
                        <button onclick="this.closest('.fixed').remove()" class="px-4 py-2 bg-gray-200 text-gray-800 rounded hover:bg-gray-300">
                            Close
                        </button>
                        <button onclick="window.networkPriorityManager.applyChanges(); this.closest('.fixed').remove();" class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700">
                            Apply Changes
                        </button>
                    </div>
                </div>
            </div>
        `;
        
        document.body.insertAdjacentHTML('beforeend', modalHTML);
    }
    
    cancelChanges() {
        console.log('[NETWORK-PRIORITY-UI] Canceling changes...');
        
        if (confirm('Are you sure you want to discard all changes?')) {
            // Reload original data
            this.networkPriorityManager.loadInitialData();
        }
    }
    
    /**
     * Show add temporary routing rule modal using popup manager
     */
    showAddTempRuleModal() {
        console.log('[NETWORK-PRIORITY-UI] Showing add temporary rule modal');
        
        if (window.popupManager) {
            // Ensure we have the latest network-priority data before showing modal
            this.networkPriorityManager.refreshNetworkPriorityData().then(() => {
                // Use popup manager for consistent modal behavior
                window.popupManager.showCustom({
                    title: 'Add Temporary Routing Rule',
                    content: `
                        <div class="temp-rule-modal-content">
                            <!-- Modal Header -->
                            <div class="flex items-center justify-between mb-6">
                                <div class="flex items-center">
                                    <div class="w-10 h-10 rounded-full bg-orange-100 flex items-center justify-center mr-3">
                                        <i class="fa-solid fa-clock text-orange-600"></i>
                                    </div>
                                    <div>
                                        <h3 class="text-lg font-semibold text-neutral-900">Add Temporary Routing Rule</h3>
                                        <p class="text-sm text-orange-600">This rule will be removed when the device reboots</p>
                                    </div>
                                </div>
                            </div>
                            
                            <!-- Modal Body -->
                            <form id="temp-rule-form" class="space-y-4">
                                <!-- Destination -->
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-2">
                                        <i class="fa-solid fa-network-wired mr-1"></i>
                                        Destination Network *
                                    </label>
                                    <input type="text" 
                                           id="temp-destination" 
                                           name="destination"
                                           placeholder="e.g., 192.168.1.0/24 or 10.0.0.0/8"
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-orange-500 focus:border-transparent"
                                           required>
                                    <p class="mt-1 text-xs text-neutral-500">Enter destination network in CIDR notation</p>
                                </div>
                                
                                <!-- Gateway -->
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-2">
                                        <i class="fa-solid fa-route mr-1"></i>
                                        Gateway *
                                    </label>
                                    <input type="text" 
                                           id="temp-gateway" 
                                           name="gateway"
                                           placeholder="e.g., 192.168.1.1 or 10.0.0.1"
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-orange-500 focus:border-transparent"
                                           required>
                                    <p class="mt-1 text-xs text-neutral-500">Enter gateway IP address</p>
                                </div>
                                
                                <!-- Interface -->
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-2">
                                        <i class="fa-solid fa-ethernet mr-1"></i>
                                        Interface *
                                    </label>
                                    <select id="temp-interface" 
                                            name="interface"
                                            class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-orange-500 focus:border-transparent"
                                            required>
                                        <option value="">Loading interfaces...</option>
                                    </select>
                                    <p class="mt-1 text-xs text-neutral-500">Choose the network interface for this rule</p>
                                </div>
                                
                                <!-- Priority and Metric in row -->
                                <div class="grid grid-cols-2 gap-4">
                                    <!-- Priority -->
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">
                                            <i class="fa-solid fa-sort-numeric-down mr-1"></i>
                                            Priority *
                                        </label>
                                        <input type="number" 
                                               id="temp-priority" 
                                               name="priority"
                                               placeholder="1-1000"
                                               min="1"
                                               max="1000"
                                               value="100"
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-orange-500 focus:border-transparent"
                                               required>
                                        <p class="mt-1 text-xs text-neutral-500">Lower number = higher priority</p>
                                    </div>
                                    
                                    <!-- Metric -->
                                    <div>
                                        <label class="block text-sm font-medium text-neutral-700 mb-2">
                                            <i class="fa-solid fa-tachometer-alt mr-1"></i>
                                            Metric *
                                        </label>
                                        <input type="number" 
                                               id="temp-metric" 
                                               name="metric"
                                               placeholder="0-255"
                                               min="0"
                                               max="255"
                                               value="100"
                                               class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-orange-500 focus:border-transparent"
                                               required>
                                        <p class="mt-1 text-xs text-neutral-500">Route metric (lower = preferred)</p>
                                    </div>
                                </div>
                                
                                <!-- Description (Optional) -->
                                <div>
                                    <label class="block text-sm font-medium text-neutral-700 mb-2">
                                        <i class="fa-solid fa-comment mr-1"></i>
                                        Description (Optional)
                                    </label>
                                    <textarea id="temp-description" 
                                              name="description"
                                              rows="2"
                                              placeholder="Add a note about this temporary rule..."
                                              class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-orange-500 focus:border-transparent"></textarea>
                                </div>
                                
                                <!-- Warning Box -->
                                <div class="bg-orange-50 border border-orange-200 rounded-md p-3">
                                    <div class="flex">
                                        <div class="flex-shrink-0">
                                            <i class="fa-solid fa-exclamation-triangle text-orange-400"></i>
                                        </div>
                                        <div class="ml-3">
                                            <h3 class="text-sm font-medium text-orange-800">Temporary Rule Notice</h3>
                                            <div class="mt-1 text-sm text-orange-700">
                                                <p>• This rule will be automatically removed when the device reboots</p>
                                                <p>• Use the "Convert to Static Rules" button to make it permanent</p>
                                                <p>• Temporary rules are useful for testing and temporary network changes</p>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </form>
                            
                            <!-- Modal Footer Info -->
                            <div class="mt-6 text-sm text-neutral-500">
                                <i class="fa-solid fa-info-circle mr-1"></i>
                                All fields marked with * are required
                            </div>
                        </div>
                    `,
                    buttons: [
                        {
                            text: 'Cancel',
                            class: 'px-4 py-2 text-neutral-700 border border-neutral-300 rounded-md hover:bg-neutral-50 transition-colors',
                            action: () => {
                                window.popupManager.close();
                            }
                        },
                        {
                            text: '<i class="fa-solid fa-plus mr-2"></i>Add Temporary Rule',
                            class: 'px-4 py-2 bg-orange-600 text-white rounded-md hover:bg-orange-700 transition-colors',
                            action: () => {
                                this.submitTempRuleForm();
                            }
                        }
                    ],
                    closeOnBackdrop: true,
                    closeOnEscape: true
                }).then(() => {
                    // Initialize modal after it's shown (following popup manager animation rules)
                    this.initializeTempRuleModal();
                });
            });
        } else {
            // Fallback to simple modal when popup manager is not available
            this.showFallbackTempRuleModal();
        }
    }
    
    /**
     * Initialize temporary rule modal after popup manager shows it
     */
    initializeTempRuleModal() {
        console.log('[NETWORK-PRIORITY-UI] Initializing temporary rule modal');
        
        // Wait a moment for the popup content to be fully rendered (following popup manager rules)
        setTimeout(() => {
            // Populate interface options from network-priority data
            this.populateTempRuleInterfaces();
            
            // Focus first input for better UX - but only if it's visible in viewport
            const destinationInput = document.getElementById('temp-destination');
            if (destinationInput && this.isElementInViewport(destinationInput)) {
                destinationInput.focus();
            }
            
            // Add form submission handler
            const form = document.getElementById('temp-rule-form');
            if (form) {
                form.addEventListener('submit', (e) => {
                    e.preventDefault();
                    this.submitTempRuleForm();
                });
            }
            
            console.log('[NETWORK-PRIORITY-UI] Temporary rule modal initialized with network-priority data');
        }, 150); // Slightly longer delay to ensure popup animation completes
    }
    
    /**
     * Check if element is visible in viewport
     */
    isElementInViewport(element) {
        const rect = element.getBoundingClientRect();
        return (
            rect.top >= 0 &&
            rect.left >= 0 &&
            rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
            rect.right <= (window.innerWidth || document.documentElement.clientWidth)
        );
    }
    
    /**
     * Populate interface options in temp rule modal from network-priority data
     */
    populateTempRuleInterfaces() {
        const interfaceSelect = document.getElementById('temp-interface');
        if (!interfaceSelect) return;
        
        // Get available interfaces from network priority manager
        const interfaces = this.networkPriorityManager?.interfaces || [];
        
        console.log('[NETWORK-PRIORITY-UI] Populating interfaces from network-priority data:', interfaces);
        
        // Clear existing options except the first one
        interfaceSelect.innerHTML = '<option value="">Select an interface</option>';
        
        // Add interface options from network-priority data
        interfaces.forEach(iface => {
            const option = document.createElement('option');
            option.value = iface.name || iface.id;
            option.textContent = `${iface.name || iface.id} (${iface.type || 'unknown'}) - ${iface.status || 'unknown'}`;
            option.title = `ID: ${iface.id}, Type: ${iface.type || 'unknown'}, Status: ${iface.status || 'unknown'}`;
            
            // Only enable online interfaces, but show all for visibility
            if (iface.status === 'online') {
                option.disabled = false;
            } else {
                option.disabled = true;
                option.textContent += ' (offline)';
            }
            
            interfaceSelect.appendChild(option);
        });
        
        // Add fallback option if no interfaces available
        if (interfaces.length === 0) {
            const option = document.createElement('option');
            option.value = 'auto';
            option.textContent = 'Auto-detect interface';
            option.title = 'Automatically detect the best interface';
            interfaceSelect.appendChild(option);
        }
        
        console.log('[NETWORK-PRIORITY-UI] Populated', interfaces.length, 'interfaces in temp rule modal');
    }
    
    /**
     * Submit temporary rule form using popup manager
     */
    submitTempRuleForm() {
        console.log('[NETWORK-PRIORITY-UI] Submitting temporary rule form');
        
        const form = document.getElementById('temp-rule-form');
        if (!form) return;
        
        // Get form data
        const formData = new FormData(form);
        const tempRule = {
            destination: formData.get('destination')?.trim(),
            gateway: formData.get('gateway')?.trim(),
            interface: formData.get('interface')?.trim(),
            priority: parseInt(formData.get('priority')) || 100,
            metric: parseInt(formData.get('metric')) || 100,
            description: formData.get('description')?.trim(),
            status: 'active'
        };
        
        // Validate required fields
        if (!tempRule.destination || !tempRule.gateway || !tempRule.interface) {
            this.showTempRuleError('Please fill in all required fields');
            return;
        }
        
        // Validate IP/network format (basic validation)
        if (!this.validateNetworkAddress(tempRule.destination)) {
            this.showTempRuleError('Please enter a valid destination network (e.g., 192.168.1.0/24)');
            return;
        }
        
        if (!this.validateIPAddress(tempRule.gateway)) {
            this.showTempRuleError('Please enter a valid gateway IP address');
            return;
        }
        
        // Close modal using popup manager
        if (window.popupManager) {
            window.popupManager.close();
        }
        
        // Add the temporary rule
        this.networkPriorityManager.addTempRoutingRule(tempRule);
    }
    
    /**
     * Show error using popup manager
     */
    showTempRuleError(message) {
        if (window.popupManager) {
            window.popupManager.showError({
                title: 'Validation Error',
                message: message,
                buttonText: 'OK'
            });
        } else {
            // Fallback notification
            this.networkPriorityManager.showNotification(message, 'error');
        }
    }
    
    /**
     * Validate network address (CIDR notation)
     */
    validateNetworkAddress(address) {
        // Basic CIDR validation (can be enhanced)
        const cidrPattern = /^(\d{1,3}\.){3}\d{1,3}\/\d{1,2}$/;
        return cidrPattern.test(address);
    }
    
    /**
     * Validate IP address
     */
    validateIPAddress(address) {
        const ipPattern = /^(\d{1,3}\.){3}\d{1,3}$/;
        if (!ipPattern.test(address)) return false;
        
        const parts = address.split('.');
        return parts.every(part => {
            const num = parseInt(part);
            return num >= 0 && num <= 255;
        });
    }
    
    /**
     * Fallback modal when popup manager is not available
     */
    showFallbackTempRuleModal() {
        console.log('[NETWORK-PRIORITY-UI] Popup manager not available, using fallback modal');
        
        // Create simple fallback modal
        const modalOverlay = document.createElement('div');
        modalOverlay.id = 'temp-rule-modal-overlay';
        modalOverlay.className = 'fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50';
        
        const modalContent = document.createElement('div');
        modalContent.className = 'bg-white rounded-lg shadow-xl max-w-md w-full mx-4 p-6';
        
        modalContent.innerHTML = `
            <div class="text-center">
                <i class="fa-solid fa-exclamation-triangle text-orange-500 text-4xl mb-4"></i>
                <h3 class="text-lg font-semibold text-neutral-900 mb-2">Popup Manager Not Available</h3>
                <p class="text-neutral-600 mb-4">The popup manager is not loaded. Please refresh the page and try again.</p>
                <button onclick="this.closest('#temp-rule-modal-overlay').remove()" 
                        class="px-4 py-2 bg-orange-600 text-white rounded-md hover:bg-orange-700">
                    Close
                </button>
            </div>
        `;
        
        modalOverlay.appendChild(modalContent);
        document.body.appendChild(modalOverlay);
    }

    destroy() {
        console.log('[NETWORK-PRIORITY-UI] Destroying UI Manager...');
        
        // Close any open popups
        if (this.currentPopup) {
            this.closeRulePopup();
        }
        
        // Remove popup HTML
        const popup = document.getElementById('add-rule-popup');
        if (popup) {
            popup.remove();
        }
        
        // Remove preview modal if exists
        const previewModal = document.getElementById('preview-modal');
        if (previewModal) {
            previewModal.remove();
        }
    }
}

// ES6 Module Export
export { NetworkPriorityUI };

// Export for use in other modules (fallback for non-ES6 environments)
if (typeof module !== 'undefined' && module.exports) {
    module.exports = NetworkPriorityUI;
} else if (typeof window !== 'undefined') {
    window.NetworkPriorityUI = NetworkPriorityUI;
}

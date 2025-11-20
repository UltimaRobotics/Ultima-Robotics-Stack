/**
 * MAVLink Extension UI Controller
 * Handles all UI interactions and rendering for the MAVLink extension system
 */

export class MavlinkExtensionUI {
    constructor(mavlinkExtensionManager) {
        this.manager = mavlinkExtensionManager;
        this.isInitialized = false;
        this.currentEditingEndpoint = null;
        this.currentDeletingEndpoint = null;
    }

    async init() {
        try {
            console.log('[MAVLINK-EXTENSION-UI] Initializing UI controller...');
            
            // Step 1: Show loading state immediately
            this.showLoadingState();
            
            // Step 2: Wait for manager to be initialized (max 1 second wait)
            let managerReady = false;
            if (this.manager && typeof this.manager.isInitialized === 'function') {
                try {
                    const managerTimeout = setTimeout(() => {
                        console.warn('[MAVLINK-EXTENSION-UI] Manager initialization timeout (1s)');
                    }, 1000);
                    
                    if (!this.manager._isInitialized) {
                        await new Promise((resolve) => {
                            const onInitialized = () => {
                                clearTimeout(managerTimeout);
                                this.manager.off('initialized', onInitialized);
                                managerReady = true;
                                resolve();
                            };
                            
                            this.manager.on('initialized', onInitialized);
                            
                            // Also resolve if manager gets initialized while we're waiting
                            const checkInterval = setInterval(() => {
                                if (this.manager._isInitialized) {
                                    clearInterval(checkInterval);
                                    clearTimeout(managerTimeout);
                                    this.manager.off('initialized', onInitialized);
                                    managerReady = true;
                                    resolve();
                                }
                            }, 100);
                        });
                    } else {
                        clearTimeout(managerTimeout);
                        managerReady = true;
                    }
                } catch (error) {
                    console.warn('[MAVLINK-EXTENSION-UI] Manager initialization failed:', error);
                }
            }
            
            // Step 3: Try WebSocket request (max 1 second total from start)
            const startTime = Date.now();
            let dataReceived = false;
            
            if (managerReady && this.manager) {
                try {
                    console.log('[MAVLINK-EXTENSION-UI] Making WebSocket request...');
                    
                    // Calculate remaining time (max 1 second total)
                    const remainingTime = Math.max(0, 1000 - (Date.now() - startTime));
                    
                    await Promise.race([
                        this.manager.requestMavLinkExtensionData(),
                        new Promise((_, reject) => 
                            setTimeout(() => reject(new Error('WebSocket request timeout')), remainingTime)
                        )
                    ]);
                    
                    // Wait a moment for data to be processed
                    await new Promise(resolve => setTimeout(resolve, 100));
                    
                    if (this.manager.isDataLoaded()) {
                        dataReceived = true;
                        console.log('[MAVLINK-EXTENSION-UI] Data received from backend');
                    }
                } catch (error) {
                    console.warn('[MAVLINK-EXTENSION-UI] WebSocket request failed:', error);
                }
            }
            
            // Step 4: Render appropriate UI based on results
            if (dataReceived) {
                console.log('[MAVLINK-EXTENSION-UI] Rendering data UI');
                this.render();
            } else {
                console.log('[MAVLINK-EXTENSION-UI] No data received, rendering no-data UI');
                this.showNoDataState();
            }
            
            // Step 5: Bind event listeners after UI is rendered
            this.bindEventListeners();
            this.bindManagerEvents();
            
            this.isInitialized = true;
            console.log('[MAVLINK-EXTENSION-UI] UI controller initialized successfully');
            
        } catch (error) {
            console.error('[MAVLINK-EXTENSION-UI] Failed to initialize UI controller:', error);
            // Fallback: show no data state and bind listeners
            this.showNoDataState();
            this.bindEventListeners();
            this.bindManagerEvents();
            this.isInitialized = true;
        }
    }

    bindEventListeners() {
        console.log('[MAVLINK-EXTENSION-UI] Binding event listeners...');
        
        // Debug: Check if modals exist in DOM
        const importModal = document.getElementById('import-modal');
        const stopAllModal = document.getElementById('stop-all-modal');
        const deleteModal = document.getElementById('delete-endpoint-modal');
        
        console.log('[MAVLINK-EXTENSION-UI] Modal availability check:');
        console.log('  - import-modal:', importModal ? '✅ Found' : '❌ Not found');
        console.log('  - stop-all-modal:', stopAllModal ? '✅ Found' : '❌ Not found');
        console.log('  - delete-endpoint-modal:', deleteModal ? '✅ Found' : '❌ Not found');

        // Header controls
        const refreshBtn = document.getElementById('refresh-all-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.handleRefresh());
            console.log('[MAVLINK-EXTENSION-UI] Bound refresh button');
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] Refresh button not found');
        }

        // Quick action buttons
        const addEndpointBtn = document.getElementById('add-endpoint-btn');
        if (addEndpointBtn) {
            addEndpointBtn.addEventListener('click', () => this.showAddEndpointModal());
            console.log('[MAVLINK-EXTENSION-UI] Bound add endpoint button');
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] Add endpoint button not found');
        }

        const exportConfigBtn = document.getElementById('export-config-btn');
        if (exportConfigBtn) {
            exportConfigBtn.addEventListener('click', () => this.exportConfiguration());
            console.log('[MAVLINK-EXTENSION-UI] Bound export config button');
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] Export config button not found');
        }

        const importConfigBtn = document.getElementById('import-config-btn');
        if (importConfigBtn) {
            importConfigBtn.addEventListener('click', () => this.showImportModal());
            console.log('[MAVLINK-EXTENSION-UI] Bound import config button');
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] Import config button not found');
        }

        // Endpoint list controls
        const startAllBtn = document.getElementById('start-all-btn');
        if (startAllBtn) {
            startAllBtn.addEventListener('click', () => this.startAllEndpoints());
            console.log('[MAVLINK-EXTENSION-UI] Bound start all button');
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] Start all button not found');
        }

        const stopAllBtn = document.getElementById('stop-all-btn');
        if (stopAllBtn) {
            stopAllBtn.addEventListener('click', () => this.showStopAllConfirmation());
            console.log('[MAVLINK-EXTENSION-UI] Bound stop all button');
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] Stop all button not found');
        }

        // Modal controls
        this.bindModalControls();
        
        // Delete confirmation modal controls
        const cancelDeleteBtn = document.getElementById('cancel-delete-btn');
        if (cancelDeleteBtn) {
            cancelDeleteBtn.addEventListener('click', () => this.hideDeleteConfirmationModal());
        }

        const confirmDeleteBtn = document.getElementById('confirm-delete-btn');
        if (confirmDeleteBtn) {
            confirmDeleteBtn.addEventListener('click', () => this.confirmDeleteEndpoint());
        }

        // Background click to close delete modal
        if (deleteModal) {
            deleteModal.addEventListener('click', (e) => {
                if (e.target === deleteModal) this.hideDeleteConfirmationModal();
            });
        }
        
        // Import modal drag and drop
        this.bindDragAndDrop();
        
        // Toggle switches
        this.bindToggleSwitches();
        
        console.log('[MAVLINK-EXTENSION-UI] Event listeners binding complete');
    }

    bindModalControls() {
        // Endpoint modal - updated to work with new design
        const cancelBtn = document.getElementById('cancel-btn');
        const saveBtn = document.getElementById('save-btn');

        if (cancelBtn) cancelBtn.addEventListener('click', () => this.hideEndpointModal());
        if (saveBtn) saveBtn.addEventListener('click', () => this.handleEndpointFormSubmit());

        // Import modal - updated to work with drag and drop
        const cancelImportBtn = document.getElementById('cancel-import-btn');
        const importBtn = document.getElementById('import-btn');
        const dropArea = document.getElementById('drop-area');
        const configFileInput = document.getElementById('config-file');

        if (cancelImportBtn) cancelImportBtn.addEventListener('click', () => this.hideImportConfigModal());
        if (importBtn) importBtn.addEventListener('click', () => this.handleImportConfig());
        
        // Drag and drop functionality
        if (dropArea && configFileInput) {
            dropArea.addEventListener('click', () => configFileInput.click());
            
            dropArea.addEventListener('dragover', (e) => {
                e.preventDefault();
                dropArea.classList.add('border-blue-500', 'bg-blue-50');
            });
            
            dropArea.addEventListener('dragleave', () => {
                dropArea.classList.remove('border-blue-500', 'bg-blue-50');
            });
            
            dropArea.addEventListener('drop', (e) => {
                e.preventDefault();
                dropArea.classList.remove('border-blue-500', 'bg-blue-50');
                this.handleFileSelect(e.dataTransfer.files[0]);
            });
            
            configFileInput.addEventListener('change', (e) => {
                this.handleFileSelect(e.target.files[0]);
            });
        }

        // Stop all confirmation modal
        const cancelStopAllBtn = document.getElementById('cancel-stop-all-btn');
        const confirmStopAllBtn = document.getElementById('confirm-stop-all-btn');

        if (cancelStopAllBtn) cancelStopAllBtn.addEventListener('click', () => this.hideStopAllModal());
        if (confirmStopAllBtn) confirmStopAllBtn.addEventListener('click', () => this.handleStopAll());
    }

    bindDragAndDrop() {
        const dropArea = document.getElementById('drop-area');
        const fileInput = document.getElementById('config-file');
        
        if (!dropArea || !fileInput) return;

        // Click to browse
        dropArea.addEventListener('click', () => {
            fileInput.click();
        });

        // Drag and drop events
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            dropArea.addEventListener(eventName, (e) => {
                e.preventDefault();
                e.stopPropagation();
            });
        });

        ['dragenter', 'dragover'].forEach(eventName => {
            dropArea.addEventListener(eventName, () => {
                dropArea.classList.add('border-neutral-400', 'bg-neutral-100');
            });
        });

        ['dragleave', 'drop'].forEach(eventName => {
            dropArea.addEventListener(eventName, () => {
                dropArea.classList.remove('border-neutral-400', 'bg-neutral-100');
            });
        });

        dropArea.addEventListener('drop', (e) => {
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                this.handleFileSelect(files[0]);
            }
        });

        fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) {
                this.handleFileSelect(e.target.files[0]);
            }
        });
    }

    bindToggleSwitches() {
        // Auto-start toggle - updated for new design
        const autostartCheckbox = document.getElementById('endpoint-autostart');
        const autostartToggleContainer = autostartCheckbox?.parentElement?.querySelector('.w-12.h-6');
        const autostartToggleText = autostartCheckbox?.parentElement?.querySelector('.text-neutral-500');
        
        if (autostartCheckbox && autostartToggleContainer && autostartToggleText) {
            autostartToggleContainer.addEventListener('click', () => {
                autostartCheckbox.checked = !autostartCheckbox.checked;
                this.updateToggleVisual(autostartCheckbox, autostartToggleContainer, autostartToggleText);
            });
            
            // Initialize toggle state
            this.updateToggleVisual(autostartCheckbox, autostartToggleContainer, autostartToggleText);
        }
    }

    updateToggleVisual(checkbox, toggleContainer, toggleText) {
        const toggleDot = toggleContainer.querySelector('.w-5.h-5');
        
        if (checkbox.checked) {
            toggleContainer.classList.remove('bg-neutral-300');
            toggleContainer.classList.add('bg-neutral-900');
            toggleDot.classList.remove('translate-x-0.5');
            toggleDot.classList.add('translate-x-6');
            if (toggleText) toggleText.textContent = 'Enabled';
        } else {
            toggleContainer.classList.remove('bg-neutral-900');
            toggleContainer.classList.add('bg-neutral-300');
            toggleDot.classList.remove('translate-x-6');
            toggleDot.classList.add('translate-x-0.5');
            if (toggleText) toggleText.textContent = 'Disabled';
        }
    }

    handleFileSelect(file) {
        const selectedFileDiv = document.getElementById('selected-file');
        const fileName = document.getElementById('selected-file-name');
        const fileSize = document.getElementById('selected-file-size');
        
        if (selectedFileDiv && fileName && fileSize) {
            fileName.textContent = file.name;
            fileSize.textContent = `(${(file.size / 1024).toFixed(1)} KB)`;
            selectedFileDiv.classList.remove('hidden');
        }
    }

    showImportModal() {
        console.log('[MAVLINK-EXTENSION-UI] showImportModal called');
        const modal = document.getElementById('import-modal');
        console.log('[MAVLINK-EXTENSION-UI] import modal element:', modal);
        if (modal) {
            modal.classList.remove('hidden');
            console.log('[MAVLINK-EXTENSION-UI] import modal shown, classes:', modal.className);
        } else {
            console.error('[MAVLINK-EXTENSION-UI] import modal not found in DOM');
        }
    }

    hideImportModal() {
        const modal = document.getElementById('import-modal');
        if (modal) {
            modal.classList.add('hidden');
        }
    }

    showStopAllConfirmation() {
        console.log('[MAVLINK-EXTENSION-UI] showStopAllConfirmation called');
        const modal = document.getElementById('stop-all-modal');
        console.log('[MAVLINK-EXTENSION-UI] stop all modal element:', modal);
        if (modal) {
            modal.classList.remove('hidden');
            console.log('[MAVLINK-EXTENSION-UI] stop all modal shown, classes:', modal.className);
        } else {
            console.error('[MAVLINK-EXTENSION-UI] stop all modal not found in DOM');
        }
    }

    hideStopAllModal() {
        const modal = document.getElementById('stop-all-modal');
        if (modal) {
            modal.classList.add('hidden');
        }
    }

    hideImportConfigModal() {
        const modal = document.getElementById('import-modal');
        if (modal) {
            modal.classList.add('hidden');
            // Reset file selection
            const selectedFileDiv = document.getElementById('selected-file');
            if (selectedFileDiv) {
                selectedFileDiv.classList.add('hidden');
            }
            const fileInput = document.getElementById('config-file');
            if (fileInput) {
                fileInput.value = '';
            }
        }
    }

    bindManagerEvents() {
        this.manager.on('dataUpdated', () => {
            this.updateStatistics();
            // Rebind event listeners when data updates
            setTimeout(() => this.bindEventListeners(), 50);
        });
        this.manager.on('endpointAdded', () => {
            this.renderEndpoints();
            // Rebind event listeners when endpoints change
            setTimeout(() => this.bindEventListeners(), 50);
        });
        this.manager.on('endpointUpdated', () => {
            this.renderEndpoints();
            // Rebind event listeners when endpoints change
            setTimeout(() => this.bindEventListeners(), 50);
        });
        this.manager.on('endpointDeleted', () => {
            this.renderEndpoints();
            // Rebind event listeners when endpoints change
            setTimeout(() => this.bindEventListeners(), 50);
        });
        this.manager.on('alert', (alert) => this.showAlert(alert));
        this.manager.on('systemValidated', (result) => this.showValidationResult(result));
    }

    /**
     * Wait for data to load or timeout
     */
    async waitForDataLoad() {
        return new Promise((resolve) => {
            // Check if data is already loaded
            if (this.manager._isDataLoaded) {
                resolve();
                return;
            }

            let isResolved = false;

            // Listen for data load events
            const onDataLoaded = () => {
                if (isResolved) return;
                isResolved = true;
                this.manager.off('dataLoaded', onDataLoaded);
                this.manager.off('dataLoadTimeout', onDataTimeout);
                resolve();
            };

            const onDataTimeout = () => {
                if (isResolved) return;
                isResolved = true;
                this.manager.off('dataLoaded', onDataLoaded);
                this.manager.off('dataLoadTimeout', onDataTimeout);
                this.showNoDataState();
                resolve();
            };

            // Set a timeout to prevent infinite waiting
            const timeoutId = setTimeout(() => {
                if (isResolved) return;
                isResolved = true;
                this.manager.off('dataLoaded', onDataLoaded);
                this.manager.off('dataLoadTimeout', onDataTimeout);
                console.warn('[MAVLINK-EXTENSION-UI] Data loading timeout, showing no data state');
                this.showNoDataState();
                resolve();
            }, 2000); // 2 second timeout as fallback

            this.manager.on('dataLoaded', onDataLoaded);
            this.manager.on('dataLoadTimeout', onDataTimeout);

            // Also resolve if manager is already initialized but no data events fired
            if (this.manager._isInitialized) {
                setTimeout(() => {
                    if (!isResolved) {
                        isResolved = true;
                        this.manager.off('dataLoaded', onDataLoaded);
                        this.manager.off('dataLoadTimeout', onDataTimeout);
                        clearTimeout(timeoutId);
                        console.warn('[MAVLINK-EXTENSION-UI] Manager initialized but no data events, proceeding with demo data');
                        this.showNoDataState();
                        resolve();
                    }
                }, 500);
            }
        });
    }

    /**
     * Show loading state in UI
     */
    showLoadingState() {
        const container = document.getElementById('endpoints-container');
        const loadingState = document.getElementById('loading-state');
        const emptyState = document.getElementById('empty-state');
        
        if (container) {
            container.innerHTML = '';
        }
        
        if (loadingState) {
            loadingState.classList.remove('hidden');
        }
        
        if (emptyState) {
            emptyState.classList.add('hidden');
        }
        
        console.log('[MAVLINK-EXTENSION-UI] Showing loading state');
    }

    /**
     * Show no data state in UI
     */
    showNoDataState() {
        const container = document.getElementById('endpoints-container');
        const loadingState = document.getElementById('loading-state');
        const emptyState = document.getElementById('empty-state');
        
        if (container) {
            container.innerHTML = `
                <div class="col-span-full text-center py-12">
                    <div class="w-16 h-16 bg-neutral-100 rounded-full flex items-center justify-center mx-auto mb-4">
                        <i class="text-neutral-400 text-2xl fa-solid fa-wifi-slash"></i>
                    </div>
                    <h3 class="text-lg text-neutral-900 mb-2">No Data Available</h3>
                    <p class="text-sm text-neutral-600 mb-4">Unable to load MAVLink extension data from the server.</p>
                    <div class="flex justify-center gap-3">
                        <button class="px-4 py-2 bg-neutral-600 text-white rounded-lg text-sm hover:bg-neutral-700 transition-colors" onclick="window.mavlinkExtensionUI.handleRefresh()">
                            <i class="fa-solid fa-refresh mr-2"></i>
                            Try Again
                        </button>
                        <button class="px-4 py-2 bg-blue-600 text-white rounded-lg text-sm hover:bg-blue-700 transition-colors" onclick="window.mavlinkExtensionUI.addDemoEndpoint()">
                            <i class="fa-solid fa-plus mr-2"></i>
                            Add Demo Endpoint
                        </button>
                    </div>
                </div>
            `;
        }
        
        if (loadingState) {
            loadingState.classList.add('hidden');
        }
        
        if (emptyState) {
            emptyState.classList.add('hidden');
        }
        
        console.log('[MAVLINK-EXTENSION-UI] Showing no data state');
    }

    render() {
        console.log('[MAVLINK-EXTENSION-UI] Rendering UI...');
        this.updateStatistics();
        this.renderEndpoints();
        this.updateSystemStatus();
        
        // Note: Event listeners are bound in init() after render
    }

    updateStatistics() {
        const stats = this.manager.getStatistics();
        
        // Update counts
        const onlineCount = document.getElementById('online-count');
        const warningCount = document.getElementById('warning-count');
        const offlineCount = document.getElementById('offline-count');
        
        if (onlineCount) onlineCount.textContent = stats.online;
        if (warningCount) warningCount.textContent = stats.warning;
        if (offlineCount) offlineCount.textContent = stats.offline;
    }

    renderEndpoints() {
        const container = document.getElementById('endpoints-container');
        const loadingState = document.getElementById('loading-state');
        const emptyState = document.getElementById('empty-state');
        
        if (!container) return;

        const endpoints = this.manager.getEndpoints();
        
        // Hide loading state
        if (loadingState) loadingState.classList.add('hidden');
        
        if (endpoints.length === 0) {
            container.innerHTML = '';
            if (emptyState) emptyState.classList.remove('hidden');
            return;
        }
        
        if (emptyState) emptyState.classList.add('hidden');
        
        container.innerHTML = endpoints.map(endpoint => this.createEndpointCard(endpoint)).join('');
        
        // Bind endpoint card events
        this.bindEndpointCardEvents();
    }

    createEndpointCard(endpoint) {
        const statusColors = {
            online: 'bg-green-500',
            warning: 'bg-yellow-500',
            offline: 'bg-red-500'
        };

        const statusText = {
            online: 'Running',
            warning: 'Warning',
            offline: 'Stopped'
        };

        return `
            <div class="bg-white rounded-lg shadow-sm border border-neutral-200 p-6" data-endpoint-id="${endpoint.id}">
                <div class="flex items-center justify-between mb-4">
                    <div class="flex items-center gap-3">
                        <div class="w-3 h-3 ${statusColors[endpoint.status]} rounded-full"></div>
                        <h4 class="text-neutral-900">${endpoint.name}</h4>
                    </div>
                    <div class="flex items-center gap-2">
                        <button class="copy-endpoint-btn p-1 text-neutral-500 hover:text-neutral-700" data-endpoint-id="${endpoint.id}">
                            <i class="text-sm fa-solid fa-copy"></i>
                        </button>
                        <button class="edit-endpoint-btn p-1 text-neutral-500 hover:text-neutral-700" data-endpoint-id="${endpoint.id}">
                            <i class="text-sm fa-solid fa-pen-to-square"></i>
                        </button>
                        <button class="delete-endpoint-btn p-1 text-neutral-500 hover:text-neutral-700" data-endpoint-id="${endpoint.id}">
                            <i class="text-sm fa-solid fa-trash"></i>
                        </button>
                    </div>
                </div>
                
                <div class="space-y-2 mb-4">
                    <div class="flex justify-between">
                        <span class="text-sm text-neutral-600">Protocol:</span>
                        <span class="text-sm text-neutral-900">${endpoint.protocol}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-sm text-neutral-600">Host:</span>
                        <span class="text-sm text-neutral-900">${endpoint.hostIp || '192.168.1.100'}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-sm text-neutral-600">Port:</span>
                        <span class="text-sm text-neutral-900">${endpoint.port}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-sm text-neutral-600">Status:</span>
                        <div class="flex items-center gap-2">
                            <div class="w-2 h-2 ${statusColors[endpoint.status]} rounded-full"></div>
                            <span class="text-sm text-neutral-900">${statusText[endpoint.status]}</span>
                        </div>
                    </div>
                </div>
                
                ${endpoint.description ? `<p class="text-sm text-neutral-600 mb-4">${endpoint.description}</p>` : ''}
                
                <button class="w-full px-3 py-2 ${endpoint.autostart ? 'bg-neutral-100 text-neutral-700' : 'bg-neutral-50 text-neutral-500'} rounded-full text-sm ${endpoint.autostart ? 'hover:bg-neutral-200' : ''}" ${endpoint.autostart ? '' : 'disabled'}>
                    Auto-start
                </button>
            </div>
        `;
    }

    bindEndpointCardEvents() {
        // Copy buttons
        document.querySelectorAll('.copy-endpoint-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const endpointId = e.currentTarget.dataset.endpointId;
                this.copyEndpointConfig(endpointId);
            });
        });

        // Edit buttons
        document.querySelectorAll('.edit-endpoint-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const endpointId = e.currentTarget.dataset.endpointId;
                this.showEditEndpointModal(endpointId);
            });
        });

        // Delete buttons
        document.querySelectorAll('.delete-endpoint-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const endpointId = e.currentTarget.dataset.endpointId;
                this.deleteEndpoint(endpointId);
            });
        });
    }

    updateSystemStatus() {
        const statusElement = document.getElementById('system-status');
        if (!statusElement) return;

        const stats = this.manager.getStatistics();
        let status, statusClass, iconClass;

        if (stats.offline > 0) {
            status = 'Partial Outage';
            statusClass = 'bg-red-100 text-red-800';
            iconClass = 'text-red-600';
        } else if (stats.warning > 0) {
            status = 'Warning';
            statusClass = 'bg-yellow-100 text-yellow-800';
            iconClass = 'text-yellow-600';
        } else if (stats.online > 0) {
            status = 'Online';
            statusClass = 'bg-green-100 text-green-800';
            iconClass = 'text-green-600';
        } else {
            status = 'No Endpoints';
            statusClass = 'bg-gray-100 text-gray-800';
            iconClass = 'text-gray-600';
        }

        statusElement.className = `inline-flex items-center px-3 py-1 rounded-full text-sm ${statusClass}`;
        statusElement.innerHTML = `
            <i class="fa-solid fa-circle text-xs mr-2 ${iconClass}"></i>
            ${status}
        `;
    }

    // Action methods
    handleRefresh() {
        console.log('[MAVLINK-EXTENSION-UI] Refresh requested');
        
        // Show loading state
        this.showLoadingState();
        
        // Try to reload data with 1 second timeout
        if (this.manager) {
            const startTime = Date.now();
            
            try {
                Promise.race([
                    this.manager.requestMavLinkExtensionData(),
                    new Promise((_, reject) => 
                        setTimeout(() => reject(new Error('WebSocket request timeout')), 1000)
                    )
                ]).then(() => {
                    // Wait a moment for data to be processed
                    setTimeout(() => {
                        if (this.manager.isDataLoaded()) {
                            console.log('[MAVLINK-EXTENSION-UI] Refresh: Data received');
                            this.render();
                            this.bindEventListeners();
                        } else {
                            console.log('[MAVLINK-EXTENSION-UI] Refresh: No data loaded');
                            this.showNoDataState();
                            this.bindEventListeners();
                        }
                    }, 100);
                }).catch((error) => {
                    console.warn('[MAVLINK-EXTENSION-UI] Refresh failed:', error);
                    this.showNoDataState();
                    this.bindEventListeners();
                });
            } catch (error) {
                console.warn('[MAVLINK-EXTENSION-UI] Manager refresh failed:', error);
                this.showNoDataState();
                this.bindEventListeners();
            }
        } else {
            console.warn('[MAVLINK-EXTENSION-UI] No manager available for refresh');
            this.showNoDataState();
            this.bindEventListeners();
        }
    }

    addDemoEndpoint() {
        console.log('[MAVLINK-EXTENSION-UI] Adding demo endpoint');
        
        if (this.manager) {
            // Delegate to manager
            const result = this.manager.addDemoEndpoint();
            if (result) {
                this.showToast('Demo endpoint added successfully', 'success');
            } else {
                this.showToast('Failed to add demo endpoint', 'error');
            }
        } else {
            this.showToast('Manager not available. Please refresh the page.', 'error');
        }
    }

    // Modal methods
    showAddEndpointModal() {
        this.currentEditingEndpoint = null;
        const modal = document.getElementById('endpoint-modal');
        const modalTitle = document.getElementById('modal-title');
        
        if (modal && modalTitle) {
            modalTitle.textContent = 'Add MAVLink Endpoint';
            
            // Reset form fields
            document.getElementById('endpoint-name').value = '';
            document.getElementById('endpoint-protocol').value = 'TCP';
            document.getElementById('endpoint-mode').value = 'Client';
            document.getElementById('endpoint-host-ip').value = '';
            document.getElementById('endpoint-port').value = '';
            document.getElementById('endpoint-description').value = '';
            document.getElementById('endpoint-autostart').checked = false;
            
            // Reset save button text
            const saveBtn = document.getElementById('save-btn');
            if (saveBtn) saveBtn.textContent = 'Create endpoint';
            
            // Show modal
            modal.classList.remove('hidden');
        }
    }

    showEditEndpointModal(endpointId) {
        const endpoint = this.manager.getEndpoint(endpointId);
        if (!endpoint) return;

        this.currentEditingEndpoint = endpointId;
        const modal = document.getElementById('endpoint-modal');
        const modalTitle = document.getElementById('modal-title');
        
        if (modal && modalTitle) {
            modalTitle.textContent = 'Edit MAVLink Endpoint';
            
            // Populate form fields
            document.getElementById('endpoint-name').value = endpoint.name;
            document.getElementById('endpoint-protocol').value = endpoint.protocol;
            document.getElementById('endpoint-mode').value = endpoint.mode || 'Client';
            document.getElementById('endpoint-port').value = endpoint.port;
            document.getElementById('endpoint-host-ip').value = endpoint.hostIp || '';
            document.getElementById('endpoint-description').value = endpoint.description || '';
            document.getElementById('endpoint-autostart').checked = endpoint.autostart || false;
            
            // Update save button text
            const saveBtn = document.getElementById('save-btn');
            if (saveBtn) saveBtn.textContent = 'Update endpoint';
            
            modal.classList.remove('hidden');
        }
    }

    hideEndpointModal() {
        const modal = document.getElementById('endpoint-modal');
        if (modal) {
            modal.classList.add('hidden');
            this.currentEditingEndpoint = null;
        }
    }

    handleEndpointFormSubmit() {
        if (!this.manager) {
            this.showToast('Manager not available', 'error');
            return;
        }

        const formData = {
            name: document.getElementById('endpoint-name').value,
            protocol: document.getElementById('endpoint-protocol').value,
            mode: document.getElementById('endpoint-mode').value,
            port: parseInt(document.getElementById('endpoint-port').value),
            hostIp: document.getElementById('endpoint-host-ip').value,
            description: document.getElementById('endpoint-description').value,
            autostart: document.getElementById('endpoint-autostart').checked
        };

        try {
            if (this.currentEditingEndpoint) {
                // Update existing endpoint
                this.manager.updateEndpoint(this.currentEditingEndpoint, formData);
                this.showToast('Endpoint updated successfully', 'success');
            } else {
                // Add new endpoint
                this.manager.addEndpoint(formData);
                this.showToast('Endpoint added successfully', 'success');
            }
            
            this.hideEndpointModal();
        } catch (error) {
            console.error('[MAVLINK-EXTENSION-UI] Form submission failed:', error);
            this.showToast('Failed to save endpoint', 'error');
        }
    }

    handleFileSelect(file) {
        if (!file) return;
        
        // Validate file type
        if (!file.name.endsWith('.json')) {
            this.showToast('Please select a JSON configuration file', 'error');
            return;
        }
        
        // Validate file size (10MB max)
        if (file.size > 10 * 1024 * 1024) {
            this.showToast('File size exceeds 10MB limit', 'error');
            return;
        }
        
        // Update UI to show selected file
        const selectedFileDiv = document.getElementById('selected-file');
        const fileNameSpan = document.getElementById('selected-file-name');
        const fileSizeSpan = document.getElementById('selected-file-size');
        
        if (selectedFileDiv && fileNameSpan && fileSizeSpan) {
            fileNameSpan.textContent = file.name;
            fileSizeSpan.textContent = `(${(file.size / 1024).toFixed(1)} KB)`;
            selectedFileDiv.classList.remove('hidden');
        }
        
        // Store file for import
        this.selectedConfigFile = file;
    }

    handleImportConfig() {
        if (!this.selectedConfigFile) {
            this.showToast('Please select a configuration file', 'error');
            return;
        }
        
        if (!this.manager) {
            this.showToast('Manager not available', 'error');
            return;
        }
        
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const config = JSON.parse(e.target.result);
                
                // Validate configuration structure
                if (!config.endpoints || !Array.isArray(config.endpoints)) {
                    throw new Error('Invalid configuration format');
                }
                
                // Import configuration
                const imported = this.manager.importConfiguration(config);
                this.showToast(`Successfully imported ${imported} endpoints`, 'success');
                
                // Hide modal and reset
                this.hideImportConfigModal();
                this.selectedConfigFile = null;
                
            } catch (error) {
                console.error('[MAVLINK-EXTENSION-UI] Import failed:', error);
                this.showToast('Failed to import configuration: ' + error.message, 'error');
            }
        };
        
        reader.readAsText(this.selectedConfigFile);
    }

    showImportModal() {
        const modal = document.getElementById('import-modal');
        if (modal) {
            modal.classList.remove('hidden');
        }
    }

    hideImportModal() {
        const modal = document.getElementById('import-modal');
        if (modal) {
            modal.classList.add('hidden');
        }
    }

    // Action methods
    refreshAll() {
        this.render();
    }

    startEndpoint(endpointId) {
        this.manager.startEndpoint(endpointId);
    }

    stopEndpoint(endpointId) {
        this.manager.stopEndpoint(endpointId);
    }

    startAllEndpoints() {
        if (this.manager) {
            const started = this.manager.startAllEndpoints();
            this.showToast(`Started ${started} endpoints`, 'success');
        } else {
            this.showToast('Manager not available', 'error');
        }
    }

    stopAllEndpoints() {
        if (this.manager) {
            const stopped = this.manager.stopAllEndpoints();
            this.showToast(`Stopped ${stopped} endpoints`, 'info');
        } else {
            this.showToast('Manager not available', 'error');
        }
    }

    deleteEndpoint(endpointId) {
        if (!this.manager) {
            this.showToast('Manager not available', 'error');
            return;
        }

        const endpoint = this.manager.getEndpoint(endpointId);
        if (!endpoint) {
            this.showToast('Endpoint not found', 'error');
            return;
        }

        // Check if user has chosen to skip confirmation
        const skipConfirmation = localStorage.getItem('skipDeleteConfirmation') === 'true';
        
        if (skipConfirmation) {
            // Delete directly without confirmation
            const success = this.manager.deleteEndpoint(endpointId);
            if (success) {
                this.showToast('Endpoint deleted successfully', 'success');
            } else {
                this.showToast('Failed to delete endpoint', 'error');
            }
        } else {
            // Show confirmation modal
            this.showDeleteConfirmationModal(endpointId);
        }
    }

    copyEndpointConfig(endpointId) {
        if (!this.manager) {
            this.showToast('Manager not available', 'error');
            return;
        }

        const config = this.manager.getEndpointConfiguration(endpointId);
        if (!config) {
            this.showToast('Endpoint not found', 'error');
            return;
        }

        // Copy to clipboard
        const configText = JSON.stringify(config, null, 2);
        
        if (navigator.clipboard) {
            navigator.clipboard.writeText(configText).then(() => {
                this.showToast('Endpoint configuration copied to clipboard', 'success');
            }).catch((err) => {
                this.showToast('Failed to copy configuration', 'error');
            });
        } else {
            // Fallback for older browsers
            const textArea = document.createElement('textarea');
            textArea.value = configText;
            document.body.appendChild(textArea);
            textArea.select();
            try {
                document.execCommand('copy');
                this.showToast('Endpoint configuration copied to clipboard', 'success');
            } catch (err) {
                this.showToast('Failed to copy configuration', 'error');
            }
            document.body.removeChild(textArea);
        }
    }

    exportConfiguration() {
        if (!this.manager) {
            this.showToast('Manager not available', 'error');
            return;
        }

        try {
            const config = this.manager.exportConfiguration();
            const blob = new Blob([config], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            
            const a = document.createElement('a');
            a.href = url;
            a.download = `mavlink-config-${new Date().toISOString().split('T')[0]}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            this.showToast('Configuration exported successfully', 'success');
        } catch (error) {
            console.error('[MAVLINK-EXTENSION-UI] Export failed:', error);
            this.showToast('Failed to export configuration', 'error');
        }
    }

    importConfiguration() {
        if (!this.manager) {
            this.showToast('Manager not available', 'error');
            return;
        }

        const fileInput = document.getElementById('config-file');
        if (!fileInput || !fileInput.files[0]) {
            this.showToast('Please select a configuration file', 'error');
            return;
        }

        const file = fileInput.files[0];
        const reader = new FileReader();

        reader.onload = (e) => {
            try {
                const configJson = e.target.result;
                const success = this.manager.importConfiguration(configJson);
                
                if (success) {
                    this.hideImportConfigModal();
                    this.showToast('Configuration imported successfully', 'success');
                } else {
                    this.hideImportConfigModal();
                    this.showToast('Failed to import configuration', 'error');
                }
            } catch (error) {
                this.showToast('Invalid configuration file', 'error');
            }
        };
        
        reader.readAsText(file);
    }

    // Delete confirmation modal methods
    showDeleteConfirmationModal(endpointId) {
        console.log('[MAVLINK-EXTENSION-UI] showDeleteConfirmationModal called for endpoint:', endpointId);
        const endpoint = this.manager.getEndpoint(endpointId);
        if (!endpoint) return;

        this.currentDeletingEndpoint = endpointId;
        const modal = document.getElementById('delete-endpoint-modal');
        console.log('[MAVLINK-EXTENSION-UI] delete modal element:', modal);
        
        if (modal) {
            // Update modal content with endpoint details
            const endpointNameEl = document.getElementById('delete-endpoint-name');
            const endpointInfoEl = document.getElementById('delete-endpoint-info');
            
            if (endpointNameEl) {
                endpointNameEl.textContent = endpoint.name;
            }
            
            if (endpointInfoEl) {
                endpointInfoEl.textContent = `${endpoint.protocol} · ${endpoint.port}`;
            }
            
            // Reset checkbox
            const skipCheckbox = document.getElementById('delete-skip-confirmation');
            if (skipCheckbox) {
                skipCheckbox.checked = false;
            }
            
            modal.classList.remove('hidden');
            console.log('[MAVLINK-EXTENSION-UI] delete modal shown, classes:', modal.className);
        } else {
            console.error('[MAVLINK-EXTENSION-UI] delete modal not found in DOM');
        }
    }

    hideDeleteConfirmationModal() {
        const modal = document.getElementById('delete-endpoint-modal');
        if (modal) {
            modal.classList.add('hidden');
            this.currentDeletingEndpoint = null;
        }
    }

    confirmDeleteEndpoint() {
        if (!this.currentDeletingEndpoint) return;

        // Check if user wants to skip future confirmations
        const skipCheckbox = document.getElementById('delete-skip-confirmation');
        if (skipCheckbox && skipCheckbox.checked) {
            localStorage.setItem('skipDeleteConfirmation', 'true');
        }

        // Delete the endpoint using manager
        if (this.manager) {
            const success = this.manager.deleteEndpoint(this.currentDeletingEndpoint);
            this.hideDeleteConfirmationModal();
            
            if (success) {
                this.showToast('Endpoint deleted successfully', 'success');
            } else {
                this.showToast('Failed to delete endpoint', 'error');
            }
        } else {
            this.showToast('Manager not available', 'error');
            this.hideDeleteConfirmationModal();
        }
    }

    showAlert(alert) {
        const alertsSection = document.getElementById('alerts-section');
        const alertsContainer = document.getElementById('alerts-container');
        
        if (!alertsSection || !alertsContainer) return;

        alertsSection.classList.remove('hidden');
        
        const alertElement = document.createElement('div');
        alertElement.className = `p-3 rounded-lg mb-2 flex items-center justify-between ${
            alert.type === 'error' ? 'bg-red-50 text-red-800' :
            alert.type === 'warning' ? 'bg-yellow-50 text-yellow-800' :
            'bg-blue-50 text-blue-800'
        }`;
        
        alertElement.innerHTML = `
            <div class="flex items-center">
                <i class="fa-solid ${
                    alert.type === 'error' ? 'fa-circle-exclamation' :
                    alert.type === 'warning' ? 'fa-triangle-exclamation' :
                    'fa-info-circle'
                } mr-2"></i>
                <span class="text-sm">${alert.message}</span>
            </div>
            <button class="text-xs hover:opacity-70" onclick="this.parentElement.remove()">
                <i class="fa-solid fa-times"></i>
            </button>
        `;
        
        alertsContainer.insertBefore(alertElement, alertsContainer.firstChild);
        
        // Remove old alerts if too many
        while (alertsContainer.children.length > 5) {
            alertsContainer.removeChild(alertsContainer.lastChild);
        }
    }

    showToast(message, type = 'info') {
        // Simple toast implementation - could be enhanced with a proper toast library
        const toast = document.createElement('div');
        toast.className = `fixed bottom-4 right-4 px-4 py-2 rounded-lg text-white z-50 ${
            type === 'success' ? 'bg-green-600' :
            type === 'error' ? 'bg-red-600' :
            'bg-blue-600'
        }`;
        toast.textContent = message;
        
        document.body.appendChild(toast);
        
        setTimeout(() => {
            toast.remove();
        }, 3000);
    }

    cleanup() {
        // Remove event listeners and clean up UI state
        this.hideEndpointModal();
        this.hideImportConfigModal();
        this.hideStopAllModal();
        this.hideDeleteConfirmationModal();
        
        // Clear any ongoing operations
        this.currentEditingEndpoint = null;
        this.currentDeletingEndpoint = null;
        this.isInitialized = false;
        
        console.log('[MAVLINK-EXTENSION-UI] UI controller cleaned up');
    }

    /**
     * Generate modal HTML content for MAVLink extension
     */
    generateModalHTML() {
        return `
            <div id="mavlink-modals-container">
            <!-- Add/Edit Endpoint Modal -->
            <div id="endpoint-modal" class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center hidden z-50">
                <div class="bg-white rounded-2xl shadow-2xl w-[680px] p-8">
                    
                    <!-- Header Section -->
                    <div id="modal-header" class="mb-8">
                        <h2 id="modal-title" class="text-2xl text-neutral-900 mb-2">Add MAVLink Endpoint</h2>
                        <p class="text-neutral-600">Configure a new network endpoint for MAVLink communication</p>
                    </div>

                    <!-- Form Body -->
                    <div id="modal-form" class="space-y-6">
                        
                        <!-- Endpoint Name -->
                        <div class="form-group">
                            <label class="block text-sm text-neutral-900 mb-2">Endpoint name</label>
                            <input type="text" id="endpoint-name" class="w-full px-4 py-3 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-neutral-500 focus:border-neutral-500 outline-none" placeholder="Enter endpoint name">
                        </div>

                        <!-- Protocol and Mode Row -->
                        <div class="grid grid-cols-2 gap-6">
                            <div class="form-group">
                                <label class="block text-sm text-neutral-900 mb-2">Protocol</label>
                                <select id="endpoint-protocol" class="w-full px-4 py-3 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-neutral-500 focus:border-neutral-500 outline-none bg-white">
                                    <option>TCP</option>
                                    <option>UDP</option>
                                    <option>Serial</option>
                                </select>
                            </div>
                            <div class="form-group">
                                <label class="block text-sm text-neutral-900 mb-2">
                                    Mode 
                                    <span class="text-neutral-500">Optional</span>
                                </label>
                                <select id="endpoint-mode" class="w-full px-4 py-3 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-neutral-500 focus:border-neutral-500 outline-none bg-white">
                                    <option>Client</option>
                                    <option>Server</option>
                                </select>
                            </div>
                        </div>

                        <!-- Host IP and Port Row -->
                        <div class="grid grid-cols-2 gap-6">
                            <div class="form-group">
                                <label class="block text-sm text-neutral-900 mb-2">Host IP</label>
                                <input type="text" id="endpoint-host-ip" class="w-full px-4 py-3 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-neutral-500 focus:border-neutral-500 outline-none" placeholder="192.168.1.100">
                            </div>
                            <div class="form-group">
                                <label class="block text-sm text-neutral-900 mb-2">Port</label>
                                <input type="number" id="endpoint-port" class="w-full px-4 py-3 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-neutral-500 focus:border-neutral-500 outline-none" placeholder="14550">
                            </div>
                        </div>

                        <!-- Description -->
                        <div class="form-group">
                            <label class="block text-sm text-neutral-900 mb-2">
                                Description 
                                <span class="text-neutral-500">Optional</span>
                            </label>
                            <textarea id="endpoint-description" rows="2" class="w-full px-4 py-3 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-neutral-500 focus:border-neutral-500 outline-none resize-none" placeholder="Brief description of the endpoint"></textarea>
                        </div>

                        <!-- Auto-start Toggle -->
                        <div class="form-group">
                            <div class="flex items-start justify-between">
                                <div>
                                    <label class="block text-sm text-neutral-900 mb-1">Auto-start connection</label>
                                    <p class="text-sm text-neutral-600">Automatically connect when the application starts</p>
                                </div>
                                <div class="flex items-center space-x-3 mt-1">
                                    <span class="text-sm text-neutral-500">Disabled</span>
                                    <div class="relative">
                                        <input type="checkbox" id="endpoint-autostart" class="sr-only">
                                        <div class="w-12 h-6 bg-neutral-300 rounded-full cursor-pointer">
                                            <div class="w-5 h-5 bg-white rounded-full shadow-md transform translate-x-0.5 translate-y-0.5 transition-transform"></div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Bottom Actions -->
                    <div id="modal-actions" class="flex items-center justify-between mt-8 pt-6 border-t border-neutral-200">
                        <p class="text-sm text-neutral-500">You can edit or remove this endpoint later</p>
                        <div class="flex space-x-3">
                            <button id="cancel-btn" class="px-6 py-3 border border-neutral-300 text-neutral-700 rounded-lg hover:bg-neutral-50 transition-colors">
                                Cancel
                            </button>
                            <button id="save-btn" class="px-6 py-3 bg-neutral-900 text-white rounded-lg hover:bg-neutral-800 transition-colors">
                                Create endpoint
                            </button>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Import Configuration Modal -->
            <div id="import-modal" class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 hidden">
                <div id="modal-container" class="bg-white rounded-xl shadow-2xl w-full max-w-3xl mx-4 p-10">
                    
                    <!-- Header Section -->
                    <div id="modal-header" class="mb-6">
                        <h2 class="text-xl text-neutral-900 mb-2">Import Configuration</h2>
                        <p class="text-sm text-neutral-600 leading-relaxed">
                            Upload your configuration file to import settings and preferences.<br>
                            The file will be validated before applying any changes to your system.
                        </p>
                    </div>

                    <!-- Config Section -->
                    <div id="config-section" class="mb-6">
                        <!-- Label Row -->
                        <div class="flex justify-between items-center mb-3">
                            <label class="text-sm text-neutral-900">Config file</label>
                            <span class="text-xs text-neutral-500">JSON</span>
                        </div>

                        <!-- Drop Area -->
                        <div id="drop-area" class="border-2 border-dashed border-neutral-300 rounded-lg bg-neutral-50 p-12 text-center">
                            <div class="flex flex-col items-center space-y-3">
                                <div class="w-12 h-12 bg-neutral-400 rounded flex items-center justify-center">
                                    <i class="fa-solid fa-file-arrow-up text-white text-lg"></i>
                                </div>
                                <div class="space-y-1">
                                    <p class="text-base text-neutral-900">Drop your config file here</p>
                                    <p class="text-sm text-neutral-600">Click to browse or drag and drop from your desktop</p>
                                    <p class="text-xs text-neutral-500">Maximum file size: 10MB. Existing configurations will not be overwritten.</p>
                                </div>
                            </div>
                        </div>
                        <input type="file" id="config-file" accept=".json" class="hidden">
                    </div>

                    <!-- Selected File Row -->
                    <div id="selected-file" class="mb-6 hidden">
                        <div class="bg-neutral-100 rounded-lg px-4 py-3 flex items-center justify-between">
                            <div class="flex items-center space-x-3">
                                <i class="fa-solid fa-file-code text-neutral-600"></i>
                                <div>
                                    <span id="selected-file-name" class="text-sm text-neutral-900">config-settings.json</span>
                                    <span id="selected-file-size" class="text-xs text-neutral-500 ml-2">(2.4 KB)</span>
                                </div>
                            </div>
                            <span class="text-xs text-neutral-600 bg-white px-2 py-1 rounded">Ready to validate</span>
                        </div>
                    </div>

                    <!-- Footer -->
                    <div id="modal-footer" class="pt-6 border-t border-neutral-200 flex justify-between items-center">
                        <p class="text-xs text-neutral-500">
                            Configuration will be validated before import. No changes will be made until confirmation.
                        </p>
                        <div class="flex space-x-4">
                            <button id="cancel-import-btn" class="px-4 py-2 text-sm text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50 focus:outline-none focus:ring-2 focus:ring-neutral-500">
                                Cancel
                            </button>
                            <button id="import-btn" class="px-4 py-2 text-sm text-white bg-neutral-800 rounded-md hover:bg-neutral-900 focus:outline-none focus:ring-2 focus:ring-neutral-500">
                                Validate config
                            </button>
                        </div>
                    </div>

                </div>
            </div>

            <!-- Stop All Endpoints Confirmation Modal -->
            <div id="stop-all-modal" class="fixed inset-0 bg-black bg-opacity-60 flex items-center justify-center p-4 hidden z-50">
                <div id="modal-container" class="bg-white rounded-lg shadow-2xl w-full max-w-[560px] p-8">
                    
                    <div id="header-section" class="flex gap-4 mb-6">
                        <div class="w-8 h-8 bg-neutral-300 rounded flex items-center justify-center flex-shrink-0">
                            <i class="text-neutral-600 fa-solid fa-circle-exclamation"></i>
                        </div>
                        <div class="flex-1">
                            <h2 class="text-xl text-neutral-900 mb-1">Stop all MAVLink endpoints?</h2>
                            <p class="text-sm text-neutral-600">This action will immediately terminate all active MAVLink connections and endpoints running on your system.</p>
                        </div>
                    </div>

                    <div id="info-panel" class="bg-neutral-100 rounded-lg p-5 mb-6">
                        <h3 class="text-sm text-neutral-900 mb-3">What will happen</h3>
                        <ul class="space-y-2">
                            <li class="flex items-start gap-2 text-sm text-neutral-700">
                                <span class="text-neutral-500 mt-0.5">•</span>
                                <span>All active MAVLink connections will be terminated immediately</span>
                            </li>
                            <li class="flex items-start gap-2 text-sm text-neutral-700">
                                <span class="text-neutral-500 mt-0.5">•</span>
                                <span>Data streaming from connected devices will stop</span>
                            </li>
                            <li class="flex items-start gap-2 text-sm text-neutral-700">
                                <span class="text-neutral-500 mt-0.5">•</span>
                                <span>You will need to manually restart endpoints to resume operations</span>
                            </li>
                            <li class="flex items-start gap-2 text-sm text-neutral-700">
                                <span class="text-neutral-500 mt-0.5">•</span>
                                <span>Any unsaved telemetry data may be lost</span>
                            </li>
                        </ul>
                    </div>

                    <div id="confirmation-text" class="mb-6">
                        <p class="text-sm text-neutral-800">Are you sure you want to stop all MAVLink endpoints now?</p>
                    </div>

                    <div id="checkbox-section" class="mb-8">
                        <label class="flex items-center gap-2 cursor-pointer">
                            <input type="checkbox" class="w-4 h-4 rounded border-neutral-300 text-neutral-900 focus:ring-2 focus:ring-neutral-400">
                            <span class="text-sm text-neutral-700">Do not show this confirmation next time</span>
                        </label>
                    </div>

                    <div id="footer-actions" class="flex items-center justify-end gap-3">
                        <button id="cancel-stop-all-btn" class="px-5 py-2.5 rounded-md bg-white border border-neutral-300 text-neutral-700 text-sm hover:bg-neutral-50 transition-colors">
                            Cancel
                        </button>
                        <button id="confirm-stop-all-btn" class="px-5 py-2.5 rounded-md bg-neutral-900 text-white text-sm hover:bg-neutral-800 transition-colors">
                            Stop all endpoints
                        </button>
                    </div>

                </div>
            </div>

            <!-- Delete Endpoint Confirmation Modal -->
            <div id="delete-endpoint-modal" class="fixed inset-0 bg-black/60 flex items-center justify-center p-4 hidden z-50">
                <div id="delete-confirmation-card" class="bg-white rounded-lg shadow-2xl w-full max-w-[520px]">
                    
                    <!-- Header Section -->
                    <div id="card-header" class="p-6 pb-4">
                        <div class="flex gap-4">
                            <div class="flex-shrink-0">
                                <div class="w-12 h-12 rounded-lg bg-neutral-100 flex items-center justify-center">
                                    <i class="text-neutral-700 text-xl fa-regular fa-trash-can"></i>
                                </div>
                            </div>
                            <div class="flex-1 min-w-0">
                                <h2 class="text-xl text-neutral-900 mb-1">Delete MAVLink endpoint?</h2>
                                <p class="text-sm text-neutral-600">This action cannot be undone and will permanently remove the endpoint.</p>
                            </div>
                        </div>
                    </div>

                    <!-- Context Section -->
                    <div id="card-context" class="px-6 pb-4">
                        <div class="space-y-3">
                            <!-- Resource Info -->
                            <div class="flex items-center justify-between">
                                <span id="delete-endpoint-name" class="text-base text-neutral-900">Endpoint Name</span>
                                <span id="delete-endpoint-info" class="text-sm text-neutral-500">Protocol · Port</span>
                            </div>

                            <!-- Consequences Panel -->
                            <div class="bg-neutral-50 rounded-md p-4 border border-neutral-200">
                                <h3 class="text-sm text-neutral-900 mb-2">What will happen</h3>
                                <ul class="space-y-1.5 text-sm text-neutral-700">
                                    <li class="flex items-start gap-2">
                                        <span class="text-neutral-400 mt-0.5">•</span>
                                        <span>All active connections will be terminated immediately</span>
                                    </li>
                                    <li class="flex items-start gap-2">
                                        <span class="text-neutral-400 mt-0.5">•</span>
                                        <span>Endpoint configuration and settings will be lost</span>
                                    </li>
                                    <li class="flex items-start gap-2">
                                        <span class="text-neutral-400 mt-0.5">•</span>
                                        <span>Associated telemetry data streams will stop</span>
                                    </li>
                                </ul>
                            </div>
                        </div>
                    </div>

                    <!-- Confirmation Question -->
                    <div id="card-confirmation" class="px-6 pb-4">
                        <p class="text-sm text-neutral-700">Are you sure you want to delete this MAVLink endpoint?</p>
                    </div>

                    <!-- Preference Checkbox -->
                    <div id="card-preference" class="px-6 pb-5">
                        <label class="flex items-center gap-2 cursor-pointer group">
                            <input type="checkbox" id="delete-skip-confirmation" class="w-4 h-4 rounded border-neutral-300 text-neutral-900 focus:ring-2 focus:ring-neutral-900 focus:ring-offset-2 cursor-pointer">
                            <span class="text-sm text-neutral-600 group-hover:text-neutral-900 transition-colors">Do not show this confirmation next time</span>
                        </label>
                    </div>

                    <!-- Action Buttons -->
                    <div id="card-actions" class="px-6 pb-6 flex items-center justify-end gap-3">
                        <button id="cancel-delete-btn" class="px-4 py-2 text-sm text-neutral-700 hover:text-neutral-900 hover:bg-neutral-100 rounded-md transition-colors focus:outline-none focus:ring-2 focus:ring-neutral-900 focus:ring-offset-2">
                            Cancel
                        </button>
                        <button id="confirm-delete-btn" class="px-4 py-2 text-sm text-white bg-neutral-900 hover:bg-neutral-800 rounded-md transition-colors flex items-center gap-2 focus:outline-none focus:ring-2 focus:ring-neutral-900 focus:ring-offset-2">
                            <i class="text-sm fa-regular fa-trash-can"></i>
                            <span>Delete endpoint</span>
                        </button>
                    </div>

                </div>
            </div>
        `;
    }

    /**
     * Generate main content HTML for MAVLink extension
     */
    generateMainContentHTML() {
        return `
            <div id="mavlink-extension-container" class="w-full">
                <!-- MAVLink Extension System Section -->
                <div class="space-y-6" id="mavlink-extension-section">
                    <!-- Page Header -->
                    <div class="flex items-center justify-between">
                        <div>
                            <h1 class="text-2xl text-neutral-900 flex items-center">
                                <i class="fa-solid fa-satellite-dish mr-3 text-neutral-600"></i>
                                MAVLink Extension System
                            </h1>
                            <p class="text-neutral-600 mt-1">Monitor and manage MAVLink communication endpoints and extensions</p>
                        </div>
                        <div class="flex items-center space-x-2">
                            <span id="system-status" class="inline-flex items-center px-3 py-1 rounded-full text-sm bg-gray-100 text-gray-800">
                                <i class="fa-solid fa-circle text-xs mr-2"></i>
                                Loading...
                            </span>
                            <button id="refresh-all-btn" class="px-3 py-1 bg-blue-600 text-white rounded text-sm hover:bg-blue-700">
                                <i class="fa-solid fa-refresh mr-1"></i>
                                Refresh
                            </button>
                        </div>
                    </div>

                    <!-- Main Content Area -->
                    <div class="grid grid-cols-1 lg:grid-cols-4 gap-6">
                        <!-- Sidebar -->
                        <div class="lg:col-span-1">
                            <!-- Statistics Card -->
                            <div class="bg-white rounded-lg border border-neutral-200 mb-4">
                                <div class="p-4 border-b border-neutral-200">
                                    <h3 class="text-lg text-neutral-900">System Statistics</h3>
                                </div>
                                <div class="p-4">
                                    <div id="endpoint-stats" class="space-y-3 text-sm">
                                        <div class="flex items-center justify-between">
                                            <div class="flex items-center">
                                                <div class="w-2 h-2 bg-green-500 rounded-full mr-2"></div>
                                                <span>Online</span>
                                            </div>
                                            <span id="online-count">-</span>
                                        </div>
                                        <div class="flex items-center justify-between">
                                            <div class="flex items-center">
                                                <div class="w-2 h-2 bg-yellow-500 rounded-full mr-2"></div>
                                                <span>Warning</span>
                                            </div>
                                            <span id="warning-count">-</span>
                                        </div>
                                        <div class="flex items-center justify-between">
                                            <div class="flex items-center">
                                                <div class="w-2 h-2 bg-red-500 rounded-full mr-2"></div>
                                                <span>Offline</span>
                                            </div>
                                            <span id="offline-count">-</span>
                                        </div>
                                    </div>
                                </div>
                            </div>

                            <!-- Quick Actions Card -->
                            <div class="bg-white rounded-lg border border-neutral-200 p-4">
                                <h3 class="text-lg text-neutral-900 mb-4">Quick Actions</h3>
                                <div class="space-y-2">
                                    <button id="add-endpoint-btn" class="w-full bg-neutral-600 text-white p-3 rounded-lg text-sm hover:bg-neutral-700 flex items-center justify-center space-x-2">
                                        <i class="fa-solid fa-plus"></i>
                                        <span>Add Endpoint</span>
                                    </button>
                                    <button id="export-config-btn" class="w-full border border-neutral-300 text-neutral-700 p-3 rounded-lg text-sm hover:bg-neutral-50 flex items-center justify-center space-x-2">
                                        <i class="fa-solid fa-download"></i>
                                        <span>Export Config</span>
                                    </button>
                                    <button id="import-config-btn" class="w-full bg-neutral-600 text-white p-3 rounded-lg text-sm hover:bg-neutral-700 flex items-center justify-center space-x-2">
                                        <i class="fa-solid fa-upload"></i>
                                        <span>Import Config</span>
                                    </button>
                                </div>
                            </div>
                        </div>

                        <!-- Main Content -->
                        <div class="lg:col-span-3">
                            <!-- Endpoint List -->
                            <div class="bg-white rounded-lg border border-neutral-200">
                                <div class="p-4 border-b border-neutral-200">
                                    <div class="flex items-center justify-between">
                                        <h3 class="text-lg text-neutral-900">MAVLink Endpoints</h3>
                                        <div class="flex space-x-2">
                                            <button id="start-all-btn" class="px-3 py-1 bg-neutral-600 text-white rounded text-sm hover:bg-neutral-700">
                                                <i class="fa-solid fa-play mr-1"></i>
                                                Start All
                                            </button>
                                            <button id="stop-all-btn" class="px-3 py-1 bg-red-600 text-white rounded text-sm hover:bg-red-700">
                                                <i class="fa-solid fa-stop mr-1"></i>
                                                Stop All
                                            </button>
                                        </div>
                                    </div>
                                </div>

                                <div class="p-6">
                                    <div id="endpoints-container" class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4">
                                        <!-- Endpoints will be dynamically loaded here -->
                                    </div>

                                    <!-- Loading State -->
                                    <div id="loading-state" class="text-center py-8 hidden">
                                        <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                                        <p class="text-neutral-600">Loading MAVLink endpoints...</p>
                                    </div>

                                    <!-- Empty State -->
                                    <div id="empty-state" class="text-center py-8 hidden">
                                        <i class="fa-solid fa-satellite-dish text-4xl text-gray-300 mb-4"></i>
                                        <h3 class="text-lg font-medium text-gray-900 mb-2">No Endpoints Found</h3>
                                        <p class="text-gray-500 mb-4">Create your first MAVLink endpoint to get started.</p>
                                        <button class="bg-blue-600 text-white px-4 py-2 rounded hover:bg-blue-700">
                                            <i class="fa-solid fa-plus mr-2"></i>
                                            Add Endpoint
                                        </button>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
            </div>
        `;
    }
}

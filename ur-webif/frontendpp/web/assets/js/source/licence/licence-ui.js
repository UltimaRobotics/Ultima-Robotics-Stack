/**
 * Licence Page UI Controller
 * Manages the licence UI, interactions, and dynamic content with loading states
 */

import { LicenceManager } from './licence.js';
import { LicenceDataLoadingUI } from './licence-data-loading.js';

class LicenceUI {
    constructor(sourceManager = null) {
        this.sourceManager = sourceManager;
        this.licenceManager = new LicenceManager(sourceManager);
        this.loadingUI = new LicenceDataLoadingUI();
        this.initialized = false;
        this.sections = {
            activation: false,
            configuration: false,
            events: false
        };
        
        this.init();
    }
    
    init() {
        console.log('[LICENCE-UI] Initializing licence UI controller...');
        
        // Wait for licence manager to be initialized
        if (this.licenceManager.isInitialized()) {
            this.setupUI();
        } else {
            document.addEventListener('licenceDataUpdated', () => {
                this.setupUI();
            });
            
            // Fallback timeout
            setTimeout(() => {
                if (!this.initialized) {
                    this.setupUI();
                }
            }, 2000);
        }
    }
    
    setupUI() {
        this.createLicenceContent();
        this.setupEventListeners();
        
        // Show loading states for all components
        this.loadingUI.showAllLoadingStates();
        
        this.initialized = true;
        console.log('[LICENCE-UI] Licence UI controller initialized');
    }
    
    createLicenceContent() {
        // Get the main content container
        const mainContent = document.getElementById('main-content') || document.querySelector('.main-content');
        
        if (!mainContent) {
            console.error('[LICENCE-UI] Main content container not found');
            return;
        }
        
        // Create licence content from the HTML template
        mainContent.innerHTML = this.getLicenceHTMLTemplate();
    }
    
    getLicenceHTMLTemplate() {
        return `
        <div class="space-y-6">
            <!-- Page Header -->
            <div class="mb-8">
                <h2 class="text-2xl text-neutral-900 mb-2 flex items-center">
                    <i class="fa-solid fa-certificate text-neutral-700 mr-3"></i>
                    License Management
                </h2>
                <p class="text-neutral-600">Manage your software licensing, activation, and feature access</p>
            </div>

            <!-- License Status Overview -->
            <section id="license-status-overview" class="bg-white rounded-lg border border-neutral-200">
                <div class="px-6 py-4 border-b border-neutral-200">
                    <div class="flex items-center justify-between">
                        <div class="flex items-center space-x-3">
                            <h3 class="text-lg text-neutral-900">License Status Overview</h3>
                            <div id="license-status-indicator" class="flex items-center space-x-2">
                                <div class="w-2 h-2 bg-green-500 rounded-full"></div>
                                <span id="license-status-text" class="text-sm text-neutral-600">Loading...</span>
                            </div>
                        </div>
                        <div class="flex items-center space-x-3">
                            <button id="license-refresh-btn" class="text-neutral-600 hover:text-neutral-900 p-2 rounded-lg hover:bg-neutral-100" title="Refresh License Status">
                                <i class="fa-solid fa-refresh text-sm"></i>
                            </button>
                        </div>
                    </div>
                </div>

                <!-- License Information Grid -->
                <div class="p-6">
                    <div class="grid grid-cols-1 lg:grid-cols-3 gap-6">
                        <!-- License Information Card -->
                        <div class="border border-neutral-200 rounded-lg p-6">
                            <h4 class="text-lg text-neutral-800 mb-4 flex items-center">
                                <i class="fa-solid fa-info-circle text-neutral-600 mr-2"></i>
                                License Information
                            </h4>
                            <div class="space-y-3">
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">License Type:</span>
                                    <span id="license-type" class="text-neutral-800">Loading...</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">License ID:</span>
                                    <span id="license-id" class="text-neutral-800">Loading...</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">Status:</span>
                                    <span id="license-activation-status" class="px-2 py-1 text-xs rounded-full bg-green-100 text-green-800">Loading...</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">Health Score:</span>
                                    <span id="license-health-score" class="text-neutral-800">Loading...</span>
                                </div>
                            </div>
                        </div>

                        <!-- Validity Information Card -->
                        <div class="border border-neutral-200 rounded-lg p-6">
                            <h4 class="text-lg text-neutral-800 mb-4 flex items-center">
                                <i class="fa-solid fa-calendar text-neutral-600 mr-2"></i>
                                Validity Information
                            </h4>
                            <div class="space-y-3">
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">Start Date:</span>
                                    <span id="license-start-date" class="text-neutral-800">Loading...</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">Expiry Date:</span>
                                    <span id="license-expiry-date" class="text-neutral-800">Loading...</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">Days Remaining:</span>
                                    <span id="license-remaining-days" class="text-neutral-800">Loading...</span>
                                </div>
                                <div class="flex justify-between">
                                    <span class="text-neutral-600">Next Validation:</span>
                                    <span id="license-validation-days" class="text-neutral-800">Loading...</span>
                                </div>
                            </div>
                        </div>

                        <!-- Features Card -->
                        <div class="border border-neutral-200 rounded-lg p-6">
                            <h4 class="text-lg text-neutral-800 mb-4 flex items-center">
                                <i class="fa-solid fa-list-check text-neutral-600 mr-2"></i>
                                Enabled Features
                            </h4>
                            <div id="license-features-list" class="space-y-2 max-h-48 overflow-y-auto">
                                <!-- Features will be populated by loading UI -->
                            </div>
                        </div>
                    </div>

                    <!-- Health Status Bar -->
                    <div class="mt-6 p-4 bg-neutral-50 rounded-lg">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-neutral-800">License Health Score</span>
                            <span id="license-health-percentage" class="text-neutral-900 font-semibold">Loading...</span>
                        </div>
                        <div class="w-full bg-gray-200 rounded-full h-2">
                            <div id="license-health-bar" class="bg-green-600 h-2 rounded-full transition-all duration-500" style="width: 0%"></div>
                        </div>
                        <p class="text-neutral-600 text-sm mt-2">Loading license health information...</p>
                    </div>
                </div>
            </section>

            <!-- License Activation Section -->
            <section id="license-activation-section" class="bg-white rounded-lg border border-neutral-200">
                <div class="px-6 py-4 border-b border-neutral-200 cursor-pointer hover:bg-neutral-50" id="license-activation-header">
                    <div class="flex items-center justify-between">
                        <div>
                            <h3 class="text-lg text-neutral-900">License Activation</h3>
                            <p class="text-sm text-neutral-600 mt-1">Activate your license using key, file upload, or online activation</p>
                        </div>
                        <i class="fa-solid fa-chevron-down text-neutral-500 transition-transform duration-200 rotate-180" id="license-activation-chevron"></i>
                    </div>
                </div>

                <div class="p-6 hidden" id="license-activation-content">
                    <div class="grid grid-cols-1 lg:grid-cols-3 gap-6">
                        <!-- License Key Activation -->
                        <div class="border border-neutral-200 rounded-lg p-6">
                            <h4 class="text-lg text-neutral-800 mb-4 flex items-center">
                                <i class="fa-solid fa-key text-neutral-600 mr-2"></i>
                                License Key
                            </h4>
                            <form id="license-key-form" class="space-y-4">
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Enter License Key</label>
                                    <input type="text" id="license-key-input" placeholder="XXXX-XXXX-XXXX-XXXX" 
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-neutral-500">
                                </div>
                                <button type="submit" id="activate-license-key-btn" 
                                        class="w-full bg-neutral-600 hover:bg-neutral-700 text-white px-4 py-2 rounded-lg text-sm">
                                    <i class="fa-solid fa-key mr-2"></i>
                                    Activate License
                                </button>
                            </form>
                        </div>

                        <!-- File Upload Activation -->
                        <div class="border border-neutral-200 rounded-lg p-6">
                            <h4 class="text-lg text-neutral-800 mb-4 flex items-center">
                                <i class="fa-solid fa-upload text-neutral-600 mr-2"></i>
                                File Upload
                            </h4>
                            <div class="space-y-4">
                                <div id="license-file-drop-zone" class="border-2 border-dashed border-neutral-300 rounded-lg p-6 text-center hover:border-neutral-400 transition-colors cursor-pointer">
                                    <input type="file" id="license-file-input" accept=".lic,.dat,.key" style="display: none;">
                                    <div class="w-12 h-12 bg-neutral-100 rounded-full flex items-center justify-center mx-auto mb-3">
                                        <i class="fa-solid fa-cloud-upload text-neutral-400 text-xl"></i>
                                    </div>
                                    <p class="text-neutral-700">Drop license file here</p>
                                    <p class="text-neutral-500 text-sm">or click to browse</p>
                                </div>
                                <button id="upload-activate-btn" disabled
                                        class="w-full bg-neutral-400 text-white px-4 py-2 rounded-lg text-sm cursor-not-allowed">
                                    <i class="fa-solid fa-upload mr-2"></i>
                                    Upload & Activate
                                </button>
                            </div>
                        </div>

                        <!-- Online Activation -->
                        <div class="border border-neutral-200 rounded-lg p-6">
                            <h4 class="text-lg text-neutral-800 mb-4 flex items-center">
                                <i class="fa-solid fa-globe text-neutral-600 mr-2"></i>
                                Online Activation
                            </h4>
                            <div class="space-y-4">
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Product Code</label>
                                    <input type="text" id="product-code-input" placeholder="Enter product code" 
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-neutral-500">
                                </div>
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Email Address</label>
                                    <input type="email" id="email-input" placeholder="your@email.com" 
                                           class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-neutral-500">
                                </div>
                                <button id="online-activate-btn" 
                                        class="w-full bg-neutral-600 hover:bg-neutral-700 text-white px-4 py-2 rounded-lg text-sm">
                                    <i class="fa-solid fa-globe mr-2"></i>
                                    Auto-Activate Online
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            </section>

            <!-- License Configuration Section -->
            <section id="license-configuration-section" class="bg-white rounded-lg border border-neutral-200">
                <div class="px-6 py-4 border-b border-neutral-200 cursor-pointer hover:bg-neutral-50" id="license-configuration-header">
                    <div class="flex items-center justify-between">
                        <div>
                            <h3 class="text-lg text-neutral-900">License Configuration</h3>
                            <p class="text-sm text-neutral-600 mt-1">Configure license validation and security settings</p>
                        </div>
                        <i class="fa-solid fa-chevron-down text-neutral-500 transition-transform duration-200 rotate-180" id="license-configuration-chevron"></i>
                    </div>
                </div>

                <div class="p-6 hidden" id="license-configuration-content">
                    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
                        <!-- Validation Settings -->
                        <div class="space-y-4">
                            <h4 class="text-sm font-medium text-neutral-800">Validation Settings</h4>
                            
                            <div class="flex items-center justify-between">
                                <div>
                                    <h5 class="text-sm text-neutral-700">Signature Validation</h5>
                                    <p class="text-xs text-neutral-500">Verify license signatures</p>
                                </div>
                                <div id="signature-validation-toggle" class="w-12 h-6 bg-neutral-600 rounded-full relative cursor-pointer" data-setting="signature_validation">
                                    <div class="absolute right-1 top-1 bg-white w-4 h-4 rounded-full transition-transform"></div>
                                </div>
                            </div>

                            <div class="flex items-center justify-between">
                                <div>
                                    <h5 class="text-sm text-neutral-700">Auto Renewal</h5>
                                    <p class="text-xs text-neutral-500">Automatically renew before expiry</p>
                                </div>
                                <div id="auto-renewal-toggle" class="w-12 h-6 bg-neutral-300 rounded-full relative cursor-pointer" data-setting="auto_renewal">
                                    <div class="absolute left-1 top-1 bg-white w-4 h-4 rounded-full transition-transform"></div>
                                </div>
                            </div>

                            <div>
                                <label class="block text-sm text-neutral-700 mb-2">Validation Interval (Days)</label>
                                <input type="number" id="validation-interval-input" value="7" min="1" max="30"
                                       class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-neutral-500">
                            </div>
                        </div>

                        <!-- Security Settings -->
                        <div class="space-y-4">
                            <h4 class="text-sm font-medium text-neutral-800">Security Settings</h4>
                            
                            <div class="flex items-center justify-between">
                                <div>
                                    <h5 class="text-sm text-neutral-700">Require HTTPS</h5>
                                    <p class="text-xs text-neutral-500">Force secure connections</p>
                                </div>
                                <div id="require-https-toggle" class="w-12 h-6 bg-neutral-600 rounded-full relative cursor-pointer" data-setting="require_https">
                                    <div class="absolute right-1 top-1 bg-white w-4 h-4 rounded-full transition-transform"></div>
                                </div>
                            </div>

                            <div class="flex items-center justify-between">
                                <div>
                                    <h5 class="text-sm text-neutral-700">Log Activation Attempts</h5>
                                    <p class="text-xs text-neutral-500">Record all activation events</p>
                                </div>
                                <div id="log-attempts-toggle" class="w-12 h-6 bg-neutral-600 rounded-full relative cursor-pointer" data-setting="log_activation_attempts">
                                    <div class="absolute right-1 top-1 bg-white w-4 h-4 rounded-full transition-transform"></div>
                                </div>
                            </div>

                            <div>
                                <label class="block text-sm text-neutral-700 mb-2">Max File Size (MB)</label>
                                <input type="number" id="max-file-size-input" value="5" min="1" max="50"
                                       class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-neutral-500">
                            </div>
                        </div>
                    </div>

                    <div class="mt-6 pt-6 border-t border-neutral-200">
                        <div class="flex justify-end space-x-3">
                            <button id="reset-config-btn" class="px-4 py-2 border border-neutral-300 text-neutral-700 rounded-lg hover:bg-neutral-50">
                                Reset to Defaults
                            </button>
                            <button id="save-config-btn" class="px-4 py-2 bg-neutral-600 hover:bg-neutral-700 text-white rounded-lg">
                                Save Configuration
                            </button>
                        </div>
                    </div>
                </div>
            </section>

            <!-- License Events Section -->
            <section id="license-events-section" class="bg-white rounded-lg border border-neutral-200">
                <div class="px-6 py-4 border-b border-neutral-200 cursor-pointer hover:bg-neutral-50" id="license-events-header">
                    <div class="flex items-center justify-between">
                        <div>
                            <h3 class="text-lg text-neutral-900">License Events</h3>
                            <p class="text-sm text-neutral-600 mt-1">Recent license activity and events</p>
                        </div>
                        <div class="flex items-center space-x-3">
                            <button id="clear-events-btn" class="text-neutral-600 hover:text-neutral-900 px-3 py-1 rounded border border-neutral-300 text-sm">
                                Clear Events
                            </button>
                            <i class="fa-solid fa-chevron-down text-neutral-500 transition-transform duration-200 rotate-180" id="license-events-chevron"></i>
                        </div>
                    </div>
                </div>

                <div class="p-6 hidden" id="license-events-content">
                    <div id="license-events-container" class="space-y-3 max-h-64 overflow-y-auto">
                        <!-- Events will be populated by loading UI -->
                    </div>
                    
                    <div id="license-no-events" class="text-center py-8 text-neutral-500" style="display: none;">
                        <i class="fa-solid fa-calendar-xmark text-2xl mb-3"></i>
                        <p>No license events recorded</p>
                    </div>
                </div>
            </section>
        </div>`;
    }
    
    setupEventListeners() {
        // Section toggles
        this.setupSectionToggle('license-activation-header', 'license-activation-content', 'license-activation-chevron');
        this.setupSectionToggle('license-configuration-header', 'license-configuration-content', 'license-configuration-chevron');
        this.setupSectionToggle('license-events-header', 'license-events-content', 'license-events-chevron');
        
        // Refresh button
        const refreshBtn = document.getElementById('license-refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.refreshLicenceStatus());
        }
        
        // License key activation
        const keyForm = document.getElementById('license-key-form');
        if (keyForm) {
            keyForm.addEventListener('submit', (e) => this.handleKeyActivation(e));
        }
        
        // File upload
        this.setupFileUpload();
        
        // Online activation
        const onlineBtn = document.getElementById('online-activate-btn');
        if (onlineBtn) {
            onlineBtn.addEventListener('click', () => this.handleOnlineActivation());
        }
        
        // Configuration toggles
        this.setupConfigurationToggles();
        
        // Configuration buttons
        const saveConfigBtn = document.getElementById('save-config-btn');
        if (saveConfigBtn) {
            saveConfigBtn.addEventListener('click', () => this.saveConfiguration());
        }
        
        const resetConfigBtn = document.getElementById('reset-config-btn');
        if (resetConfigBtn) {
            resetConfigBtn.addEventListener('click', () => this.resetConfiguration());
        }
        
        // Clear events button
        const clearEventsBtn = document.getElementById('clear-events-btn');
        if (clearEventsBtn) {
            clearEventsBtn.addEventListener('click', () => this.clearEvents());
        }
        
        // Listen for licence manager events
        document.addEventListener('licenceEventAdded', (e) => this.onEventAdded(e));
        document.addEventListener('licenceDataUpdated', (e) => this.onLicenceDataUpdated(e));
        document.addEventListener('licenceConfigurationUpdated', (e) => this.onConfigurationUpdated(e));
    }
    
    setupSectionToggle(headerId, contentId, chevronId) {
        const header = document.getElementById(headerId);
        const content = document.getElementById(contentId);
        const chevron = document.getElementById(chevronId);
        
        if (header && content && chevron) {
            header.addEventListener('click', () => {
                const isHidden = content.classList.contains('hidden');
                
                if (isHidden) {
                    content.classList.remove('hidden');
                    chevron.classList.remove('rotate-180');
                } else {
                    content.classList.add('hidden');
                    chevron.classList.add('rotate-180');
                }
            });
        }
    }
    
    setupFileUpload() {
        const dropZone = document.getElementById('license-file-drop-zone');
        const fileInput = document.getElementById('license-file-input');
        const uploadBtn = document.getElementById('upload-activate-btn');
        
        if (dropZone && fileInput && uploadBtn) {
            // Click to browse
            dropZone.addEventListener('click', () => fileInput.click());
            
            // File selection
            fileInput.addEventListener('change', (e) => this.handleFileSelection(e));
            
            // Drag and drop
            dropZone.addEventListener('dragover', (e) => {
                e.preventDefault();
                dropZone.classList.add('border-neutral-500', 'bg-neutral-50');
            });
            
            dropZone.addEventListener('dragleave', () => {
                dropZone.classList.remove('border-neutral-400', 'bg-neutral-50');
            });
            
            dropZone.addEventListener('drop', (e) => {
                e.preventDefault();
                dropZone.classList.remove('border-neutral-400', 'bg-neutral-50');
                
                const files = e.dataTransfer.files;
                if (files.length > 0) {
                    fileInput.files = files;
                    this.handleFileSelection({ target: { files } });
                }
            });
            
            // Upload button
            uploadBtn.addEventListener('click', () => {
                this.handleFileUpload();
            });
        }
    }
    
    setupConfigurationToggles() {
        const toggles = document.querySelectorAll('[data-setting]');
        toggles.forEach(toggle => {
            toggle.addEventListener('click', () => {
                this.handleToggleClick(toggle);
            });
        });
    }
    
    setupRefreshButton() {
        const refreshBtn = document.getElementById('license-refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.refreshLicenceStatus();
            });
        }
    }
    
    setupLicenceEventListeners() {
        // Listen for licence manager events
        document.addEventListener('licenceEventAdded', (e) => {
            this.onEventAdded(e);
        });
        
        document.addEventListener('licenceDataUpdated', (e) => {
            this.onLicenceDataUpdated(e);
        });
        
        document.addEventListener('licenceConfigurationUpdated', (e) => {
            this.onConfigurationUpdated(e);
        });
    }
    
    // Event handlers
    async handleKeyActivation(e) {
        e.preventDefault();
        
        const keyInput = document.getElementById('license-key-input');
        const activateBtn = document.getElementById('activate-license-key-btn');
        
        if (!keyInput || !keyInput.value.trim()) {
            this.showMessage('Please enter a license key', 'error');
            return;
        }
        
        // Show loading state
        if (activateBtn) {
            activateBtn.disabled = true;
            activateBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Activating...';
        }
        
        try {
            const result = await this.licenceManager.activateWithKey(keyInput.value.trim());
            
            if (result.success) {
                this.showMessage('License activated successfully', 'success');
                keyInput.value = '';
                // Show loading state again for refresh
                this.loadingUI.showLicenceStatusLoading();
            } else {
                this.showMessage(result.error || 'Activation failed', 'error');
            }
        } catch (error) {
            this.showMessage('Activation failed: ' + error.message, 'error');
        } finally {
            // Reset button
            if (activateBtn) {
                activateBtn.disabled = false;
                activateBtn.innerHTML = '<i class="fa-solid fa-key mr-2"></i>Activate License';
            }
        }
    }
    
    handleFileSelection(e) {
        const file = e.target.files[0];
        const uploadBtn = document.getElementById('upload-activate-btn');
        
        if (file && uploadBtn) {
            uploadBtn.disabled = false;
            uploadBtn.className = 'w-full bg-neutral-600 hover:bg-neutral-700 text-white px-4 py-2 rounded-lg text-sm';
            uploadBtn.innerHTML = `<i class="fa-solid fa-upload mr-2"></i>Upload & Activate (${file.name})`;
        }
    }
    
    async handleFileUpload() {
        const fileInput = document.getElementById('license-file-input');
        const uploadBtn = document.getElementById('upload-activate-btn');
        
        if (!fileInput || !fileInput.files[0]) {
            this.showMessage('Please select a license file', 'error');
            return;
        }
        
        // Show loading state
        if (uploadBtn) {
            uploadBtn.disabled = true;
            uploadBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Uploading...';
        }
        
        try {
            const result = await this.licenceManager.activateWithFile(fileInput.files[0]);
            
            if (result.success) {
                this.showMessage('License file uploaded and activated successfully', 'success');
                this.resetFileUploadUI();
                // Show loading state again for refresh
                this.loadingUI.showLicenceStatusLoading();
            } else {
                this.showMessage(result.error || 'Upload failed', 'error');
            }
        } catch (error) {
            this.showMessage('Upload failed: ' + error.message, 'error');
        } finally {
            // Reset button
            if (uploadBtn) {
                uploadBtn.disabled = false;
                uploadBtn.innerHTML = '<i class="fa-solid fa-upload mr-2"></i>Upload & Activate';
            }
        }
    }
    
    async handleOnlineActivation() {
        const productCodeInput = document.getElementById('product-code-input');
        const emailInput = document.getElementById('email-input');
        const activateBtn = document.getElementById('online-activate-btn');
        
        if (!productCodeInput || !productCodeInput.value.trim()) {
            this.showMessage('Please enter a product code', 'error');
            return;
        }
        
        if (!emailInput || !emailInput.value.trim()) {
            this.showMessage('Please enter an email address', 'error');
            return;
        }
        
        // Show loading state
        if (activateBtn) {
            activateBtn.disabled = true;
            activateBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Activating...';
        }
        
        try {
            const result = await this.licenceManager.activateOnline(
                productCodeInput.value.trim(),
                emailInput.value.trim()
            );
            
            if (result.success) {
                this.showMessage('Online activation completed successfully', 'success');
                productCodeInput.value = '';
                emailInput.value = '';
                // Show loading state again for refresh
                this.loadingUI.showLicenceStatusLoading();
            } else {
                this.showMessage(result.error || 'Online activation failed', 'error');
            }
        } catch (error) {
            this.showMessage('Online activation failed: ' + error.message, 'error');
        } finally {
            // Reset button
            if (activateBtn) {
                activateBtn.disabled = false;
                activateBtn.innerHTML = '<i class="fa-solid fa-globe mr-2"></i>Auto-Activate Online';
            }
        }
    }
    
    handleToggleClick(toggle) {
        const setting = toggle.getAttribute('data-setting');
        const isEnabled = toggle.classList.contains('bg-neutral-600');
        
        // Update toggle appearance
        if (isEnabled) {
            toggle.classList.remove('bg-neutral-600');
            toggle.classList.add('bg-neutral-300');
            const indicator = toggle.querySelector('div');
            if (indicator) {
                indicator.classList.remove('right-1');
                indicator.classList.add('left-1');
            }
        } else {
            toggle.classList.remove('bg-neutral-300');
            toggle.classList.add('bg-neutral-600');
            const indicator = toggle.querySelector('div');
            if (indicator) {
                indicator.classList.remove('left-1');
                indicator.classList.add('right-1');
            }
        }
        
        // Update configuration
        this.updateConfiguration(setting, !isEnabled);
    }
    
    async saveConfiguration() {
        const saveBtn = document.getElementById('save-config-btn');
        
        // Get configuration values
        const config = {
            validation_interval: parseInt(document.getElementById('validation-interval-input')?.value || 7),
            max_file_size: parseInt(document.getElementById('max-file-size-input')?.value || 5),
            signature_validation: document.getElementById('signature-validation-toggle')?.classList.contains('bg-neutral-600') || false,
            auto_renewal: document.getElementById('auto-renewal-toggle')?.classList.contains('bg-neutral-600') || false,
            require_https: document.getElementById('require-https-toggle')?.classList.contains('bg-neutral-600') || false,
            log_activation_attempts: document.getElementById('log-attempts-toggle')?.classList.contains('bg-neutral-600') || false
        };
        
        // Show loading state
        if (saveBtn) {
            saveBtn.disabled = true;
            saveBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Saving...';
        }
        
        try {
            const result = await this.licenceManager.updateConfiguration(config);
            
            if (result.success) {
                this.showMessage('Configuration saved successfully', 'success');
            } else {
                this.showMessage(result.error || 'Failed to save configuration', 'error');
            }
        } catch (error) {
            this.showMessage('Failed to save configuration: ' + error.message, 'error');
        } finally {
            // Reset button
            if (saveBtn) {
                saveBtn.disabled = false;
                saveBtn.innerHTML = 'Save Configuration';
            }
        }
    }
    
    async resetConfiguration() {
        const resetBtn = document.getElementById('reset-config-btn');
        
        // Show loading state
        if (resetBtn) {
            resetBtn.disabled = true;
            resetBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Resetting...';
        }
        
        try {
            const result = await this.licenceManager.resetConfiguration();
            
            if (result.success) {
                this.showMessage('Configuration reset to defaults', 'success');
                // Update UI with new configuration
                this.updateConfigurationUI();
            } else {
                this.showMessage(result.error || 'Failed to reset configuration', 'error');
            }
        } catch (error) {
            this.showMessage('Failed to reset configuration: ' + error.message, 'error');
        } finally {
            // Reset button
            if (resetBtn) {
                resetBtn.disabled = false;
                resetBtn.innerHTML = 'Reset to Defaults';
            }
        }
    }
    
    async clearEvents() {
        try {
            const result = await this.licenceManager.clearEvents();
            
            if (result.success) {
                this.showMessage('Events cleared successfully', 'success');
                // Show loading state for events
                this.loadingUI.showLicenceEventsLoading();
            } else {
                this.showMessage(result.error || 'Failed to clear events', 'error');
            }
        } catch (error) {
            this.showMessage('Failed to clear events: ' + error.message, 'error');
        }
    }
    
    async refreshLicenceStatus() {
        // Show loading state
        this.loadingUI.showRefreshButtonLoading();
        this.loadingUI.showLicenceStatusLoading();
        
        try {
            const result = await this.licenceManager.refreshLicenceStatus();
            
            if (result.success) {
                this.showMessage('License status refreshed', 'success');
                // Keep loading state as we don't have actual data
            } else {
                this.showMessage(result.error || 'Failed to refresh license status', 'error');
            }
        } catch (error) {
            this.showMessage('Failed to refresh license status: ' + error.message, 'error');
        } finally {
            // Hide refresh button loading
            this.loadingUI.hideRefreshButtonLoading();
        }
    }
    
    // Event listeners for licence manager updates
    onEventAdded(e) {
        const event = e.detail.event;
        this.updateEventsList();
    }
    
    onLicenceDataUpdated(e) {
        this.updateLicenceStatus();
    }
    
    onConfigurationUpdated(e) {
        this.updateConfigurationUI();
    }
    
    // UI update methods (now use loading states)
    updateLicenceStatus() {
        // Keep loading state as we don't have actual data
        console.log('[LICENCE-UI] Licence status updated - keeping loading state');
    }
    
    updateConfigurationUI() {
        const config = this.licenceManager.getConfiguration();
        
        // Update input values
        this.updateInput('validation-interval-input', config.validation_interval);
        this.updateInput('max-file-size-input', config.max_file_size);
        
        // Update toggle states
        this.updateToggle('signature-validation-toggle', config.signature_validation);
        this.updateToggle('auto-renewal-toggle', config.auto_renewal);
        this.updateToggle('require-https-toggle', config.require_https);
        this.updateToggle('log-attempts-toggle', config.log_activation_attempts);
    }
    
    updateEventsList() {
        const events = this.licenceManager.getEvents();
        const container = document.getElementById('license-events-container');
        const noEvents = document.getElementById('license-no-events');
        
        if (!container || !noEvents) return;
        
        if (events.length === 0) {
            container.style.display = 'none';
            noEvents.style.display = 'block';
        } else {
            container.style.display = 'block';
            noEvents.style.display = 'none';
            
            // Show recent events (keep loading state for older ones)
            const recentEvents = events.slice(0, 5);
            container.innerHTML = recentEvents.map(event => this.createEventHTML(event)).join('');
        }
    }
    
    // Utility methods
    updateElement(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }
    
    updateToggle(id, enabled) {
        const toggle = document.getElementById(id);
        if (toggle) {
            if (enabled) {
                toggle.classList.remove('bg-neutral-300');
                toggle.classList.add('bg-neutral-600');
                const indicator = toggle.querySelector('div');
                if (indicator) {
                    indicator.classList.remove('left-1');
                    indicator.classList.add('right-1');
                }
            } else {
                toggle.classList.remove('bg-neutral-600');
                toggle.classList.add('bg-neutral-300');
                const indicator = toggle.querySelector('div');
                if (indicator) {
                    indicator.classList.remove('right-1');
                    indicator.classList.add('left-1');
                }
            }
        }
    }
    
    updateHealthBar(score) {
        const percentage = document.getElementById('license-health-percentage');
        const bar = document.getElementById('license-health-bar');
        
        if (percentage) percentage.textContent = `${score}%`;
        if (bar) bar.style.width = `${score}%`;
    }
    
    updateFeaturesList(features) {
        const container = document.getElementById('license-features-list');
        if (container && features) {
            container.innerHTML = features.map(feature => 
                `<div class="flex items-center space-x-2">
                    <i class="fa-solid fa-check text-green-500 text-xs"></i>
                    <span class="text-sm text-neutral-700">${feature}</span>
                </div>`
            ).join('');
        }
    }
    
    createEventHTML(event) {
        const iconClass = this.getEventIconClass(event.type);
        const bgClass = this.getEventBgClass(event.type);
        const formattedDate = this.formatDateTime(event.timestamp);
        
        return `
            <div class="flex items-start space-x-3 p-3 rounded-lg ${bgClass}">
                <i class="${iconClass} text-sm mt-0.5"></i>
                <div class="flex-1">
                    <p class="text-sm text-neutral-800">${event.message}</p>
                    <p class="text-xs text-neutral-500 mt-1">${formattedDate}</p>
                </div>
            </div>
        `;
    }
    
    getStatusClass(status) {
        switch (status) {
            case 'active': return 'bg-green-100 text-green-800';
            case 'expired': return 'bg-red-100 text-red-800';
            case 'pending': return 'bg-yellow-100 text-yellow-800';
            case 'suspended': return 'bg-orange-100 text-orange-800';
            default: return 'bg-gray-100 text-gray-800';
        }
    }
    
    getEventIconClass(type) {
        switch (type) {
            case 'success': return 'fa-solid fa-check-circle text-green-500';
            case 'error': return 'fa-solid fa-exclamation-circle text-red-500';
            case 'warning': return 'fa-solid fa-exclamation-triangle text-yellow-500';
            case 'info': return 'fa-solid fa-info-circle text-blue-500';
            default: return 'fa-solid fa-circle text-gray-500';
        }
    }
    
    getEventBgClass(type) {
        switch (type) {
            case 'success': return 'bg-green-50';
            case 'error': return 'bg-red-50';
            case 'warning': return 'bg-yellow-50';
            case 'info': return 'bg-blue-50';
            default: return 'bg-neutral-50';
        }
    }
    
    formatDate(dateString) {
        const date = new Date(dateString);
        return date.toLocaleDateString('en-US', { 
            year: 'numeric', 
            month: 'short', 
            day: 'numeric' 
        });
    }
    
    formatDateTime(dateString) {
        const date = new Date(dateString);
        return date.toLocaleDateString('en-US', { 
            year: 'numeric', 
            month: 'short', 
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit'
        });
    }
    
    showMessage(message, type = 'info') {
        // Create toast notification
        const toast = document.createElement('div');
        toast.className = `fixed top-4 right-4 px-6 py-3 rounded-lg shadow-lg z-50 transition-all duration-300 transform translate-x-full`;
        
        // Set color based on type
        switch (type) {
            case 'success':
                toast.className += ' bg-green-500 text-white';
                break;
            case 'error':
                toast.className += ' bg-red-500 text-white';
                break;
            case 'warning':
                toast.className += ' bg-yellow-500 text-white';
                break;
            default:
                toast.className += ' bg-blue-500 text-white';
        }
        
        toast.innerHTML = `
            <div class="flex items-center">
                <i class="fa-solid fa-${type === 'success' ? 'check' : type === 'error' ? 'exclamation' : 'info'}-circle mr-2"></i>
                <span>${message}</span>
            </div>
        `;
        
        document.body.appendChild(toast);
        
        // Animate in
        setTimeout(() => {
            toast.classList.remove('translate-x-full');
        }, 100);
        
        // Remove after 3 seconds
        setTimeout(() => {
            toast.classList.add('translate-x-full');
            setTimeout(() => {
                if (toast.parentNode) {
                    toast.parentNode.removeChild(toast);
                }
            }, 300);
        }, 3000);
    }
    
    resetFileUploadUI() {
        const fileInput = document.getElementById('license-file-input');
        const uploadBtn = document.getElementById('upload-activate-btn');
        
        if (fileInput) fileInput.value = '';
        if (uploadBtn) {
            uploadBtn.disabled = true;
            uploadBtn.className = 'w-full bg-neutral-400 text-white px-4 py-2 rounded-lg text-sm cursor-not-allowed';
            uploadBtn.innerHTML = '<i class="fa-solid fa-upload mr-2"></i>Upload & Activate';
        }
    }
    
    // Public methods for external use
    initialize() {
        if (!this.initialized) {
            this.setupUI();
        }
    }
    
    refreshAllData() {
        this.loadingUI.showAllLoadingStates();
        this.refreshLicenceStatus();
    }
    
    // Cleanup method
    destroy() {
        // Remove event listeners
        document.removeEventListener('licenceEventAdded', this.onEventAdded);
        document.removeEventListener('licenceDataUpdated', this.onLicenceDataUpdated);
        document.removeEventListener('licenceConfigurationUpdated', this.onConfigurationUpdated);
        
        // Cleanup licence manager
        if (this.licenceManager) {
            this.licenceManager.destroy();
        }
        
        // Cleanup loading UI
        if (this.loadingUI) {
            this.loadingUI.destroy();
        }
        
        this.initialized = false;
    }
}

// Export for use in other modules
export { LicenceUI };

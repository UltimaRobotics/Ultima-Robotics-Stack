/**
 * Firmware Upgrade UI Controller
 * Manages the firmware upgrade page interface, user interactions, and dynamic content rendering
 */

import { FirmwareUpdateDataLoadingUI } from './firmware-update-data-loading.js';

class FirmwareUpgradeUI {
    constructor(firmwareUpgradeManager) {
        this.firmwareUpgradeManager = firmwareUpgradeManager;
        this.currentUpgradeSource = 'upload-file';
        this.selectedFile = null;
        this.uploadProgress = 0;
        this.isUpgrading = false;
        this.loadingUI = new FirmwareUpdateDataLoadingUI();
        
        this.init();
    }
    
    init() {
        console.log('[FIRMWARE-UPGRADE-UI] Initializing firmware upgrade UI controller...');
        
        // Wait for firmware upgrade manager to be initialized
        if (this.firmwareUpgradeManager.isInitialized()) {
            this.setupUI();
        } else {
            document.addEventListener('firmwareUpgradeInitialized', () => {
                this.setupUI();
            });
        }
    }
    
    setupUI() {
        this.createFirmwareUpgradeStructure();
        this.setupEventListeners();
        this.loadCurrentFirmwareInfo();
        
        console.log('[FIRMWARE-UPGRADE-UI] Firmware upgrade UI controller initialized');
    }
    
    createFirmwareUpgradeStructure() {
        const mainContainer = document.getElementById('main-content');
        if (!mainContainer) return;
        
        // Set the firmware upgrade HTML content
        mainContainer.innerHTML = `
            <!-- Page Header -->
            <div class="mb-8">
                <div class="flex items-center justify-between">
                    <div>
                        <h1 class="text-3xl text-neutral-900">Firmware Upgrade</h1>
                        <p class="mt-2 text-neutral-600">Manage and update your device firmware with comprehensive upgrade options</p>
                    </div>
                    <div class="flex items-center space-x-3">
                        <button id="refresh-firmware-btn" class="inline-flex items-center px-4 py-2 border border-neutral-300 rounded-md shadow-sm text-sm text-neutral-700 bg-white hover:bg-neutral-50">
                            <i class="fa-solid fa-refresh mr-2"></i>Refresh
                        </button>
                    </div>
                </div>
            </div>

            <!-- Current Firmware Information -->
            <div id="current-firmware-section" class="bg-white rounded-lg border border-neutral-200 p-6 mb-6">
                <div class="flex items-center justify-between mb-6">
                    <h2 class="text-lg text-neutral-900 flex items-center">
                        <i class="fa-solid fa-info-circle mr-2 text-neutral-600"></i>
                        Current Firmware Information
                    </h2>
                    <div class="flex items-center space-x-2">
                        <span class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs bg-green-100 text-green-800">
                            <i class="fa-solid fa-circle mr-1"></i>Active
                        </span>
                        <span class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs bg-blue-100 text-blue-800">Verified</span>
                    </div>
                </div>

                <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
                    <div class="bg-neutral-50 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm text-neutral-600">Current Version</span>
                            <i class="fa-solid fa-code-branch text-neutral-400"></i>
                        </div>
                        <div id="current-version" class="text-2xl text-neutral-900">Loading...</div>
                        <div id="build-number" class="text-sm text-neutral-500 mt-1">Loading...</div>
                    </div>

                    <div class="bg-neutral-50 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm text-neutral-600">Device Model</span>
                            <i class="fa-solid fa-microchip text-neutral-400"></i>
                        </div>
                        <div id="device-model" class="text-lg text-neutral-900">Loading...</div>
                        <div id="hardware-revision" class="text-sm text-neutral-500 mt-1">Loading...</div>
                    </div>

                    <div class="bg-neutral-50 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm text-neutral-600">Build Date</span>
                            <i class="fa-solid fa-calendar text-neutral-400"></i>
                        </div>
                        <div id="build-date" class="text-lg text-neutral-900">Loading...</div>
                        <div id="build-time" class="text-sm text-neutral-500 mt-1">Loading...</div>
                    </div>

                    <div class="bg-neutral-50 rounded-lg p-4">
                        <div class="flex items-center justify-between mb-2">
                            <span class="text-sm text-neutral-600">Uptime</span>
                            <i class="fa-solid fa-clock text-neutral-400"></i>
                        </div>
                        <div id="uptime" class="text-lg text-neutral-900">Loading...</div>
                        <div id="uptime-description" class="text-sm text-neutral-500 mt-1">Since last reboot</div>
                    </div>
                </div>
            </div>

            <!-- Firmware Upgrade Options -->
            <div id="firmware-upgrade-section" class="bg-white rounded-lg border border-neutral-200 mb-6">
                <div class="p-4 border-b border-neutral-200 cursor-pointer" id="firmware-upgrade-header">
                    <div class="flex items-center justify-between">
                        <h2 class="text-lg text-neutral-900 flex items-center">
                            <i class="fa-solid fa-upload mr-2 text-neutral-600"></i>
                            Firmware Upgrade Options
                        </h2>
                        <i id="firmware-upgrade-chevron" class="fa-solid fa-chevron-down text-neutral-500 transform transition-transform duration-200"></i>
                    </div>
                </div>

                <div id="firmware-upgrade-content" class="hidden p-6">
                    <div class="space-y-8">
                        <!-- Upgrade Source Selection -->
                        <div id="upgrade-source" class="border border-neutral-200 rounded-lg p-6">
                            <h3 class="text-base text-neutral-900 mb-4">Upgrade Source</h3>
                            <div class="space-y-3">
                                <!-- Upload Firmware File Option -->
                                <div class="relative">
                                    <input id="upload-file" name="upgrade-source" type="radio" class="peer sr-only" checked />
                                    <label for="upload-file" class="flex items-center p-4 border border-neutral-200 rounded-lg cursor-pointer hover:bg-neutral-50 peer-checked:border-neutral-600 peer-checked:bg-neutral-50 transition-colors">
                                        <div class="flex items-center flex-1">
                                            <div class="w-5 h-5 border-2 border-neutral-300 rounded-full flex items-center justify-center peer-checked:border-neutral-600 peer-checked:bg-neutral-600 mr-3">
                                                <div class="w-2 h-2 bg-white rounded-full opacity-0 peer-checked:opacity-100"></div>
                                            </div>
                                            <div class="flex items-center">
                                                <i class="fa-solid fa-upload text-neutral-600 mr-3"></i>
                                                <div>
                                                    <div class="text-sm font-medium text-neutral-900">Upload Firmware File (Manual)</div>
                                                    <div class="text-sm text-neutral-500">Select and upload a firmware file from your local system</div>
                                                </div>
                                            </div>
                                        </div>
                                    </label>
                                </div>

                                <!-- Check for Online Update Option -->
                                <div class="relative">
                                    <input id="online-update" name="upgrade-source" type="radio" class="peer sr-only" />
                                    <label for="online-update" class="flex items-center p-4 border border-neutral-200 rounded-lg cursor-pointer hover:bg-neutral-50 peer-checked:border-neutral-600 peer-checked:bg-neutral-50 transition-colors">
                                        <div class="flex items-center flex-1">
                                            <div class="w-5 h-5 border-2 border-neutral-300 rounded-full flex items-center justify-center peer-checked:border-neutral-600 peer-checked:bg-neutral-600 mr-3">
                                                <div class="w-2 h-2 bg-white rounded-full opacity-0 peer-checked:opacity-100"></div>
                                            </div>
                                            <div class="flex items-center">
                                                <i class="fa-solid fa-globe text-neutral-600 mr-3"></i>
                                                <div>
                                                    <div class="text-sm font-medium text-neutral-900">Check for Online Update</div>
                                                    <div class="text-sm text-neutral-500">Search for and download the latest firmware version from the official server</div>
                                                </div>
                                            </div>
                                        </div>
                                    </label>
                                </div>

                                <!-- TFTP Server Upload Option -->
                                <div class="relative">
                                    <input id="tftp-update" name="upgrade-source" type="radio" class="peer sr-only" />
                                    <label for="tftp-update" class="flex items-center p-4 border border-neutral-200 rounded-lg cursor-pointer hover:bg-neutral-50 peer-checked:border-neutral-600 peer-checked:bg-neutral-50 transition-colors">
                                        <div class="flex items-center flex-1">
                                            <div class="w-5 h-5 border-2 border-neutral-300 rounded-full flex items-center justify-center peer-checked:border-neutral-600 peer-checked:bg-neutral-600 mr-3">
                                                <div class="w-2 h-2 bg-white rounded-full opacity-0 peer-checked:opacity-100"></div>
                                            </div>
                                            <div class="flex items-center">
                                                <i class="fa-solid fa-server text-neutral-600 mr-3"></i>
                                                <div>
                                                    <div class="text-sm font-medium text-neutral-900">TFTP Server Upload</div>
                                                    <div class="text-sm text-neutral-500">Upload firmware from a TFTP server on your local network</div>
                                                </div>
                                            </div>
                                        </div>
                                    </label>
                                </div>
                            </div>
                        </div>

                        <!-- File Upload Section -->
                        <div id="file-upload-section" class="border border-neutral-200 rounded-lg p-6">
                            <h3 class="text-base text-neutral-900 mb-4">File Upload Configuration</h3>
                            <div class="space-y-4">
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Select Firmware File</label>
                                    <div class="flex items-center justify-center w-full">
                                        <label for="firmware-file" id="firmware-drop-zone" class="flex flex-col items-center justify-center w-full h-32 border-2 border-neutral-300 border-dashed rounded-lg cursor-pointer bg-neutral-50 hover:bg-neutral-100 transition-colors">
                                            <div class="flex flex-col items-center justify-center pt-5 pb-6">
                                                <i id="upload-icon" class="fa-solid fa-cloud-upload-alt text-neutral-400 text-3xl mb-2"></i>
                                                <p class="mb-2 text-sm text-neutral-500">
                                                    <span id="upload-text">Click to upload</span> or drag and drop
                                                </p>
                                                <p class="text-xs text-neutral-500">BIN, IMG, TRX files (MAX. 32MB)</p>
                                            </div>
                                            <input id="firmware-file" type="file" class="hidden" accept=".bin,.img,.trx" />
                                        </label>
                                    </div>
                                    <div class="mt-2 text-sm text-neutral-600">
                                        <span>Selected: </span><span id="selected-file">No file selected</span>
                                    </div>
                                </div>

                                <!-- Upload Progress Display -->
                                <div id="manual-upload-progress-display" class="bg-green-50 border border-green-200 rounded-lg p-4 hidden">
                                    <h4 class="text-green-800 font-medium mb-3">Upload Progress</h4>
                                    <div class="space-y-3">
                                        <div class="flex items-center justify-between">
                                            <span class="text-sm text-green-700">Status:</span>
                                            <span id="manual-upload-status" class="text-sm font-medium text-green-800">Idle</span>
                                        </div>
                                        <div class="w-full bg-green-200 rounded-full h-2">
                                            <div id="manual-upload-progress-bar" class="bg-green-600 h-2 rounded-full transition-all duration-300" style="width: 0%"></div>
                                        </div>
                                        <div class="flex items-center justify-between text-xs text-green-600">
                                            <span id="manual-upload-progress-text">0%</span>
                                            <span id="manual-upload-size-text">0 bytes</span>
                                        </div>
                                    </div>
                                </div>

                                <div class="flex items-center space-x-4">
                                    <div class="flex items-center">
                                        <input id="verify-signature" type="checkbox" class="h-4 w-4 text-neutral-600 border-neutral-300 rounded focus:ring-neutral-500" />
                                        <label for="verify-signature" class="ml-2 text-sm text-neutral-700">Verify Digital Signature</label>
                                    </div>
                                    <button id="verify-file-btn" class="inline-flex items-center px-4 py-2 border border-neutral-300 rounded-md shadow-sm text-sm text-neutral-700 bg-white hover:bg-neutral-50">
                                        <i class="fa-solid fa-check-circle mr-2"></i>Verify File Integrity
                                    </button>
                                    <button id="upload-firmware-btn" class="inline-flex items-center px-4 py-2 bg-green-600 text-white text-sm rounded-md hover:bg-green-700 hidden">
                                        <i class="fa-solid fa-upload mr-2"></i>Upload Firmware
                                    </button>
                                </div>
                            </div>
                        </div>

                        <!-- Online Update Section -->
                        <div id="online-update-section" class="border border-neutral-200 rounded-lg p-6 hidden">
                            <h3 class="text-base text-neutral-900 mb-4">Online Update Configuration</h3>
                            <div class="space-y-4">
                                <div class="bg-blue-50 border border-blue-200 rounded-lg p-4">
                                    <div class="flex items-center">
                                        <i class="fa-solid fa-info-circle text-blue-600 mr-2"></i>
                                        <span class="text-sm text-blue-800">Checking for available firmware updates...</span>
                                    </div>
                                </div>
                                <div class="space-y-3">
                                    <div class="flex items-center justify-between">
                                        <span class="text-sm text-neutral-700">Update Server:</span>
                                        <span class="text-sm text-neutral-900">firmware.ultima-robotics.com</span>
                                    </div>
                                    <div class="flex items-center justify-between">
                                        <span class="text-sm text-neutral-700">Check Frequency:</span>
                                        <select class="text-sm border border-neutral-300 rounded px-2 py-1">
                                            <option>Daily</option>
                                            <option>Weekly</option>
                                            <option>Manual Only</option>
                                        </select>
                                    </div>
                                    <div class="flex items-center space-x-4">
                                        <button id="check-updates-btn" class="inline-flex items-center px-4 py-2 bg-blue-600 text-white text-sm rounded-md hover:bg-blue-700">
                                            <i class="fa-solid fa-refresh mr-2"></i>Check for Updates
                                        </button>
                                        <div class="flex items-center">
                                            <input id="auto-download" type="checkbox" class="h-4 w-4 text-blue-600 border-neutral-300 rounded focus:ring-blue-500" />
                                            <label for="auto-download" class="ml-2 text-sm text-neutral-700">Auto-download available updates</label>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>

                        <!-- TFTP Upload Section -->
                        <div id="tftp-upload-section" class="border border-neutral-200 rounded-lg p-6 hidden">
                            <h3 class="text-base text-neutral-900 mb-4">TFTP Server Configuration</h3>
                            <div class="space-y-4">
                                <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                                    <div>
                                        <label class="block text-sm text-neutral-700 mb-2">TFTP Server IP</label>
                                        <input id="tftp-server-ip" type="text" placeholder="192.168.1.100" class="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-500" />
                                    </div>
                                    <div>
                                        <label class="block text-sm text-neutral-700 mb-2">Port</label>
                                        <input id="tftp-port" type="number" placeholder="69" value="69" class="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-500" />
                                    </div>
                                </div>
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Firmware Filename</label>
                                    <input id="tftp-filename" type="text" placeholder="firmware.bin" class="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-500" />
                                </div>
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Username</label>
                                    <input id="tftp-username" type="text" placeholder="admin" class="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-500" />
                                </div>
                                <div>
                                    <label class="block text-sm text-neutral-700 mb-2">Password</label>
                                    <input id="tftp-password" type="password" placeholder="••••••••" class="w-full px-3 py-2 border border-neutral-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-neutral-500" />
                                </div>
                                <div class="flex items-center space-x-4">
                                    <button id="test-tftp-connection-btn" class="inline-flex items-center px-4 py-2 border border-neutral-300 rounded-md shadow-sm text-sm text-neutral-700 bg-white hover:bg-neutral-50">
                                        <i class="fa-solid fa-network-wired mr-2"></i>Test Connection
                                    </button>
                                    <div class="flex items-center">
                                        <input id="verify-tftp-signature" type="checkbox" class="h-4 w-4 text-neutral-600 border-neutral-300 rounded focus:ring-neutral-500" />
                                        <label for="verify-tftp-signature" class="ml-2 text-sm text-neutral-700">Verify Digital Signature</label>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Upgrade Actions -->
            <div id="upgrade-actions-section" class="bg-white rounded-lg border border-neutral-200 mb-6">
                <div class="p-4 border-b border-neutral-200 cursor-pointer" id="upgrade-actions-header">
                    <div class="flex items-center justify-between">
                        <h2 class="text-lg text-neutral-900 flex items-center">
                            <i class="fa-solid fa-play-circle mr-2 text-neutral-600"></i>
                            Upgrade Actions
                        </h2>
                        <i id="upgrade-actions-chevron" class="fa-solid fa-chevron-down text-neutral-500 transform transition-transform duration-200"></i>
                    </div>
                </div>

                <div id="upgrade-actions-content" class="hidden p-6">
                    <div class="space-y-6">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center space-x-4">
                                <button id="start-upgrade-btn" class="inline-flex items-center px-6 py-3 bg-neutral-800 text-white text-sm rounded-md hover:bg-neutral-700">
                                   Start Upgrade
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }
    
    setupEventListeners() {
        // Refresh button
        const refreshBtn = document.getElementById('refresh-firmware-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.refreshFirmwareInfo());
        }
        
        // Collapsible sections
        this.setupCollapsibleSection('firmware-upgrade-header', 'firmware-upgrade-content', 'firmware-upgrade-chevron');
        this.setupCollapsibleSection('upgrade-actions-header', 'upgrade-actions-content', 'upgrade-actions-chevron');
        
        // Upgrade source selection
        const uploadFileRadio = document.getElementById('upload-file');
        const onlineUpdateRadio = document.getElementById('online-update');
        const tftpUpdateRadio = document.getElementById('tftp-update');
        
        if (uploadFileRadio) uploadFileRadio.addEventListener('change', () => this.handleUpgradeSourceChange('upload-file'));
        if (onlineUpdateRadio) onlineUpdateRadio.addEventListener('change', () => this.handleUpgradeSourceChange('online-update'));
        if (tftpUpdateRadio) tftpUpdateRadio.addEventListener('change', () => this.handleUpgradeSourceChange('tftp-update'));
        
        // File upload
        this.setupFileUpload();
        
        // Action buttons
        const verifyFileBtn = document.getElementById('verify-file-btn');
        const uploadFirmwareBtn = document.getElementById('upload-firmware-btn');
        const checkUpdatesBtn = document.getElementById('check-updates-btn');
        const testTftpConnectionBtn = document.getElementById('test-tftp-connection-btn');
        const startUpgradeBtn = document.getElementById('start-upgrade-btn');
        
        if (verifyFileBtn) verifyFileBtn.addEventListener('click', () => this.verifyFileIntegrity());
        if (uploadFirmwareBtn) uploadFirmwareBtn.addEventListener('click', () => this.uploadFirmware());
        if (checkUpdatesBtn) checkUpdatesBtn.addEventListener('click', () => this.checkForUpdates());
        if (testTftpConnectionBtn) testTftpConnectionBtn.addEventListener('click', () => this.testTftpConnection());
        if (startUpgradeBtn) startUpgradeBtn.addEventListener('click', () => this.startUpgrade());
    }
    
    setupCollapsibleSection(headerId, contentId, chevronId) {
        const header = document.getElementById(headerId);
        const content = document.getElementById(contentId);
        const chevron = document.getElementById(chevronId);
        
        if (header && content && chevron) {
            header.addEventListener('click', () => {
                const isHidden = content.classList.contains('hidden');
                
                if (isHidden) {
                    content.classList.remove('hidden');
                    chevron.classList.add('rotate-180');
                } else {
                    content.classList.add('hidden');
                    chevron.classList.remove('rotate-180');
                }
            });
        }
    }
    
    setupFileUpload() {
        const fileInput = document.getElementById('firmware-file');
        const dropZone = document.getElementById('firmware-drop-zone');
        const uploadText = document.getElementById('upload-text');
        const uploadIcon = document.getElementById('upload-icon');
        
        if (!fileInput || !dropZone) return;
        
        // File input change
        fileInput.addEventListener('change', (e) => {
            this.handleFileSelect(e.target.files[0]);
        });
        
        // Drag and drop
        dropZone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropZone.classList.add('border-neutral-600', 'bg-neutral-100');
            uploadText.textContent = 'Drop file here';
            uploadIcon.classList.add('text-neutral-600');
        });
        
        dropZone.addEventListener('dragleave', (e) => {
            e.preventDefault();
            dropZone.classList.remove('border-neutral-600', 'bg-neutral-100');
            uploadText.textContent = 'Click to upload';
            uploadIcon.classList.remove('text-neutral-600');
        });
        
        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.classList.remove('border-neutral-600', 'bg-neutral-100');
            uploadText.textContent = 'Click to upload';
            uploadIcon.classList.remove('text-neutral-600');
            
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                this.handleFileSelect(files[0]);
            }
        });
    }
    
    handleFileSelect(file) {
        if (!file) return;
        
        // Validate file type
        const validTypes = ['.bin', '.img', '.trx'];
        const fileExtension = file.name.toLowerCase().substring(file.name.lastIndexOf('.'));
        
        if (!validTypes.includes(fileExtension)) {
            this.showNotification('Invalid file type. Please select a BIN, IMG, or TRX file.', 'error');
            return;
        }
        
        // Validate file size (32MB max)
        const maxSize = 32 * 1024 * 1024;
        if (file.size > maxSize) {
            this.showNotification('File size exceeds 32MB limit.', 'error');
            return;
        }
        
        this.selectedFile = file;
        document.getElementById('selected-file').textContent = file.name;
        document.getElementById('upload-firmware-btn').classList.remove('hidden');
        
        console.log('[FIRMWARE-UPGRADE-UI] File selected:', file.name, 'Size:', file.size);
    }
    
    handleUpgradeSourceChange(source) {
        this.currentUpgradeSource = source;
        
        // Hide all sections
        document.getElementById('file-upload-section').classList.add('hidden');
        document.getElementById('online-update-section').classList.add('hidden');
        document.getElementById('tftp-upload-section').classList.add('hidden');
        
        // Show selected section
        switch (source) {
            case 'upload-file':
                document.getElementById('file-upload-section').classList.remove('hidden');
                break;
            case 'online-update':
                document.getElementById('online-update-section').classList.remove('hidden');
                break;
            case 'tftp-update':
                document.getElementById('tftp-upload-section').classList.remove('hidden');
                break;
        }
        
        console.log('[FIRMWARE-UPGRADE-UI] Upgrade source changed to:', source);
    }
    
    async loadCurrentFirmwareInfo() {
        try {
            console.log('[FIRMWARE-UPGRADE-UI] Loading current firmware information...');
            
            // Always show loading state - this will persist until data is available
            this.loadingUI.showCurrentFirmwareLoading();
            
            // Get firmware information from manager
            const firmwareInfo = await this.firmwareUpgradeManager.getCurrentFirmwareInfo();
            
            // Update UI with firmware data only if valid data is actually provided
            if (firmwareInfo && (firmwareInfo.version || firmwareInfo.buildDate || firmwareInfo.buildTime || firmwareInfo.uptime)) {
                console.log('[FIRMWARE-UPGRADE-UI] Valid firmware data received, updating UI...');
                
                // Hide loading state only when we have actual data
                this.loadingUI.hideCurrentFirmwareLoading();
                
                // Update all firmware fields with the provided data using correct element IDs
                document.getElementById('current-version').textContent = firmwareInfo.version || 'Unknown';
                document.getElementById('build-number').textContent = firmwareInfo.buildNumber || 'Build #12345';
                document.getElementById('device-model').textContent = firmwareInfo.deviceModel || 'Unknown Model';
                document.getElementById('hardware-revision').textContent = firmwareInfo.hardwareRevision || 'Rev 1.0';
                document.getElementById('build-date').textContent = firmwareInfo.buildDate || 'Unknown';
                document.getElementById('build-time').textContent = firmwareInfo.buildTime || 'Unknown';
                document.getElementById('uptime').textContent = firmwareInfo.uptime || 'Unknown';
                
                console.log('[FIRMWARE-UPGRADE-UI] Current firmware info loaded:', firmwareInfo);
                
                // Stop any retry mechanism since data is now available
                this.stopDataRetry();
                
                // Show success notification
                this.showNotification('Firmware information loaded successfully', 'success');
            } else {
                // No valid data provided - keep loading effect active
                console.log('[FIRMWARE-UPGRADE-UI] No valid firmware data provided, keeping loading effect...');
                this.handleNoFirmwareData();
            }
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] Failed to load firmware info:', error);
            // Even on error, keep loading effect active and start retry mechanism
            this.handleNoFirmwareData();
        }
        // Note: Never hide loading state here - it persists until valid data arrives
    }
    
    /**
     * Handle case when no firmware data is available
     */
    handleNoFirmwareData() {
        console.log('[FIRMWARE-UPGRADE-UI] No firmware data available, keeping loading effect active...');
        
        // Ensure loading skeleton effect is continuously visible
        // The loading effect will remain visible until data becomes available
        this.loadingUI.showCurrentFirmwareLoading();
        
        // Start periodic retry if not already running
        this.startDataRetry();
        
        // Don't show error notifications - just keep loading silently
        // Users will see the loading effect, which indicates data is being fetched
    }
    
    /**
     * Start periodic retry for loading firmware data
     */
    startDataRetry() {
        // Clear any existing retry timer
        if (this.dataRetryTimer) {
            clearInterval(this.dataRetryTimer);
        }
        
        // Set up retry every 3 seconds for better responsiveness
        this.dataRetryTimer = setInterval(async () => {
            console.log('[FIRMWARE-UPGRADE-UI] Retrying to load firmware data...');
            try {
                // Ensure loading effect is always visible during retry
                this.loadingUI.showCurrentFirmwareLoading();
                
                const firmwareInfo = await this.firmwareUpgradeManager.getCurrentFirmwareInfo();
                
                // Check if we have valid data now
                if (firmwareInfo && (firmwareInfo.version || firmwareInfo.buildDate || firmwareInfo.buildTime || firmwareInfo.uptime)) {
                    console.log('[FIRMWARE-UPGRADE-UI] Firmware data is now available!');
                    
                    // Clear the retry timer
                    clearInterval(this.dataRetryTimer);
                    this.dataRetryTimer = null;
                    
                    // Hide loading state and update UI
                    this.loadingUI.hideCurrentFirmwareLoading();
                    
                    // Update all firmware fields with the provided data using correct element IDs
                    document.getElementById('current-version').textContent = firmwareInfo.version || 'Unknown';
                    document.getElementById('build-number').textContent = firmwareInfo.buildNumber || 'Build #12345';
                    document.getElementById('device-model').textContent = firmwareInfo.deviceModel || 'Unknown Model';
                    document.getElementById('hardware-revision').textContent = firmwareInfo.hardwareRevision || 'Rev 1.0';
                    document.getElementById('build-date').textContent = firmwareInfo.buildDate || 'Unknown';
                    document.getElementById('build-time').textContent = firmwareInfo.buildTime || 'Unknown';
                    document.getElementById('uptime').textContent = firmwareInfo.uptime || 'Unknown';
                    
                    // Show success notification only once when data finally arrives
                    this.showNotification('Firmware information loaded successfully', 'success');
                } else {
                    // No data yet - ensure loading effect continues
                    this.loadingUI.showCurrentFirmwareLoading();
                }
            } catch (error) {
                console.log('[FIRMWARE-UPGRADE-UI] Retry failed, keeping loading effect active...');
                // Ensure loading effect remains visible even on retry failures
                this.loadingUI.showCurrentFirmwareLoading();
            }
        }, 3000); // Retry every 3 seconds for better user experience
    }
    
    /**
     * Stop data retry timer
     */
    stopDataRetry() {
        if (this.dataRetryTimer) {
            clearInterval(this.dataRetryTimer);
            this.dataRetryTimer = null;
            console.log('[FIRMWARE-UPGRADE-UI] Data retry timer stopped');
        }
    }
    
    async refreshFirmwareInfo() {
        try {
            console.log('[FIRMWARE-UPGRADE-UI] Refreshing firmware information...');
            
            // Show loading state using loading UI
            this.loadingUI.showRefreshLoading();
            
            // Reload firmware info - this will maintain loading effect until data is available
            await this.loadCurrentFirmwareInfo();
            
            // Only show success message if data was actually loaded (loading is no longer active)
            if (!this.loadingUI.isLoading('current-firmware')) {
                this.showNotification('Firmware information refreshed successfully', 'success');
            } else {
                // Data is still loading, don't show any message - the loading effect indicates the status
                console.log('[FIRMWARE-UPGRADE-UI] Refresh initiated, loading effect active until data arrives');
            }
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] Failed to refresh firmware info:', error);
            // Don't show error notification - keep loading effect active
            console.log('[FIRMWARE-UPGRADE-UI] Refresh failed, maintaining loading effect');
        } finally {
            // Hide refresh button loading state
            this.loadingUI.hideRefreshLoading();
        }
    }
    
    async verifyFileIntegrity() {
        if (!this.selectedFile) {
            this.showNotification('Please select a firmware file first', 'warning');
            return;
        }
        
        const verifyBtn = document.getElementById('verify-file-btn');
        const originalContent = verifyBtn.innerHTML;
        
        try {
            console.log('[FIRMWARE-UPGRADE-UI] Verifying file integrity...');
            
            // Show loading state
            this.loadingUI.showFileUploadLoading();
            verifyBtn.disabled = true;
            verifyBtn.innerHTML = '<div class="firmware-loading-spinner"></div> Verifying...';
            
            const verificationResult = await this.firmwareUpgradeManager.verifyFileIntegrity(this.selectedFile);
            
            if (verificationResult.valid) {
                this.showNotification('File integrity verified successfully', 'success');
                document.getElementById('file-verification-result').innerHTML = `
                    <div class="flex items-center text-sm text-green-600">
                        <i class="fa-solid fa-check-circle mr-2"></i>
                        <span>${verificationResult.message || 'File is valid and ready for upgrade'}</span>
                    </div>
                `;
            } else {
                this.showNotification(`File verification failed: ${verificationResult.error}`, 'error');
                document.getElementById('file-verification-result').innerHTML = `
                    <div class="flex items-center text-sm text-red-600">
                        <i class="fa-solid fa-exclamation-circle mr-2"></i>
                        <span>${verificationResult.error || 'File verification failed'}</span>
                    </div>
                `;
            }
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] File verification failed:', error);
            this.showNotification('File verification failed', 'error');
        } finally {
            // Hide loading state
            this.loadingUI.hideFileUploadLoading();
            verifyBtn.disabled = false;
            verifyBtn.innerHTML = originalContent;
        }
    }
    
    async uploadFirmware() {
        if (!this.selectedFile) {
            this.showNotification('Please select a firmware file first', 'warning');
            return;
        }
        
        const uploadBtn = document.getElementById('upload-firmware-btn');
        const originalContent = uploadBtn.innerHTML;
        
        try {
            console.log('[FIRMWARE-UPGRADE-UI] Uploading firmware file...');
            
            // Show loading state
            this.loadingUI.showFileUploadLoading();
            uploadBtn.disabled = true;
            uploadBtn.innerHTML = '<div class="firmware-loading-spinner"></div> Uploading...';
            
            // Show progress display
            const progressDisplay = document.getElementById('manual-upload-progress-display');
            progressDisplay.classList.remove('hidden');
            
            await this.firmwareUpgradeManager.uploadFirmware(this.selectedFile, (progress) => {
                this.updateUploadProgress(progress);
            });
            
            this.showNotification('Firmware uploaded successfully', 'success');
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] Firmware upload failed:', error);
            this.showNotification('Firmware upload failed', 'error');
        } finally {
            // Hide loading state
            this.loadingUI.hideFileUploadLoading();
            uploadBtn.disabled = false;
            uploadBtn.innerHTML = originalContent;
        }
    }
    
    updateUploadProgress(progress) {
        const progressBar = document.getElementById('manual-upload-progress-bar');
        const progressText = document.getElementById('manual-upload-progress-text');
        const sizeText = document.getElementById('manual-upload-size-text');
        const statusText = document.getElementById('manual-upload-status');
        
        if (progressBar) progressBar.style.width = `${progress.percentage}%`;
        if (progressText) progressText.textContent = `${progress.percentage}%`;
        if (sizeText) sizeText.textContent = `${progress.loaded} / ${progress.total}`;
        if (statusText) statusText.textContent = progress.status || 'Uploading...';
    }
    
    async checkForUpdates() {
        const checkBtn = document.getElementById('check-updates-btn');
        const originalContent = checkBtn.innerHTML;
        
        // Show loading state
        checkBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Checking...';
        checkBtn.disabled = true;
        
        try {
            const updateInfo = await this.firmwareUpgradeManager.checkForOnlineUpdates();
            
            if (updateInfo.available) {
                this.showNotification(`Update available: ${updateInfo.version}`, 'success');
            } else {
                this.showNotification('No updates available', 'info');
            }
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] Update check failed:', error);
            this.showNotification('Failed to check for updates', 'error');
        } finally {
            // Restore button state
            checkBtn.innerHTML = originalContent;
            checkBtn.disabled = false;
        }
    }
    
    async testTftpConnection() {
        const serverIp = document.getElementById('tftp-server-ip').value;
        const port = document.getElementById('tftp-port').value;
        const filename = document.getElementById('tftp-filename').value;
        
        if (!serverIp || !filename) {
            this.showNotification('Please fill in TFTP server IP and filename', 'warning');
            return;
        }
        
        const testBtn = document.getElementById('test-tftp-connection-btn');
        const originalContent = testBtn.innerHTML;
        
        // Show loading state
        testBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Testing...';
        testBtn.disabled = true;
        
        try {
            const connectionResult = await this.firmwareUpgradeManager.testTftpConnection({
                serverIp,
                port: port || 69,
                filename
            });
            
            if (connectionResult.success) {
                this.showNotification('TFTP connection test successful', 'success');
            } else {
                this.showNotification(`TFTP connection failed: ${connectionResult.error}`, 'error');
            }
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] TFTP connection test failed:', error);
            this.showNotification('TFTP connection test failed', 'error');
        } finally {
            // Restore button state
            testBtn.innerHTML = originalContent;
            testBtn.disabled = false;
        }
    }
    
    async startUpgrade() {
        if (this.isUpgrading) {
            this.showNotification('Upgrade already in progress', 'warning');
            return;
        }
        
        // Show dedicated confirmation modal
        const confirmed = await this.showUpgradeConfirmationModal();
        if (!confirmed) return;
        
        const startBtn = document.getElementById('start-upgrade-btn');
        const originalContent = startBtn.innerHTML;
        
        // Show loading state
        this.isUpgrading = true;
        startBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Upgrading...';
        startBtn.disabled = true;
        
        try {
            const upgradeConfig = this.getUpgradeConfiguration();
            const upgradeResult = await this.firmwareUpgradeManager.startUpgrade(upgradeConfig);
            
            if (upgradeResult.success) {
                this.showNotification('Firmware upgrade started successfully', 'success');
                this.monitorUpgradeProgress();
            } else {
                this.showNotification(`Upgrade failed: ${upgradeResult.error}`, 'error');
            }
        } catch (error) {
            console.error('[FIRMWARE-UPGRADE-UI] Firmware upgrade failed:', error);
            this.showNotification('Firmware upgrade failed', 'error');
        } finally {
            if (!this.isUpgrading) {
                // Restore button state if upgrade failed immediately
                startBtn.innerHTML = originalContent;
                startBtn.disabled = false;
            }
        }
    }
    
    async showUpgradeConfirmationModal() {
        return new Promise((resolve) => {
            if (!window.popupManager) {
                // Fallback to simple confirm if popup manager is not available
                const confirmed = confirm('Are you sure you want to start the firmware upgrade? This will restart your device.');
                resolve(confirmed);
                return;
            }
            
            // Create custom confirmation modal
            window.popupManager.showCustom({
                title: '', // No title since it's in the custom content
                content: `
                    <div class="firmware-upgrade-modal">
                        <!-- Modal Header -->
                        <div class="flex items-center justify-between px-6 py-4 border-b border-neutral-200">
                            <h2 class="text-xl text-neutral-900">Confirm Firmware Upgrade</h2>
                            <button class="p-1 hover:bg-neutral-100 rounded-md transition-colors" onclick="window.popupManager.close()">
                                <i class="fa-solid fa-times text-neutral-500 text-lg"></i>
                            </button>
                        </div>

                        <!-- Modal Content -->
                        <div class="px-6 py-6">
                            <div class="flex flex-col items-center justify-center text-center mb-6">
                                <!-- Warning Icon -->
                                <div class="w-12 h-12 bg-orange-100 rounded-full flex items-center justify-center mb-4">
                                    <i class="fa-solid fa-exclamation-triangle text-orange-600 text-xl"></i>
                                </div>
                                
                                <!-- Warning Message -->
                                <h3 class="text-lg text-neutral-900 mb-2">
                                    Ready to Upgrade Firmware
                                </h3>
                                
                                <p class="text-sm text-neutral-600 mb-4">
                                    You are about to start the firmware upgrade process. This will restart your device and temporarily interrupt service.
                                </p>
                                
                                <!-- Upgrade Info -->
                                <div class="bg-neutral-50 rounded-lg p-4 w-full text-left mb-4">
                                    <div class="space-y-2">
                                        <div class="flex items-center text-sm">
                                            <i class="fa-solid fa-info-circle text-neutral-400 mr-2"></i>
                                            <span class="text-neutral-600">Current Version:</span>
                                            <span class="text-neutral-900 ml-auto" id="modal-current-version">Loading...</span>
                                        </div>
                                        <div class="flex items-center text-sm">
                                            <i class="fa-solid fa-rocket text-neutral-400 mr-2"></i>
                                            <span class="text-neutral-600">Upgrade Source:</span>
                                            <span class="text-neutral-900 ml-auto" id="modal-upgrade-source">Loading...</span>
                                        </div>
                                        <div class="flex items-center text-sm">
                                            <i class="fa-solid fa-clock text-neutral-400 mr-2"></i>
                                            <span class="text-neutral-600">Estimated Time:</span>
                                            <span class="text-neutral-900 ml-auto">5-10 minutes</span>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            
                            <!-- User Configuration Option -->
                            <div class="bg-blue-50 border border-blue-200 rounded-lg p-4">
                                <div class="flex items-start">
                                    <div class="flex items-center h-5">
                                        <input id="keep-config-checkbox" type="checkbox" class="h-4 w-4 text-blue-600 border-blue-300 rounded focus:ring-blue-500" checked>
                                    </div>
                                    <div class="ml-3">
                                        <label for="keep-config-checkbox" class="text-sm text-blue-900 font-medium cursor-pointer">
                                            Keep Current User Configuration
                                        </label>
                                        <p class="text-sm text-blue-700 mt-1">
                                            Preserve your network settings, user preferences, and custom configurations during the upgrade.
                                        </p>
                                    </div>
                                </div>
                            </div>
                        </div>

                        <!-- Modal Footer -->
                        <div class="flex justify-end gap-3 px-6 py-4 border-t border-neutral-200">
                            <button id="cancel-upgrade-btn" class="px-4 py-2 text-sm text-neutral-700 bg-white border border-neutral-300 rounded-md hover:bg-neutral-50 focus:outline-none focus:ring-2 focus:ring-neutral-500 transition-colors">
                                Cancel
                            </button>
                            <button id="confirm-upgrade-btn" class="px-4 py-2 text-sm text-white bg-orange-600 rounded-md hover:bg-orange-700 focus:outline-none focus:ring-2 focus:ring-orange-500 transition-colors">
                                <i class="fa-solid fa-rocket mr-2"></i>Start Upgrade
                            </button>
                        </div>
                    </div>
                `
            });
            
            // Update modal with current information
            setTimeout(() => {
                const currentVersionEl = document.getElementById('modal-current-version');
                const upgradeSourceEl = document.getElementById('modal-upgrade-source');
                
                if (currentVersionEl && this.firmwareUpgradeManager) {
                    this.firmwareUpgradeManager.getCurrentFirmwareInfo().then(info => {
                        if (currentVersionEl) currentVersionEl.textContent = info.version || 'Unknown';
                    });
                }
                
                if (upgradeSourceEl) {
                    const sourceMap = {
                        'upload-file': 'Manual Upload',
                        'online-update': 'Online Update',
                        'tftp-update': 'TFTP Server'
                    };
                    upgradeSourceEl.textContent = sourceMap[this.currentUpgradeSource] || 'Unknown';
                }
            }, 100);
            
            // Setup button handlers
            const cancelBtn = document.getElementById('cancel-upgrade-btn');
            const confirmBtn = document.getElementById('confirm-upgrade-btn');
            
            const handleCancel = () => {
                window.popupManager.close();
                resolve(false);
            };
            
            const handleConfirm = () => {
                const keepConfig = document.getElementById('keep-config-checkbox').checked;
                
                // Store the configuration preference
                this.upgradeKeepConfig = keepConfig;
                
                window.popupManager.close();
                resolve(true);
            };
            
            if (cancelBtn) cancelBtn.addEventListener('click', handleCancel);
            if (confirmBtn) confirmBtn.addEventListener('click', handleConfirm);
            
            // Also handle close button and ESC key
            const closeBtn = document.querySelector('[onclick*="popupManager.close()"]');
            if (closeBtn) {
                closeBtn.addEventListener('click', handleCancel);
            }
            
            // Handle ESC key
            const handleEscKey = (e) => {
                if (e.key === 'Escape') {
                    document.removeEventListener('keydown', handleEscKey);
                    handleCancel();
                }
            };
            document.addEventListener('keydown', handleEscKey);
        });
    }
    
    getUpgradeConfiguration() {
        const config = {
            source: this.currentUpgradeSource,
            verifySignature: document.getElementById('verify-signature').checked,
            keepConfig: this.upgradeKeepConfig !== undefined ? this.upgradeKeepConfig : true // Default to true
        };
        
        switch (this.currentUpgradeSource) {
            case 'upload-file':
                config.file = this.selectedFile;
                break;
            case 'online-update':
                config.autoDownload = document.getElementById('auto-download').checked;
                break;
            case 'tftp-update':
                config.tftpConfig = {
                    serverIp: document.getElementById('tftp-server-ip').value,
                    port: document.getElementById('tftp-port').value || 69,
                    filename: document.getElementById('tftp-filename').value,
                    username: document.getElementById('tftp-username').value,
                    password: document.getElementById('tftp-password').value,
                    verifySignature: document.getElementById('verify-tftp-signature').checked
                };
                break;
        }
        
        return config;
    }
    
    async monitorUpgradeProgress() {
        // This would typically involve HTTP polling to monitor progress
        // For now, we'll show a simple progress simulation
        console.log('[FIRMWARE-UPGRADE-UI] Monitoring upgrade progress...');
        
        // Show upgrade progress modal or notification
        this.showNotification('Firmware upgrade in progress. Do not power off the device.', 'info');
        return this.selectedFile;
    }
    
    isUpgradeInProgress() {
        return this.isUpgrading;
    }
    
    getNotificationIcon(type) {
        const icons = {
            success: '<i class="fa-solid fa-check-circle text-green-500"></i>',
            error: '<i class="fa-solid fa-exclamation-circle text-red-500"></i>',
            warning: '<i class="fa-solid fa-exclamation-triangle text-yellow-500"></i>',
            info: '<i class="fa-solid fa-info-circle text-blue-500"></i>'
        };
        return icons[type] || icons.info;
    }
    
    showNotification(message, type = 'info') {
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `fixed top-4 right-4 z-50 p-4 rounded-lg shadow-lg max-w-sm transform transition-all duration-300 translate-x-full`;
        
        // Set color based on type
        const colorClasses = {
            success: 'bg-green-50 border border-green-200 text-green-800',
            error: 'bg-red-50 border border-red-200 text-red-800',
            warning: 'bg-yellow-50 border border-yellow-200 text-yellow-800',
            info: 'bg-blue-50 border border-blue-200 text-blue-800'
        };
        
        notification.className += ' ' + (colorClasses[type] || colorClasses.info);
        
        notification.innerHTML = `
            <div class="flex items-start">
                <div class="flex-shrink-0">
                    ${this.getNotificationIcon(type)}
                </div>
                <div class="ml-3">
                    <p class="text-sm font-medium">${message}</p>
                </div>
                <div class="ml-auto pl-3">
                    <button onclick="this.parentElement.parentElement.remove()" class="inline-flex text-gray-400 hover:text-gray-500 focus:outline-none">
                        <i class="fa-solid fa-times"></i>
                    </button>
                </div>
            </div>
        `;
        
        document.body.appendChild(notification);
        
        // Slide in
        setTimeout(() => {
            notification.classList.remove('translate-x-full');
            notification.classList.add('translate-x-0');
        }, 100);
        
        // Auto remove after 5 seconds
        setTimeout(() => {
            if (notification.parentElement) {
                notification.classList.remove('translate-x-0');
                notification.classList.add('translate-x-full');
                setTimeout(() => notification.remove(), 300);
            }
        }, 5000);
    }
    
    /**
     * Cleanup method to stop timers and clear loading states
     */
    cleanup() {
        console.log('[FIRMWARE-UPGRADE-UI] Cleaning up firmware upgrade UI...');
        
        // Stop data retry timer
        this.stopDataRetry();
        
        // Clear all loading states
        if (this.loadingUI) {
            this.loadingUI.clearAllLoadingStates();
        }
        
        // Reset state
        this.isUpgrading = false;
        this.selectedFile = null;
        this.uploadProgress = 0;
    }
}

export { FirmwareUpgradeUI };

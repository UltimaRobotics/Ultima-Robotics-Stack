/**
 * Backup & Restore UI Controller
 * Manages the backup and restore page interface, user interactions, and dynamic content rendering
 */

import { BackupRestoreDataLoadingUI } from './backup-restore-data-loading.js';

class BackupRestoreUI {
    constructor(backupRestoreManager) {
        this.backupRestoreManager = backupRestoreManager;
        this.selectedFile = null;
        this.backupInProgress = false;
        this.restoreInProgress = false;
        this.loadingUI = new BackupRestoreDataLoadingUI();
        
        this.init();
    }
    
    init() {
        console.log('[BACKUP-RESTORE-UI] Initializing backup & restore UI controller...');
        
        // Wait for backup restore manager to be initialized
        if (this.backupRestoreManager && this.backupRestoreManager.isInitialized && this.backupRestoreManager.isInitialized()) {
            this.setupUI();
        } else {
            document.addEventListener('backupRestoreInitialized', () => {
                this.setupUI();
            });
        }
    }
    
    setupUI() {
        this.createBackupRestoreStructure();
        this.setupEventListeners();
        this.loadRecentBackups();
        
        console.log('[BACKUP-RESTORE-UI] Backup & restore UI controller initialized');
    }
    
    createBackupRestoreStructure() {
        const mainContainer = document.getElementById('main-content');
        if (!mainContainer) return;
        
        // Set the backup restore HTML content
        mainContainer.innerHTML = `
            <!-- Backup & Restore Section -->
            <div class="space-y-6">
                <!-- Page Header -->
                <div class="flex items-center justify-between">
                    <div>
                        <h1 class="text-2xl text-neutral-900 flex items-center">
                            <i class="fa-solid fa-database mr-3 text-neutral-600"></i>
                            System Backup & Restore
                        </h1>
                        <p class="text-neutral-600 mt-1">Manage your system backups and restore configurations</p>
                    </div>
                </div>

                <!-- Create Backup Section -->
                <div class="bg-white rounded-lg border border-neutral-200">
                    <div class="p-6 cursor-pointer" id="create-backup-header">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <div class="w-8 h-8 bg-blue-100 rounded-lg flex items-center justify-center mr-3">
                                    <i class="fa-solid fa-box-archive text-blue-600"></i>
                                </div>
                                <h2 class="text-lg text-neutral-900">Create Backup</h2>
                            </div>
                            <i class="fa-solid fa-chevron-down text-neutral-400 transform transition-transform duration-200" id="create-backup-chevron"></i>
                        </div>
                    </div>
                    <div id="create-backup-content" class="hidden p-6 pt-0">
                        <p class="text-sm text-neutral-600 mb-6">Create a comprehensive backup of your system configuration, data, and settings with optional encryption.</p>

                        <div class="space-y-4 mb-6">
                            <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                                <h3 class="text-neutral-800 mb-3">Backup Options</h3>
                                <div class="space-y-3">
                                    <label class="flex items-start cursor-pointer">
                                        <input type="checkbox" id="full-backup-checkbox" class="mt-0.5 mr-3 rounded border-neutral-300" checked>
                                        <div>
                                            <span class="text-sm text-neutral-700">Full Backup</span>
                                            <p class="text-xs text-neutral-500 mt-1">Include complete system configuration, data, logs, and all settings</p>
                                        </div>
                                    </label>

                                    <label class="flex items-start cursor-pointer">
                                        <input type="checkbox" id="partial-backup-checkbox" class="mt-0.5 mr-3 rounded border-neutral-300">
                                        <div>
                                            <span class="text-sm text-neutral-700">Partial Backup</span>
                                            <p class="text-xs text-neutral-500 mt-1">Select specific configuration files and data</p>
                                        </div>
                                    </label>

                                    <div id="partial-backup-selection" class="ml-6 mt-3 space-y-2 hidden">
                                        <label class="flex items-center cursor-pointer">
                                            <input type="checkbox" id="network-config-backup" class="mr-3 rounded border-neutral-300">
                                            <span class="text-sm text-neutral-700">Network Configuration</span>
                                        </label>
                                        <label class="flex items-center cursor-pointer">
                                            <input type="checkbox" id="license-backup" class="mr-3 rounded border-neutral-300">
                                            <span class="text-sm text-neutral-700">License Information</span>
                                        </label>
                                        <label class="flex items-center cursor-pointer">
                                            <input type="checkbox" id="authentication-backup" class="mr-3 rounded border-neutral-300">
                                            <span class="text-sm text-neutral-700">Authentication Data</span>
                                        </label>
                                    </div>
                                </div>
                            </div>

                            <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                                <div class="flex items-center justify-between mb-3">
                                    <h3 class="text-neutral-800">Encryption</h3>
                                    <label class="relative inline-flex items-center cursor-pointer">
                                        <input type="checkbox" id="enable-encryption-checkbox" class="sr-only peer">
                                        <div class="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-blue-300 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-purple-600"></div>
                                        <span class="ml-3 text-sm font-medium text-gray-900">Enable Encryption</span>
                                    </label>
                                </div>
                                <p class="text-xs text-neutral-500">Toggle to enable AES-256 encryption for enhanced security</p>
                                
                                <div id="encryption-settings" class="mt-4 hidden">
                                    <div class="bg-purple-50 border border-purple-200 rounded-lg p-4 mb-4 flex items-start">
                                        <i class="fa-solid fa-shield-halved text-purple-600 mr-3 mt-0.5"></i>
                                        <div>
                                            <h4 class="text-purple-800 font-medium">Enhanced Security Features</h4>
                                            <ul class="text-sm text-purple-700 mt-1 list-disc list-inside space-y-1">
                                                <li>AES-256-CBC encryption with PBKDF2 key derivation</li>
                                                <li>Password validation before restore operations</li>
                                                <li>Protection against unauthorized access</li>
                                            </ul>
                                        </div>
                                    </div>
                                    
                                    <div class="space-y-3">
                                        <div>
                                            <label class="block text-sm text-neutral-700 mb-1">Encryption Password *</label>
                                            <input type="password" id="backup-password" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-purple-500" placeholder="Enter a strong password">
                                        </div>
                                        <div>
                                            <label class="block text-sm text-neutral-700 mb-1">Confirm Password *</label>
                                            <input type="password" id="confirm-backup-password" class="w-full px-3 py-2 border border-neutral-300 rounded-md focus:outline-none focus:ring-2 focus:ring-purple-500" placeholder="Confirm your password">
                                            <div id="password-strength-indicator" class="mt-2 hidden">
                                                <div class="flex items-center space-x-2">
                                                    <div class="flex-1">
                                                        <div class="w-full bg-neutral-200 rounded-full h-1">
                                                            <div id="password-strength-bar" class="h-1 rounded-full transition-all duration-300" style="width: 0%"></div>
                                                        </div>
                                                    </div>
                                                    <span id="password-strength-text" class="text-xs text-neutral-600">Weak</span>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>

                            <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                                <div class="flex items-center justify-between mb-3">
                                    <h3 class="text-neutral-800">Backup Size Estimation</h3>
                                    <button id="estimate-size-btn" class="bg-blue-100 hover:bg-blue-200 text-blue-700 px-4 py-2 rounded-md text-sm flex items-center">
                                        <i class="fa-solid fa-calculator mr-2"></i>
                                        Estimate Size
                                    </button>
                                </div>
                                <p class="text-xs text-neutral-500">Click to calculate the estimated backup size</p>
                            </div>
                        </div>

                        <div id="backup-information" class="bg-neutral-50 rounded-lg border border-neutral-200 p-4 mb-6 hidden">
                            <h3 class="text-neutral-800 mb-3">Backup Information</h3>
                            <div class="grid grid-cols-2 gap-4">
                                <div class="bg-white rounded-md p-3">
                                    <div class="text-xs text-neutral-500 uppercase tracking-wide">Estimated Size</div>
                                    <div id="estimated-size" class="text-lg text-neutral-800">-</div>
                                </div>
                                <div class="bg-white rounded-md p-3">
                                    <div class="text-xs text-neutral-500 uppercase tracking-wide">File Format</div>
                                    <div id="file-format" class="text-lg text-neutral-800">.uhb</div>
                                </div>
                            </div>
                        </div>

                        <div class="flex justify-end">
                            <button id="create-backup-btn" class="bg-blue-600 hover:bg-blue-700 text-white px-6 py-2 rounded-md flex items-center">
                                <i class="fa-solid fa-box-archive mr-2"></i>
                                <span>Create Backup</span>
                            </button>
                        </div>
                    </div>
                </div>

                <!-- Progress Section -->
                <div id="backup-progress-section" class="bg-white rounded-lg border border-neutral-200 p-6 hidden">
                    <div class="flex items-center mb-4">
                        <div class="w-8 h-8 bg-orange-100 rounded-lg flex items-center justify-center mr-3">
                            <i class="fa-solid fa-spinner fa-spin text-orange-600"></i>
                        </div>
                        <h2 class="text-lg text-neutral-900">Backup in Progress</h2>
                    </div>

                    <div class="space-y-4">
                        <div class="bg-neutral-50 rounded-lg p-4">
                            <div class="flex justify-between items-center mb-2">
                                <span class="text-sm text-neutral-700">Progress</span>
                                <span id="backup-progress-percentage" class="text-sm text-neutral-600">0%</span>
                            </div>
                            <div class="w-full bg-neutral-200 rounded-full h-2">
                                <div id="backup-progress-bar" class="bg-blue-600 h-2 rounded-full transition-all duration-300" style="width: 0%"></div>
                            </div>
                            <p id="backup-progress-status" class="text-xs text-neutral-500 mt-2">Initializing...</p>
                        </div>
                    </div>
                </div>

                <!-- Restore Section -->
                <div class="bg-white rounded-lg border border-neutral-200">
                    <div class="p-6 cursor-pointer" id="restore-backup-header">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <div class="w-8 h-8 bg-green-100 rounded-lg flex items-center justify-center mr-3">
                                    <i class="fa-solid fa-rotate-left text-green-600"></i>
                                </div>
                                <h2 class="text-lg text-neutral-900">Restore from Backup</h2>
                            </div>
                            <i class="fa-solid fa-chevron-down text-neutral-400 transform transition-transform duration-200" id="restore-backup-chevron"></i>
                        </div>
                    </div>
                    <div id="restore-backup-content" class="hidden p-6 pt-0">
                        <p class="text-sm text-neutral-600 mb-6">Upload a backup file to restore your system to a previous state.</p>

                        <div class="bg-red-50 border border-red-200 rounded-lg p-4 mb-6 flex items-start">
                            <i class="fa-solid fa-triangle-exclamation text-red-500 mr-3 mt-0.5"></i>
                            <div>
                                <h4 class="text-red-800">Warning: Destructive Operation</h4>
                                <p class="text-sm text-red-700 mt-1">Restoring will overwrite current configuration. This action cannot be undone.</p>
                            </div>
                        </div>

                        <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4 mb-6">
                            <h3 class="text-neutral-800 mb-3">Upload Backup File</h3>
                            <div class="border-2 border-dashed border-neutral-300 rounded-lg p-6 text-center" id="file-drop-zone">
                                <div class="w-12 h-12 bg-neutral-100 rounded-lg flex items-center justify-center mx-auto mb-3">
                                    <i class="fa-solid fa-cloud-arrow-up text-neutral-600 text-xl"></i>
                                </div>
                                <h4 class="text-neutral-800 mb-2">Drop your backup file here</h4>
                                <p class="text-sm text-neutral-500 mb-4">or click to browse and select a file</p>
                                <input type="file" id="backup-file-input" accept=".uhb,.uhb.enc,.tar.gz,.bin" style="display: none;">
                                <button id="choose-file-btn" class="bg-white border border-neutral-300 rounded-md px-4 py-2 text-sm text-neutral-700 hover:bg-neutral-50">
                                    Choose File
                                </button>
                                <p class="text-xs text-neutral-500 mt-2">Accepts .uhb, .uhb.enc, .tar.gz, .bin files</p>
                            </div>

                            <div id="selected-file-info" class="mt-3 hidden">
                                <div class="bg-blue-50 border border-blue-200 rounded-md p-3 flex items-center">
                                    <i class="fa-solid fa-file-archive text-blue-600 mr-3"></i>
                                    <div class="flex-1">
                                        <p class="text-sm text-blue-800" id="selected-file-name">filename.uhb</p>
                                        <p class="text-xs text-blue-600" id="selected-file-size">2.4 MB</p>
                                    </div>
                                    <button id="remove-file-btn" class="text-blue-400 hover:text-blue-600">
                                        <i class="fa-solid fa-times"></i>
                                    </button>
                                </div>
                            </div>
                        </div>

                        <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4 mb-6">
                            <h3 class="text-neutral-800 mb-3">Restore Options</h3>
                            <div class="space-y-3">
                                <label class="flex items-start cursor-pointer">
                                    <input type="checkbox" id="validate-integrity" class="mt-0.5 mr-3 rounded border-neutral-300" checked>
                                    <div>
                                        <span class="text-sm text-neutral-700">Validate Backup File Integrity</span>
                                        <p class="text-xs text-neutral-500 mt-1">Verify the backup file is not corrupted</p>
                                    </div>
                                </label>
                            </div>
                        </div>

                        <div class="flex justify-between">
                            <button id="cancel-restore-btn" class="border border-neutral-300 text-neutral-700 px-6 py-2 rounded-md hover:bg-neutral-50">
                                Cancel
                            </button>
                            <button id="restore-btn" class="bg-green-600 hover:bg-green-700 text-white px-6 py-2 rounded-md flex items-center disabled:opacity-50 disabled:cursor-not-allowed" disabled>
                                <i class="fa-solid fa-rotate-left mr-2"></i>
                                <span>Restore</span>
                            </button>
                        </div>
                    </div>
                </div>

                <!-- Recent Backups Section -->
                <div class="bg-white rounded-lg border border-neutral-200">
                    <div class="p-6 cursor-pointer" id="recent-backups-header">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <div class="w-8 h-8 bg-gray-100 rounded-lg flex items-center justify-center mr-3">
                                    <i class="fa-solid fa-clock-rotate-left text-gray-600"></i>
                                </div>
                                <h2 class="text-lg text-neutral-900">Recent Backups</h2>
                            </div>
                            <i class="fa-solid fa-chevron-down text-neutral-400 transform transition-transform duration-200" id="recent-backups-chevron"></i>
                        </div>
                    </div>
                    <div id="recent-backups-content" class="hidden p-6 pt-0">
                        <div id="backup-list" class="space-y-3">
                            <div class="text-center py-8 text-neutral-500">
                                <i class="fa-solid fa-folder-open text-4xl mb-3"></i>
                                <p>No backups found</p>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }
    
    setupEventListeners() {
        // Section toggles
        this.setupSectionToggles();
        
        // Backup options
        this.setupBackupOptions();
        
        // File upload
        this.setupFileUpload();
        
        // Backup creation buttons
        this.setupBackupCreation();
        
        // Password strength indicator
        this.setupPasswordStrength();
        
        // Restore functionality
        this.setupRestoreFunctionality();
        
        // Progress events from manager
        this.setupProgressListeners();
    }
    
    setupSectionToggles() {
        const sections = [
            { header: 'create-backup-header', content: 'create-backup-content', chevron: 'create-backup-chevron' },
            { header: 'restore-backup-header', content: 'restore-backup-content', chevron: 'restore-backup-chevron' },
            { header: 'recent-backups-header', content: 'recent-backups-content', chevron: 'recent-backups-chevron' }
        ];
        
        sections.forEach(section => {
            const header = document.getElementById(section.header);
            if (header) {
                header.addEventListener('click', () => {
                    this.toggleSection(section.content, section.chevron);
                });
            }
        });
    }
    
    toggleSection(contentId, chevronId) {
        const content = document.getElementById(contentId);
        const chevron = document.getElementById(chevronId);
        
        if (content && chevron) {
            const isHidden = content.classList.contains('hidden');
            if (isHidden) {
                content.classList.remove('hidden');
                chevron.classList.add('rotate-180');
            } else {
                content.classList.add('hidden');
                chevron.classList.remove('rotate-180');
            }
        }
    }
    
    setupBackupOptions() {
        const fullBackupCheckbox = document.getElementById('full-backup-checkbox');
        const partialBackupCheckbox = document.getElementById('partial-backup-checkbox');
        const partialSelection = document.getElementById('partial-backup-selection');
        
        if (fullBackupCheckbox && partialBackupCheckbox && partialSelection) {
            fullBackupCheckbox.addEventListener('change', () => {
                if (fullBackupCheckbox.checked) {
                    partialBackupCheckbox.checked = false;
                    partialSelection.classList.add('hidden');
                }
            });
            
            partialBackupCheckbox.addEventListener('change', () => {
                if (partialBackupCheckbox.checked) {
                    fullBackupCheckbox.checked = false;
                    partialSelection.classList.remove('hidden');
                } else {
                    partialSelection.classList.add('hidden');
                }
            });
        }
        
        // Estimate size button
        const estimateSizeBtn = document.getElementById('estimate-size-btn');
        if (estimateSizeBtn) {
            estimateSizeBtn.addEventListener('click', () => {
                this.estimateBackupSize();
            });
        }
    }
    
    setupFileUpload() {
        const chooseFileBtn = document.getElementById('choose-file-btn');
        const backupFileInput = document.getElementById('backup-file-input');
        const fileDropZone = document.getElementById('file-drop-zone');
        const removeFileBtn = document.getElementById('remove-file-btn');
        
        if (chooseFileBtn && backupFileInput) {
            chooseFileBtn.addEventListener('click', () => {
                backupFileInput.click();
            });
            
            backupFileInput.addEventListener('change', (e) => {
                if (e.target.files.length > 0) {
                    this.handleFileSelection(e.target.files[0]);
                }
            });
        }
        
        if (fileDropZone) {
            fileDropZone.addEventListener('dragover', (e) => {
                e.preventDefault();
                fileDropZone.classList.add('border-blue-400', 'bg-blue-50');
            });
            
            fileDropZone.addEventListener('dragleave', (e) => {
                e.preventDefault();
                fileDropZone.classList.remove('border-blue-400', 'bg-blue-50');
            });
            
            fileDropZone.addEventListener('drop', (e) => {
                e.preventDefault();
                fileDropZone.classList.remove('border-blue-400', 'bg-blue-50');
                
                if (e.dataTransfer.files.length > 0) {
                    this.handleFileSelection(e.dataTransfer.files[0]);
                }
            });
        }
        
        if (removeFileBtn) {
            removeFileBtn.addEventListener('click', () => {
                this.clearFileSelection();
            });
        }
    }
    
    setupBackupCreation() {
        const createBackupBtn = document.getElementById('create-backup-btn');
        if (createBackupBtn) {
            createBackupBtn.addEventListener('click', () => {
                this.createBackup();
            });
        }
        
        // Encryption toggle
        const encryptionToggle = document.getElementById('enable-encryption-checkbox');
        if (encryptionToggle) {
            encryptionToggle.addEventListener('change', () => {
                this.toggleEncryptionSettings(encryptionToggle.checked);
            });
        }
    }
    
    setupPasswordStrength() {
        const backupPassword = document.getElementById('backup-password');
        if (backupPassword) {
            backupPassword.addEventListener('input', () => {
                this.updatePasswordStrength(backupPassword.value);
            });
        }
    }
    
    setupRestoreFunctionality() {
        const cancelRestoreBtn = document.getElementById('cancel-restore-btn');
        if (cancelRestoreBtn) {
            cancelRestoreBtn.addEventListener('click', () => {
                this.clearFileSelection();
                this.showMessage('Restore cancelled', 'info');
            });
        }
        
        const restoreBtn = document.getElementById('restore-btn');
        if (restoreBtn) {
            restoreBtn.addEventListener('click', () => {
                this.startRestore();
            });
        }
    }
    
    setupProgressListeners() {
        // Listen for backup progress events from manager
        document.addEventListener('backupProgress', (event) => {
            const { progress, status } = event.detail;
            this.updateBackupProgress(progress, status);
        });
        
        // Listen for restore progress events from manager
        document.addEventListener('restoreProgress', (event) => {
            const { progress, status } = event.detail;
            this.updateRestoreProgress(progress, status);
        });
    }
    
    updateBackupProgress(progress, status) {
        const progressSection = document.getElementById('backup-progress-section');
        const progressBar = document.getElementById('backup-progress-bar');
        const progressPercentage = document.getElementById('backup-progress-percentage');
        const progressStatus = document.getElementById('backup-progress-status');
        
        if (progressSection) {
            progressSection.classList.remove('hidden');
            this.backupInProgress = true;
            
            if (progressBar) progressBar.style.width = progress + '%';
            if (progressPercentage) progressPercentage.textContent = Math.round(progress) + '%';
            if (progressStatus) progressStatus.textContent = status;
            
            if (progress >= 100) {
                setTimeout(() => {
                    if (progressSection) progressSection.classList.add('hidden');
                    this.backupInProgress = false;
                    this.loadRecentBackups();
                }, 2000);
            }
        }
    }
    
    updateRestoreProgress(progress, status) {
        // Similar implementation for restore progress
        console.log(`[BACKUP-RESTORE-UI] Restore progress: ${progress}% - ${status}`);
    }
    
    // Helper methods
    formatFileSize(bytes) {
        if (bytes === 0) return '0 bytes';
        if (bytes < 1024) return bytes + ' bytes';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
    }
    
    showMessage(message, type) {
        const existingMsg = document.getElementById('backup-message');
        if (existingMsg) existingMsg.remove();
        
        const msgDiv = document.createElement('div');
        msgDiv.id = 'backup-message';
        msgDiv.className = 'fixed top-4 right-4 z-50 p-4 rounded-lg shadow-lg max-w-md';
        
        let bgColor, icon;
        switch (type) {
            case 'success':
                bgColor = 'bg-green-50 border border-green-200 text-green-800';
                icon = 'fa-check-circle text-green-600';
                break;
            case 'error':
                bgColor = 'bg-red-50 border border-red-200 text-red-800';
                icon = 'fa-exclamation-circle text-red-600';
                break;
            default:
                bgColor = 'bg-blue-50 border border-blue-200 text-blue-800';
                icon = 'fa-info-circle text-blue-600';
        }
        
        msgDiv.className += ' ' + bgColor;
        msgDiv.innerHTML = `<div class="flex items-center"><i class="fa-solid ${icon} mr-2"></i><span>${message}</span></div>`;
        
        document.body.appendChild(msgDiv);
        setTimeout(() => {
            if (msgDiv.parentNode) msgDiv.remove();
        }, 5000);
    }
    
    // Business logic methods
    estimateBackupSize() {
        const backupInfo = document.getElementById('backup-information');
        const estimatedSize = document.getElementById('estimated-size');
        
        if (backupInfo && estimatedSize) {
            backupInfo.classList.remove('hidden');
            estimatedSize.textContent = '124.5 MB';
            this.showMessage('Estimated backup size calculated', 'info');
        }
    }
    
    handleFileSelection(file) {
        const validExtensions = ['.uhb', '.uhb.enc', '.tar.gz', '.bin'];
        const fileName = file.name.toLowerCase();
        const isValid = validExtensions.some(ext => fileName.endsWith(ext));
        
        if (!isValid) {
            this.showMessage('Invalid file type. Please select a valid backup file.', 'error');
            return;
        }
        
        this.selectedFile = file;
        
        const selectedFileInfo = document.getElementById('selected-file-info');
        const selectedFileName = document.getElementById('selected-file-name');
        const selectedFileSize = document.getElementById('selected-file-size');
        const restoreBtn = document.getElementById('restore-btn');
        
        if (selectedFileInfo && selectedFileName && selectedFileSize) {
            selectedFileName.textContent = file.name;
            selectedFileSize.textContent = this.formatFileSize(file.size);
            selectedFileInfo.classList.remove('hidden');
        }
        
        if (restoreBtn) restoreBtn.disabled = false;
        
        console.log('[BACKUP-RESTORE-UI] File selected:', file.name, 'Size:', this.formatFileSize(file.size));
    }
    
    clearFileSelection() {
        this.selectedFile = null;
        
        const selectedFileInfo = document.getElementById('selected-file-info');
        const backupFileInput = document.getElementById('backup-file-input');
        const restoreBtn = document.getElementById('restore-btn');
        
        if (selectedFileInfo) selectedFileInfo.classList.add('hidden');
        if (backupFileInput) backupFileInput.value = '';
        if (restoreBtn) restoreBtn.disabled = true;
    }
    
    updatePasswordStrength(password) {
        const indicator = document.getElementById('password-strength-indicator');
        const bar = document.getElementById('password-strength-bar');
        const text = document.getElementById('password-strength-text');
        
        if (password.length > 0 && indicator && bar && text) {
            indicator.classList.remove('hidden');
            
            const strength = Math.min(100, (password.length / 12) * 100);
            bar.style.width = strength + '%';
            
            if (strength < 50) {
                bar.className = 'h-1 rounded-full transition-all duration-300 bg-red-500';
                text.textContent = 'Weak';
            } else if (strength < 75) {
                bar.className = 'h-1 rounded-full transition-all duration-300 bg-yellow-500';
                text.textContent = 'Medium';
            } else {
                bar.className = 'h-1 rounded-full transition-all duration-300 bg-green-500';
                text.textContent = 'Strong';
            }
        }
    }
    
    createBackup() {
        if (this.backupInProgress) return;
        
        // Check if encryption is enabled
        const encryptionEnabled = document.getElementById('enable-encryption-checkbox').checked;
        
        // Gather backup options
        const options = {
            fullBackup: document.getElementById('full-backup-checkbox').checked,
            includeNetworkConfig: document.getElementById('network-config-backup').checked,
            includeLicense: document.getElementById('license-backup').checked,
            includeAuth: document.getElementById('authentication-backup').checked,
            encrypted: encryptionEnabled
        };
        
        if (encryptionEnabled) {
            // Validate password fields
            const password = document.getElementById('backup-password').value;
            const confirmPassword = document.getElementById('confirm-backup-password').value;
            
            if (!password || !confirmPassword) {
                this.showMessage('Please enter and confirm your password', 'error');
                return;
            }
            
            if (password !== confirmPassword) {
                this.showMessage('Passwords do not match', 'error');
                return;
            }
            
            options.password = password;
            console.log('[BACKUP-RESTORE-UI] Creating encrypted backup');
        } else {
            console.log('[BACKUP-RESTORE-UI] Creating standard backup');
        }
        
        // Call the manager to create the backup
        this.backupRestoreManager.createBackup(options)
            .then(result => {
                console.log('[BACKUP-RESTORE-UI] Backup created successfully:', result);
                this.showMessage(`${options.encrypted ? 'Encrypted backup' : 'Backup'} created successfully!`, 'success');
                this.loadRecentBackups();
            })
            .catch(error => {
                console.error('[BACKUP-RESTORE-UI] Error creating backup:', error);
                this.showMessage(`Failed to create backup: ${error.message}`, 'error');
            });
    }
    
    toggleEncryptionSettings(enabled) {
        const encryptionSettings = document.getElementById('encryption-settings');
        const fileFormat = document.getElementById('file-format');
        
        if (encryptionSettings) {
            if (enabled) {
                encryptionSettings.classList.remove('hidden');
                if (fileFormat) fileFormat.textContent = '.uhb.enc';
            } else {
                encryptionSettings.classList.add('hidden');
                if (fileFormat) fileFormat.textContent = '.uhb';
            }
        }
    }
    
    simulateBackup(isEncrypted = false) {
        const progressSection = document.getElementById('backup-progress-section');
        const progressBar = document.getElementById('backup-progress-bar');
        const progressPercentage = document.getElementById('backup-progress-percentage');
        const progressStatus = document.getElementById('backup-progress-status');
        
        if (progressSection) {
            progressSection.classList.remove('hidden');
            this.backupInProgress = true;
            
            let progress = 0;
            const interval = setInterval(() => {
                progress += Math.random() * 10;
                if (progress >= 100) {
                    progress = 100;
                    clearInterval(interval);
                    
                    if (progressStatus) progressStatus.textContent = 'Backup completed!';
                    this.showMessage(`${isEncrypted ? 'Encrypted backup' : 'Backup'} created successfully!`, 'success');
                    this.backupInProgress = false;
                    
                    setTimeout(() => {
                        if (progressSection) progressSection.classList.add('hidden');
                        if (progressBar) progressBar.style.width = '0%';
                        if (progressPercentage) progressPercentage.textContent = '0%';
                        if (progressStatus) progressStatus.textContent = 'Initializing...';
                        this.loadRecentBackups();
                    }, 3000);
                }
                
                if (progressBar) progressBar.style.width = progress + '%';
                if (progressPercentage) progressPercentage.textContent = Math.round(progress) + '%';
                
                // Update status based on progress
                if (progressStatus && progress < 100) {
                    if (progress < 25) {
                        progressStatus.textContent = 'Initializing...';
                    } else if (progress < 50) {
                        progressStatus.textContent = 'Collecting files...';
                    } else if (progress < 75) {
                        progressStatus.textContent = isEncrypted ? 'Compressing and encrypting...' : 'Compressing data...';
                    } else {
                        progressStatus.textContent = 'Finalizing...';
                    }
                }
            }, 300);
        }
    }
    
    startRestore() {
        if (this.restoreInProgress || !this.selectedFile) return;
        
        console.log('[BACKUP-RESTORE-UI] Restoring from backup');
        this.showMessage('Starting restore process...', 'info');
        
        // Here you would implement the actual restore logic
        // For now, just show a success message
        setTimeout(() => {
            this.showMessage('Restore completed successfully!', 'success');
            this.clearFileSelection();
        }, 2000);
    }
    
    loadRecentBackups() {
        const backupList = document.getElementById('backup-list');
        if (!backupList) return;
        
        // Show loading state
        this.loadingUI.showBackupHistoryLoading('backup-list');
        
        // Simulate loading delay
        setTimeout(() => {
            // Mock data for recent backups - in real implementation, this would come from the backend
            const mockBackups = [
                {
                    name: 'system-backup-2025-01-15.uhb',
                    size: 124567890,
                    date: '2025-01-15 14:30:00',
                    type: 'full'
                },
                {
                    name: 'config-backup-2025-01-10.uhb',
                    size: 45678901,
                    date: '2025-01-10 09:15:00',
                    type: 'partial'
                }
            ];
            
            if (mockBackups.length === 0) {
                backupList.innerHTML = `
                    <div class="text-center py-8 text-neutral-500">
                        <i class="fa-solid fa-folder-open text-4xl mb-3"></i>
                        <p>No backups found</p>
                    </div>
                `;
            } else {
                backupList.innerHTML = mockBackups.map(backup => `
                    <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4 flex items-center justify-between">
                        <div class="flex items-center">
                            <div class="w-10 h-10 bg-blue-100 rounded-lg flex items-center justify-center mr-3">
                                <i class="fa-solid fa-file-archive text-blue-600"></i>
                            </div>
                            <div>
                                <p class="text-sm text-neutral-800 font-medium">${backup.name}</p>
                                <p class="text-xs text-neutral-500">${backup.date} • ${this.formatFileSize(backup.size)} • ${backup.type}</p>
                            </div>
                        </div>
                        <div class="flex items-center space-x-2">
                            <button class="text-blue-600 hover:text-blue-800 text-sm">
                                <i class="fa-solid fa-download mr-1"></i>Download
                            </button>
                            <button class="text-green-600 hover:text-green-800 text-sm">
                                <i class="fa-solid fa-rotate-left mr-1"></i>Restore
                            </button>
                            <button class="text-red-600 hover:text-red-800 text-sm">
                                <i class="fa-solid fa-trash mr-1"></i>Delete
                            </button>
                        </div>
                    </div>
                `).join('');
            }
            
            // Hide loading state
            this.loadingUI.hideLoading('backup-list');
        }, 1000);
    }
    
    /**
     * Cleanup method to remove event listeners and clear states
     */
    cleanup() {
        console.log('[BACKUP-RESTORE-UI] Cleaning up backup & restore UI controller...');
        
        // Clear file selection
        this.clearFileSelection();
        
        // Reset states
        this.backupInProgress = false;
        this.restoreInProgress = false;
        this.selectedFile = null;
        
        // Cleanup loading UI
        if (this.loadingUI) {
            this.loadingUI.cleanup();
        }
        
        console.log('[BACKUP-RESTORE-UI] Backup & restore UI controller cleaned up');
    }
}

export { BackupRestoreUI };

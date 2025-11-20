/**
 * Backup & Restore Data Loading UI Component
 * Provides loading states and skeleton screens for backup and restore components
 */

class BackupRestoreDataLoadingUI {
    constructor() {
        this.loadingStates = new Map();
        this.init();
    }
    
    init() {
        console.log('[BACKUP-RESTORE-LOADING-UI] Initializing backup & restore loading UI component...');
        this.setupLoadingStyles();
    }
    
    /**
     * Setup CSS styles for loading animations
     */
    setupLoadingStyles() {
        const styleId = 'backup-restore-loading-ui-styles';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = `
                .backup-skeleton {
                    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
                    background-size: 200% 100%;
                    animation: backup-loading 1.5s infinite;
                    border-radius: 0.25rem;
                }
                
                @keyframes backup-loading {
                    0% { background-position: 200% 0; }
                    100% { background-position: -200% 0; }
                }
                
                .backup-loading-pulse {
                    animation: backup-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
                }
                
                @keyframes backup-pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: .5; }
                }
                
                .backup-loading-spinner {
                    border: 2px solid #f3f3f3;
                    border-top: 2px solid #3b82f6;
                    border-radius: 50%;
                    width: 20px;
                    height: 20px;
                    animation: backup-spin 1s linear infinite;
                }
                
                @keyframes backup-spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
                
                .backup-progress-bar {
                    background: linear-gradient(90deg, #3b82f6 0%, #60a5fa 50%, #3b82f6 100%);
                    background-size: 200% 100%;
                    animation: backup-progress 2s ease-in-out infinite;
                }
                
                @keyframes backup-progress {
                    0% { background-position: 0% 0%; }
                    50% { background-position: 100% 0%; }
                    100% { background-position: 0% 0%; }
                }
            `;
            document.head.appendChild(style);
        }
    }
    
    /**
     * Show loading skeleton for backup creation section
     */
    showBackupCreationLoading(containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        const loadingHtml = `
            <div class="space-y-6">
                <!-- Page Header Skeleton -->
                <div class="flex items-center justify-between">
                    <div>
                        <div class="backup-skeleton h-8 w-64 mb-2"></div>
                        <div class="backup-skeleton h-4 w-96"></div>
                    </div>
                </div>
                
                <!-- Create Backup Section Skeleton -->
                <div class="bg-white rounded-lg border border-neutral-200 p-6">
                    <div class="flex items-center justify-between mb-4">
                        <div class="flex items-center">
                            <div class="backup-skeleton w-8 h-8 rounded-lg mr-3"></div>
                            <div class="backup-skeleton h-6 w-32"></div>
                        </div>
                        <div class="backup-skeleton w-5 h-5"></div>
                    </div>
                    
                    <div class="space-y-4">
                        <div class="backup-skeleton h-4 w-full mb-2"></div>
                        <div class="backup-skeleton h-4 w-3/4"></div>
                        
                        <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                            <div class="backup-skeleton h-5 w-32 mb-3"></div>
                            <div class="space-y-3">
                                <div class="flex items-center">
                                    <div class="backup-skeleton w-4 h-4 rounded mr-3"></div>
                                    <div class="backup-skeleton h-4 w-24"></div>
                                </div>
                                <div class="flex items-center">
                                    <div class="backup-skeleton w-4 h-4 rounded mr-3"></div>
                                    <div class="backup-skeleton h-4 w-28"></div>
                                </div>
                            </div>
                        </div>
                        
                        <div class="flex justify-end">
                            <div class="backup-skeleton h-10 w-32 rounded-md"></div>
                        </div>
                    </div>
                </div>
                
                <!-- Encrypted Backup Section Skeleton -->
                <div class="bg-white rounded-lg border border-neutral-200 p-6">
                    <div class="flex items-center justify-between mb-4">
                        <div class="flex items-center">
                            <div class="backup-skeleton w-8 h-8 rounded-lg mr-3"></div>
                            <div class="backup-skeleton h-6 w-40 mr-3"></div>
                            <div class="backup-skeleton h-6 w-24 rounded-full"></div>
                        </div>
                        <div class="backup-skeleton w-5 h-5"></div>
                    </div>
                    
                    <div class="space-y-4">
                        <div class="backup-skeleton h-4 w-full mb-2"></div>
                        <div class="backup-skeleton h-4 w-2/3"></div>
                        
                        <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                            <div class="backup-skeleton h-5 w-32 mb-3"></div>
                            <div class="space-y-3">
                                <div class="backup-skeleton h-10 w-full rounded-md"></div>
                                <div class="backup-skeleton h-10 w-full rounded-md"></div>
                            </div>
                        </div>
                        
                        <div class="flex justify-end">
                            <div class="backup-skeleton h-10 w-40 rounded-md"></div>
                        </div>
                    </div>
                </div>
                
                <!-- Restore Section Skeleton -->
                <div class="bg-white rounded-lg border border-neutral-200 p-6">
                    <div class="flex items-center justify-between mb-4">
                        <div class="flex items-center">
                            <div class="backup-skeleton w-8 h-8 rounded-lg mr-3"></div>
                            <div class="backup-skeleton h-6 w-32"></div>
                        </div>
                        <div class="backup-skeleton w-5 h-5"></div>
                    </div>
                    
                    <div class="space-y-4">
                        <div class="backup-skeleton h-4 w-full mb-2"></div>
                        <div class="backup-skeleton h-4 w-3/4"></div>
                        
                        <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-6">
                            <div class="backup-skeleton h-32 w-full rounded-lg mb-4"></div>
                            <div class="flex justify-between">
                                <div class="backup-skeleton h-10 w-20 rounded-md"></div>
                                <div class="backup-skeleton h-10 w-24 rounded-md"></div>
                            </div>
                        </div>
                    </div>
                </div>
                
                <!-- Recent Backups Section Skeleton -->
                <div class="bg-white rounded-lg border border-neutral-200 p-6">
                    <div class="flex items-center justify-between mb-4">
                        <div class="flex items-center">
                            <div class="backup-skeleton w-8 h-8 rounded-lg mr-3"></div>
                            <div class="backup-skeleton h-6 w-32"></div>
                        </div>
                        <div class="backup-skeleton w-5 h-5"></div>
                    </div>
                    
                    <div class="space-y-3">
                        ${Array(3).fill('').map(() => `
                            <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                                <div class="flex items-center justify-between">
                                    <div class="flex items-center">
                                        <div class="backup-skeleton w-10 h-10 rounded-lg mr-3"></div>
                                        <div>
                                            <div class="backup-skeleton h-4 w-48 mb-1"></div>
                                            <div class="backup-skeleton h-3 w-32"></div>
                                        </div>
                                    </div>
                                    <div class="flex space-x-2">
                                        <div class="backup-skeleton h-8 w-16 rounded"></div>
                                        <div class="backup-skeleton h-8 w-16 rounded"></div>
                                        <div class="backup-skeleton h-8 w-16 rounded"></div>
                                    </div>
                                </div>
                            </div>
                        `).join('')}
                    </div>
                </div>
            </div>
        `;
        
        container.innerHTML = loadingHtml;
        this.loadingStates.set(containerId, 'backup-creation');
    }
    
    /**
     * Show loading skeleton for backup progress
     */
    showBackupProgressLoading(containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        const loadingHtml = `
            <div class="bg-white rounded-lg border border-neutral-200 p-6">
                <div class="flex items-center mb-4">
                    <div class="w-8 h-8 bg-orange-100 rounded-lg flex items-center justify-center mr-3">
                        <div class="backup-loading-spinner"></div>
                    </div>
                    <h2 class="text-lg text-neutral-900">Creating Backup...</h2>
                </div>
                
                <div class="space-y-4">
                    <div class="bg-neutral-50 rounded-lg p-4">
                        <div class="flex justify-between items-center mb-2">
                            <span class="text-sm text-neutral-700">Progress</span>
                            <span class="text-sm text-neutral-600">0%</span>
                        </div>
                        <div class="w-full bg-neutral-200 rounded-full h-2">
                            <div class="backup-progress-bar h-2 rounded-full" style="width: 0%"></div>
                        </div>
                        <p class="text-xs text-neutral-500 mt-2">Initializing...</p>
                    </div>
                </div>
            </div>
        `;
        
        container.innerHTML = loadingHtml;
        this.loadingStates.set(containerId, 'backup-progress');
    }
    
    /**
     * Show loading skeleton for restore progress
     */
    showRestoreProgressLoading(containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        const loadingHtml = `
            <div class="bg-white rounded-lg border border-neutral-200 p-6">
                <div class="flex items-center mb-4">
                    <div class="w-8 h-8 bg-green-100 rounded-lg flex items-center justify-center mr-3">
                        <div class="backup-loading-spinner" style="border-top-color: #10b981;"></div>
                    </div>
                    <h2 class="text-lg text-neutral-900">Restoring Backup...</h2>
                </div>
                
                <div class="space-y-4">
                    <div class="bg-neutral-50 rounded-lg p-4">
                        <div class="flex justify-between items-center mb-2">
                            <span class="text-sm text-neutral-700">Progress</span>
                            <span class="text-sm text-neutral-600">0%</span>
                        </div>
                        <div class="w-full bg-neutral-200 rounded-full h-2">
                            <div class="bg-green-600 h-2 rounded-full transition-all duration-300" style="width: 0%"></div>
                        </div>
                        <p class="text-xs text-neutral-500 mt-2">Validating backup...</p>
                    </div>
                </div>
            </div>
        `;
        
        container.innerHTML = loadingHtml;
        this.loadingStates.set(containerId, 'restore-progress');
    }
    
    /**
     * Show loading skeleton for backup history
     */
    showBackupHistoryLoading(containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        const loadingHtml = `
            <div class="space-y-3">
                ${Array(5).fill('').map(() => `
                    <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <div class="backup-skeleton w-10 h-10 rounded-lg mr-3"></div>
                                <div>
                                    <div class="backup-skeleton h-4 w-56 mb-1"></div>
                                    <div class="backup-skeleton h-3 w-40"></div>
                                </div>
                            </div>
                            <div class="flex space-x-2">
                                <div class="backup-skeleton h-8 w-16 rounded"></div>
                                <div class="backup-skeleton h-8 w-16 rounded"></div>
                                <div class="backup-skeleton h-8 w-16 rounded"></div>
                            </div>
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
        
        container.innerHTML = loadingHtml;
        this.loadingStates.set(containerId, 'backup-history');
    }
    
    /**
     * Update progress for loading states
     */
    updateProgress(containerId, progress, status) {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        const progressBar = container.querySelector('[class*="progress-bar"], [class*="bg-green-600"], [class*="bg-blue-600"]');
        const progressText = container.querySelector('.text-neutral-600');
        const statusText = container.querySelector('.text-neutral-500');
        
        if (progressBar) {
            progressBar.style.width = progress + '%';
        }
        
        if (progressText) {
            progressText.textContent = Math.round(progress) + '%';
        }
        
        if (statusText) {
            statusText.textContent = status;
        }
    }
    
    /**
     * Hide loading state for a container
     */
    hideLoading(containerId) {
        this.loadingStates.delete(containerId);
    }
    
    /**
     * Check if a container is in loading state
     */
    isLoading(containerId) {
        return this.loadingStates.has(containerId);
    }
    
    /**
     * Get loading state type for a container
     */
    getLoadingState(containerId) {
        return this.loadingStates.get(containerId);
    }
    
    /**
     * Clear all loading states
     */
    clearAllLoadingStates() {
        this.loadingStates.clear();
    }
    
    /**
     * Show inline loading spinner for buttons
     */
    showButtonLoading(buttonId) {
        const button = document.getElementById(buttonId);
        if (!button) return;
        
        const originalContent = button.innerHTML;
        button.setAttribute('data-original-content', originalContent);
        button.disabled = true;
        button.innerHTML = `
            <div class="backup-loading-spinner" style="width: 16px; height: 16px; border-width: 1px; margin-right: 8px;"></div>
            Processing...
        `;
    }
    
    /**
     * Hide inline loading spinner for buttons
     */
    hideButtonLoading(buttonId) {
        const button = document.getElementById(buttonId);
        if (!button) return;
        
        const originalContent = button.getAttribute('data-original-content');
        if (originalContent) {
            button.innerHTML = originalContent;
            button.removeAttribute('data-original-content');
        }
        button.disabled = false;
    }
    
    /**
     * Cleanup method
     */
    cleanup() {
        console.log('[BACKUP-RESTORE-LOADING-UI] Cleaning up loading UI component...');
        
        // Clear all loading states
        this.clearAllLoadingStates();
        
        // Remove styles if no other instances are using them
        const style = document.getElementById('backup-restore-loading-ui-styles');
        if (style) {
            style.remove();
        }
        
        console.log('[BACKUP-RESTORE-LOADING-UI] Loading UI component cleaned up');
    }
}

export { BackupRestoreDataLoadingUI };

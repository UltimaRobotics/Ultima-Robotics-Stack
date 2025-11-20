/**
 * Backup & Restore Module Entry Point
 * Exports all backup and restore related components and utilities
 */

// Import main components
import { BackupRestoreManager } from './backup-restore.js';
import { BackupRestoreUI } from './backup-restore-ui.js';

// Re-export for external use
export { BackupRestoreManager } from './backup-restore.js';
export { BackupRestoreUI } from './backup-restore-ui.js';

// Module info
export const MODULE_INFO = {
    name: 'backup-restore',
    version: '1.0.0',
    description: 'Backup and restore management module',
    dependencies: [
        'http-client',
        'jwt-token-manager'
    ],
    features: [
        'Full and partial backup creation',
        'AES-256 encrypted backup support',
        'Drag and drop file upload',
        'Backup file integrity validation',
        'Real-time backup progress monitoring',
        'Recent backup history tracking',
        'Secure restore operations'
    ]
};

// Initialize function for integration with main application
export async function initializeBackupRestore(sourceManager) {
    try {
        console.log('[BACKUP-RESTORE-MODULE] Initializing backup & restore module...');
        
        // Initialize backup restore manager
        const backupRestoreManager = new BackupRestoreManager();
        
        // Initialize UI controller
        const backupRestoreUI = new BackupRestoreUI(backupRestoreManager);
        
        // Wait for initialization to complete
        await new Promise((resolve) => {
            if (backupRestoreManager.isInitialized && backupRestoreManager.isInitialized()) {
                resolve();
            } else {
                document.addEventListener('backupRestoreInitialized', resolve, { once: true });
            }
        });
        
        console.log('[BACKUP-RESTORE-MODULE] Backup & restore module initialized successfully');
        
        return {
            manager: backupRestoreManager,
            ui: backupRestoreUI
        };
    } catch (error) {
        console.error('[BACKUP-RESTORE-MODULE] Failed to initialize backup & restore module:', error);
        throw error;
    }
}

// Cleanup function
export function cleanupBackupRestore(backupRestoreComponents) {
    try {
        console.log('[BACKUP-RESTORE-MODULE] Cleaning up backup & restore module...');
        
        if (backupRestoreComponents) {
            // Cleanup UI controller first (stops timers, loading states)
            if (backupRestoreComponents.ui) {
                backupRestoreComponents.ui.cleanup();
            }
            
            // Cleanup manager
            if (backupRestoreComponents.manager) {
                backupRestoreComponents.manager.destroy();
            }
        }
        
        console.log('[BACKUP-RESTORE-MODULE] Backup & restore module cleaned up successfully');
    } catch (error) {
        console.error('[BACKUP-RESTORE-MODULE] Error during cleanup:', error);
    }
}

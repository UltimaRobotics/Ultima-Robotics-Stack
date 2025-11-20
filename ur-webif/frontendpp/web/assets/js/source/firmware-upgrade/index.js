/**
 * Firmware Upgrade Module Entry Point
 * Exports all firmware upgrade related components and utilities
 */

// Import main components
import { FirmwareUpgradeManager } from './firmware-upgrade.js';
import { FirmwareUpgradeUI } from './firmware-upgrade-ui.js';

// Re-export for external use
export { FirmwareUpgradeManager } from './firmware-upgrade.js';
export { FirmwareUpgradeUI } from './firmware-upgrade-ui.js';

// Module info
export const MODULE_INFO = {
    name: 'firmware-upgrade',
    version: '1.0.0',
    description: 'Firmware upgrade management module',
    dependencies: [
        'http-client',
        'jwt-token-manager'
    ],
    features: [
        'File upload verification',
        'Online update checking',
        'TFTP server support',
        'Real-time progress monitoring',
        'Upgrade history tracking',
        'Digital signature verification'
    ]
};

// Initialize function for integration with main application
export async function initializeFirmwareUpgrade(sourceManager) {
    try {
        console.log('[FIRMWARE-UPGRADE-MODULE] Initializing firmware upgrade module...');
        
        // Initialize firmware upgrade manager
        const firmwareUpgradeManager = new FirmwareUpgradeManager();
        
        // Initialize UI controller
        const firmwareUpgradeUI = new FirmwareUpgradeUI(firmwareUpgradeManager);
        
        // Wait for initialization to complete
        await new Promise((resolve) => {
            if (firmwareUpgradeManager.isInitialized()) {
                resolve();
            } else {
                document.addEventListener('firmwareUpgradeInitialized', resolve, { once: true });
            }
        });
        
        console.log('[FIRMWARE-UPGRADE-MODULE] Firmware upgrade module initialized successfully');
        
        return {
            manager: firmwareUpgradeManager,
            ui: firmwareUpgradeUI
        };
    } catch (error) {
        console.error('[FIRMWARE-UPGRADE-MODULE] Failed to initialize firmware upgrade module:', error);
        throw error;
    }
}

// Cleanup function
export function cleanupFirmwareUpgrade(firmwareUpgradeComponents) {
    try {
        console.log('[FIRMWARE-UPGRADE-MODULE] Cleaning up firmware upgrade module...');
        
        if (firmwareUpgradeComponents) {
            // Cleanup UI controller first (stops timers, loading states)
            if (firmwareUpgradeComponents.ui) {
                firmwareUpgradeComponents.ui.cleanup();
            }
            
            // Cleanup manager
            if (firmwareUpgradeComponents.manager) {
                firmwareUpgradeComponents.manager.destroy();
            }
        }
        
        console.log('[FIRMWARE-UPGRADE-MODULE] Firmware upgrade module cleaned up successfully');
    } catch (error) {
        console.error('[FIRMWARE-UPGRADE-MODULE] Error during cleanup:', error);
    }
}

/**
 * MAVLink Extension Module Entry Point
 * Exports all MAVLink extension related components and utilities
 */

// Import main components
import { MavlinkExtensionManager } from './mavlink-extension.js';
import { MavlinkExtensionUI } from './mavlink-extension-ui.js';

// Re-export for external use
export { MavlinkExtensionManager } from './mavlink-extension.js';
export { MavlinkExtensionUI } from './mavlink-extension-ui.js';

// Module info
export const MODULE_INFO = {
    name: 'mavlink-extension',
    version: '1.0.0',
    description: 'MAVLink extension management module (Demo Mode)',
    dependencies: [],
    features: [
        'Demo endpoint management',
        'Simulated real-time monitoring',
        'Demo configuration import/export',
        'Demo system validation',
        'Protocol support (UDP/TCP)',
        'Auto-start capabilities',
        'Simulated bandwidth monitoring',
        'Demo message statistics'
    ]
};

// Initialize function for integration with main application
export async function initializeMavlinkExtension(sourceManager) {
    try {
        console.log('[MAVLINK-EXTENSION-MODULE] Initializing MAVLink extension module...');
        
        // Initialize MAVLink extension manager
        const mavlinkExtensionManager = new MavlinkExtensionManager();
        
        // Wait for manager to be initialized before creating UI
        await new Promise((resolve) => {
            if (mavlinkExtensionManager.isInitialized()) {
                resolve();
            } else {
                mavlinkExtensionManager.on('initialized', resolve);
            }
        });
        
        // Initialize UI controller after manager is ready
        const mavlinkExtensionUI = new MavlinkExtensionUI(mavlinkExtensionManager);
        
        // Inject modal HTML into the DOM BEFORE UI initialization
        const modalContainer = document.createElement('div');
        modalContainer.innerHTML = mavlinkExtensionUI.generateModalHTML();
        const modalElement = modalContainer.firstElementChild; // This is now the container with all modals
        document.body.appendChild(modalElement);
        console.log('[MAVLINK-EXTENSION-MODULE] Modal container HTML injected into DOM:', modalElement);
        console.log('[MAVLINK-EXTENSION-MODULE] Available modals:', modalElement.querySelectorAll('[id$="-modal"]').length);
        
        // Wait for UI to be initialized (now modals exist in DOM)
        await mavlinkExtensionUI.init();
        
        console.log('[MAVLINK-EXTENSION-MODULE] MAVLink extension module initialized successfully');
        
        return {
            manager: mavlinkExtensionManager,
            ui: mavlinkExtensionUI
        };
    } catch (error) {
        console.error('[MAVLINK-EXTENSION-MODULE] Failed to initialize MAVLink extension module:', error);
        throw error;
    }
}

// Export function to get main content HTML
export function getMavlinkExtensionContentHTML() {
    // Create a temporary UI instance just for HTML generation
    // This won't affect the actual UI initialization
    const tempUI = new MavlinkExtensionUI(null);
    return tempUI.generateMainContentHTML();
}

// Cleanup function
export function cleanupMavlinkExtension(mavlinkExtensionComponents) {
    try {
        console.log('[MAVLINK-EXTENSION-MODULE] Cleaning up MAVLink extension module...');
        
        if (mavlinkExtensionComponents) {
            // Cleanup UI controller first (stops timers, loading states)
            if (mavlinkExtensionComponents.ui) {
                mavlinkExtensionComponents.ui.cleanup();
            }
            
            // Cleanup manager
            if (mavlinkExtensionComponents.manager) {
                mavlinkExtensionComponents.manager.destroy();
            }
            
            // Remove modal HTML from DOM
            const modals = [
                'endpoint-modal',
                'import-modal', 
                'stop-all-modal',
                'delete-endpoint-modal'
            ];
            
            modals.forEach(modalId => {
                const modal = document.getElementById(modalId);
                if (modal) {
                    modal.remove();
                }
            });
        }
        
        console.log('[MAVLINK-EXTENSION-MODULE] MAVLink extension module cleaned up successfully');
    } catch (error) {
        console.error('[MAVLINK-EXTENSION-MODULE] Error during cleanup:', error);
    }
}

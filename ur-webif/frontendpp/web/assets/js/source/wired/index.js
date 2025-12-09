/**
 * Wired Configuration Module Entry Point
 * Exports the main wired configuration components
 */

import { WiredManager } from './wired.js';
import { WiredUI } from './wired-ui.js';

/**
 * Initialize wired configuration module
 */
export function initializeWired(httpClient, container) {
    console.log('[WIRED] Initializing wired configuration module');
    
    const wiredUI = new WiredUI(httpClient, container);
    
    return {
        wiredUI,
        wiredManager: wiredUI.wiredManager,
        
        /**
         * Cleanup wired configuration module
         */
        destroy() {
            console.log('[WIRED] Cleaning up wired configuration module');
            if (wiredUI) {
                wiredUI.destroy();
            }
        }
    };
}

/**
 * Get wired configuration content HTML
 * This can be used by the main UI to get the content
 */
export function getWiredConfigContentHTML() {
    return `
        <div id="wired-config-container" class="wired-config-container">
            <!-- Content will be rendered by WiredUI -->
            <div class="flex items-center justify-center h-64">
                <div class="text-center">
                    <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                    <p class="text-neutral-600">Loading wired configuration...</p>
                </div>
            </div>
        </div>
    `;
}

/**
 * Export components for individual use
 */
export { WiredManager, WiredUI };

/**
 * Default export for easy importing
 */
export default {
    initializeWired,
    getWiredConfigContentHTML,
    WiredManager,
    WiredUI
};

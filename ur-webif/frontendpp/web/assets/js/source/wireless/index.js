/**
 * Wireless Configuration Module Entry Point
 * Exports the main wireless configuration components
 */

import { WirelessManager } from './wireless.js';
import { WirelessUI } from './wireless-ui.js';

/**
 * Initialize wireless configuration module
 */
export function initializeWireless(httpClient, container) {
    
    const wirelessUI = new WirelessUI(httpClient, container);
    
    return {
        wirelessUI,
        wirelessManager: wirelessUI.wirelessManager,
        
        /**
         * Cleanup wireless configuration module
         */
        destroy() {
            if (wirelessUI) {
                wirelessUI.destroy();
            }
        }
    };
}

/**
 * Get wireless configuration content HTML
 * This can be used by the main UI to get the content
 */
export function getWirelessConfigContentHTML() {
    return `
        <div id="wireless-config-container" class="wireless-config-container">
            <!-- Content will be rendered by WirelessUI -->
            <div class="flex items-center justify-center h-64">
                <div class="text-center">
                    <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                    <p class="text-neutral-600">Loading wireless configuration...</p>
                </div>
            </div>
        </div>
    `;
}

/**
 * Export components for individual use
 */
export { WirelessManager, WirelessUI };

/**
 * Default export for easy importing
 */
export default {
    initializeWireless,
    getWirelessConfigContentHTML,
    WirelessManager,
    WirelessUI
};

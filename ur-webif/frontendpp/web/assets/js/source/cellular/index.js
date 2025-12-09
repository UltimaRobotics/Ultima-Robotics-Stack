/**
 * Cellular Configuration Module Entry Point
 * Exports the main cellular configuration components
 */

import { CellularManager } from './cellular.js';
import { CellularUI } from './cellular-ui.js';

/**
 * Initialize cellular configuration module
 */
export function initializeCellular(httpClient, container) {
    console.log('[CELLULAR] Initializing cellular configuration module');
    
    const cellularUI = new CellularUI(httpClient, container);
    
    return {
        cellularUI,
        cellularManager: cellularUI.cellularManager,
        
        /**
         * Cleanup cellular configuration module
         */
        destroy() {
            console.log('[CELLULAR] Cleaning up cellular configuration module');
            if (cellularUI) {
                cellularUI.destroy();
            }
        }
    };
}

/**
 * Get cellular configuration content HTML
 * This can be used by the main UI to get the content
 */
export function getCellularConfigContentHTML() {
    return `
        <div id="cellular-config-container" class="cellular-config-container">
            <!-- Content will be rendered by CellularUI -->
            <div class="flex items-center justify-center h-64">
                <div class="text-center">
                    <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                    <p class="text-neutral-600">Loading cellular configuration...</p>
                </div>
            </div>
        </div>
    `;
}

/**
 * Export components for individual use
 */
export { CellularManager, CellularUI };

/**
 * Default export for easy importing
 */
export default {
    initializeCellular,
    getCellularConfigContentHTML,
    CellularManager,
    CellularUI
};

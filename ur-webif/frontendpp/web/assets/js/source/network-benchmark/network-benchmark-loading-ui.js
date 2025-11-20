/**
 * Network Benchmark Loading UI Component
 * Handles loading states and UI status display for network diagnostic tools
 */

class NetworkBenchmarkLoadingUI {
    constructor() {
        this.popups = {
            traceroute: null,
            pingTest: null,
            dnsLookup: null,
            bandwidthTest: null
        };
        this.currentPopup = null;
        this.isInitialized = false;
        this.isLoading = false;
    }

    /**
     * Initialize the loading UI
     */
    initialize() {
        this.createPopups();
        this.attachPopupEventListeners();
        this.isInitialized = true;
    }

    /**
     * Create all popup modals (basic structure for loading states)
     */
    createPopups() {
        document.body.insertAdjacentHTML('beforeend', `
            <!-- Traceroute Popup -->
            <div id="traceroutePopup" class="popup-overlay" style="position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: none; align-items: center; justify-content: center; z-index: 1000; opacity: 0; visibility: hidden; transition: opacity 0.3s ease, visibility 0.3s ease;">
                <div class="popup-content" style="background: white; border-radius: 12px; padding: 0; width: 800px; max-width: 90vw; max-height: 85vh; transform: scale(0.9); transition: transform 0.3s ease; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1); display: flex; flex-direction: column;">
                    <!-- Header -->
                    <header class="bg-white text-neutral-800 px-6 py-4 rounded-t-lg flex items-center justify-between border-b border-neutral-200 flex-shrink-0">
                        <div class="flex items-center gap-3">
                            <i class="fa-solid fa-grip text-xl text-neutral-700"></i>
                            <div>
                                <h2 class="text-xl">Traceroute Test</h2>
                                <p class="text-neutral-500 text-sm">Network path tracing and hop analysis</p>
                            </div>
                        </div>
                        <button class="close-traceroute text-neutral-500 hover:text-neutral-800">
                            <i class="fa-solid fa-xmark text-xl"></i>
                        </button>
                    </header>

                    <!-- Loading Content -->
                    <main class="p-6 flex-1 overflow-y-auto flex items-center justify-center" style="max-height: calc(85vh - 80px);">
                        <div class="text-center">
                            <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-neutral-900 mx-auto mb-4"></div>
                            <p class="text-neutral-600">Loading traceroute configuration...</p>
                        </div>
                    </main>
                </div>
            </div>

            <!-- Ping Test Popup -->
            <div id="pingTestPopup" class="popup-overlay" style="position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: none; align-items: center; justify-content: center; z-index: 1000; opacity: 0; visibility: hidden; transition: opacity 0.3s ease, visibility 0.3s ease;">
                <div class="popup-content bg-neutral-100 rounded-lg w-full max-w-2xl" style="transform: scale(0.9); transition: transform 0.3s ease; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);">
                    <!-- Header -->
                    <div class="flex items-start justify-between p-6 border-b border-neutral-300">
                        <div class="flex items-start gap-4">
                            <div class="w-12 h-12 bg-neutral-300 rounded-lg flex items-center justify-center">
                                <i class="fa-solid fa-network-wired text-xl text-neutral-700"></i>
                            </div>
                            <div>
                                <h2 class="text-xl text-neutral-900">Network Ping Test</h2>
                                <p class="text-sm text-neutral-600 mt-1">Test network connectivity and latency</p>
                            </div>
                        </div>
                        <button class="close-ping-test text-neutral-500 hover:text-neutral-700 text-xl">
                            <i class="fa-solid fa-xmark"></i>
                        </button>
                    </div>

                    <!-- Loading Content -->
                    <div class="p-6 flex items-center justify-center" style="min-height: 200px;">
                        <div class="text-center">
                            <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-neutral-900 mx-auto mb-4"></div>
                            <p class="text-neutral-600">Loading ping test configuration...</p>
                        </div>
                    </div>
                </div>
            </div>

            <!-- DNS Lookup Popup -->
            <div id="dnsLookupPopup" class="popup-overlay" style="position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: none; align-items: center; justify-content: center; z-index: 1000; opacity: 0; visibility: hidden; transition: opacity 0.3s ease, visibility 0.3s ease;">
                <div class="popup-content bg-white rounded-lg w-full max-w-2xl" style="transform: scale(0.9); transition: transform 0.3s ease; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);">
                    <!-- Header -->
                    <div class="flex items-start justify-between p-6 border-b border-neutral-200">
                        <div class="flex items-start gap-4">
                            <div class="w-12 h-12 bg-neutral-100 rounded-lg flex items-center justify-center">
                                <i class="fa-solid fa-search text-xl text-neutral-600"></i>
                            </div>
                            <div>
                                <h2 class="text-xl text-neutral-900">DNS Lookup</h2>
                                <p class="text-sm text-neutral-600 mt-1">Query DNS records for domain names</p>
                            </div>
                        </div>
                        <button class="close-dns-lookup text-neutral-500 hover:text-neutral-700 text-xl">
                            <i class="fa-solid fa-xmark"></i>
                        </button>
                    </div>

                    <!-- Loading Content -->
                    <div class="p-6 flex items-center justify-center" style="min-height: 200px;">
                        <div class="text-center">
                            <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-neutral-900 mx-auto mb-4"></div>
                            <p class="text-neutral-600">Loading DNS lookup configuration...</p>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Bandwidth Test Popup -->
            <div id="bandwidthTestPopup" class="popup-overlay" style="position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: none; align-items: center; justify-content: center; z-index: 1000; opacity: 0; visibility: hidden; transition: opacity 0.3s ease, visibility 0.3s ease;">
                <div class="popup-content bg-white rounded-lg w-full max-w-4xl" style="transform: scale(0.9); transition: transform 0.3s ease; box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);">
                    <!-- Header -->
                    <div class="flex items-start justify-between p-6 border-b border-neutral-200">
                        <div class="flex items-start gap-4">
                            <div class="w-12 h-12 bg-neutral-100 rounded-lg flex items-center justify-center">
                                <i class="fa-solid fa-tachometer-alt text-xl text-neutral-600"></i>
                            </div>
                            <div>
                                <h2 class="text-xl text-neutral-900">Bandwidth Test</h2>
                                <p class="text-sm text-neutral-600 mt-1">Test network bandwidth and speed</p>
                            </div>
                        </div>
                        <button class="close-bandwidth-test text-neutral-500 hover:text-neutral-700 text-xl">
                            <i class="fa-solid fa-xmark"></i>
                        </button>
                    </div>

                    <!-- Loading Content -->
                    <div class="p-6 flex items-center justify-center" style="min-height: 300px;">
                        <div class="text-center">
                            <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-neutral-900 mx-auto mb-4"></div>
                            <p class="text-neutral-600">Loading bandwidth test configuration...</p>
                        </div>
                    </div>
                </div>
            </div>
        `);

        // Store popup references
        this.popups.traceroute = document.getElementById('traceroutePopup');
        this.popups.pingTest = document.getElementById('pingTestPopup');
        this.popups.dnsLookup = document.getElementById('dnsLookupPopup');
        this.popups.bandwidthTest = document.getElementById('bandwidthTestPopup');
    }

    /**
     * Attach popup event listeners
     */
    attachPopupEventListeners() {
        // Traceroute popup
        const tracerouteCloseBtn = document.querySelector('.close-traceroute');
        if (tracerouteCloseBtn) {
            tracerouteCloseBtn.addEventListener('click', () => this.hideTraceroutePopup());
        }

        // Ping test popup
        const pingCloseBtn = document.querySelector('.close-ping-test');
        if (pingCloseBtn) {
            pingCloseBtn.addEventListener('click', () => this.hidePingTestPopup());
        }

        // DNS lookup popup
        const dnsCloseBtn = document.querySelector('.close-dns-lookup');
        if (dnsCloseBtn) {
            dnsCloseBtn.addEventListener('click', () => this.hideDnsLookupPopup());
        }

        // Bandwidth test popup
        const bandwidthCloseBtn = document.querySelector('.close-bandwidth-test');
        if (bandwidthCloseBtn) {
            bandwidthCloseBtn.addEventListener('click', () => this.hideBandwidthTestPopup());
        }

        // Close popup when clicking overlay
        Object.values(this.popups).forEach(popup => {
            if (popup) {
                popup.addEventListener('click', (e) => {
                    if (e.target === popup) {
                        this.hideCurrentPopup();
                    }
                });
            }
        });
    }

    /**
     * Show loading state for the entire network benchmark UI
     */
    showLoadingState() {
        this.isLoading = true;
        
        // Disable all interactive elements
        this.disableInteractiveElements();
        
        // Show loading overlays
        this.showLoadingOverlays();
        
        // Show loading spinner for main content
        this.showMainContentLoading();
    }

    /**
     * Hide loading state for the entire network benchmark UI
     */
    hideLoadingState() {
        this.isLoading = false;
        
        // Enable all interactive elements
        this.enableInteractiveElements();
        
        // Hide loading overlays
        this.hideLoadingOverlays();
        
        // Hide loading spinner for main content
        this.hideMainContentLoading();
    }

    /**
     * Disable all interactive elements during loading
     */
    disableInteractiveElements() {
        // Disable all buttons
        const buttons = document.querySelectorAll('#network-benchmark-container button, #network-benchmark-container input[type="button"], #network-benchmark-container input[type="submit"]');
        buttons.forEach(button => {
            button.disabled = true;
            button.classList.add('opacity-50', 'cursor-not-allowed');
        });

        // Disable all inputs
        const inputs = document.querySelectorAll('#network-benchmark-container input, #network-benchmark-container select, #network-benchmark-container textarea');
        inputs.forEach(input => {
            input.disabled = true;
            input.classList.add('opacity-50', 'cursor-not-allowed');
        });

        // Disable all links
        const links = document.querySelectorAll('#network-benchmark-container a');
        links.forEach(link => {
            link.classList.add('pointer-events-none', 'opacity-50');
        });
    }

    /**
     * Enable all interactive elements after loading
     */
    enableInteractiveElements() {
        // Enable all buttons
        const buttons = document.querySelectorAll('#network-benchmark-container button, #network-benchmark-container input[type="button"], #network-benchmark-container input[type="submit"]');
        buttons.forEach(button => {
            button.disabled = false;
            button.classList.remove('opacity-50', 'cursor-not-allowed');
        });

        // Enable all inputs
        const inputs = document.querySelectorAll('#network-benchmark-container input, #network-benchmark-container select, #network-benchmark-container textarea');
        inputs.forEach(input => {
            input.disabled = false;
            input.classList.remove('opacity-50', 'cursor-not-allowed');
        });

        // Enable all links
        const links = document.querySelectorAll('#network-benchmark-container a');
        links.forEach(link => {
            link.classList.remove('pointer-events-none', 'opacity-50');
        });
    }

    /**
     * Show loading overlays for different sections
     */
    showLoadingOverlays() {
        // Server list loading overlay
        const serverListContainer = document.querySelector('#network-benchmark-container .server-list-container');
        if (serverListContainer) {
            this.addLoadingOverlay(serverListContainer, 'Loading servers...');
        }

        // Statistics loading overlay
        const statsContainer = document.querySelector('#network-benchmark-container .stats-container');
        if (statsContainer) {
            this.addLoadingOverlay(statsContainer, 'Loading statistics...');
        }

        // Chart loading overlay
        const chartContainer = document.querySelector('#network-benchmark-container .chart-container');
        if (chartContainer) {
            this.addLoadingOverlay(chartContainer, 'Loading chart data...');
        }
    }

    /**
     * Hide loading overlays
     */
    hideLoadingOverlays() {
        // Remove all loading overlays
        const overlays = document.querySelectorAll('#network-benchmark-container .loading-overlay');
        overlays.forEach(overlay => overlay.remove());
    }

    /**
     * Add loading overlay to a container
     */
    addLoadingOverlay(container, message = 'Loading...') {
        if (!container) return;

        const overlay = document.createElement('div');
        overlay.className = 'loading-overlay absolute inset-0 bg-white bg-opacity-90 flex items-center justify-center z-50';
        overlay.innerHTML = `
            <div class="text-center">
                <div class="animate-spin rounded-full h-12 w-12 border-b-2 border-neutral-900 mx-auto mb-4"></div>
                <p class="text-neutral-600 text-sm">${message}</p>
            </div>
        `;

        // Make container relative if not already
        if (container.style.position !== 'relative' && container.style.position !== 'absolute' && container.style.position !== 'fixed') {
            container.style.position = 'relative';
        }

        container.appendChild(overlay);
    }

    /**
     * Show loading state for main content
     */
    showMainContentLoading() {
        const mainContainer = document.getElementById('network-benchmark-container');
        if (mainContainer) {
            this.addLoadingOverlay(mainContainer, 'Initializing network benchmark...');
        }
    }

    /**
     * Hide loading state for main content
     */
    hideMainContentLoading() {
        const mainOverlay = document.querySelector('#network-benchmark-container > .loading-overlay');
        if (mainOverlay) {
            mainOverlay.remove();
        }
    }

    /**
     * Show popup with loading state
     */
    showPopupWithLoading(popupId) {
        const popup = this.popups[popupId];
        if (!popup) return;

        // Show popup
        popup.style.display = 'flex';
        
        // Trigger animation
        requestAnimationFrame(() => {
            popup.style.opacity = '1';
            popup.style.visibility = 'visible';
            popup.querySelector('.popup-content').style.transform = 'scale(1)';
        });

        // Disable popup buttons during loading
        const popupButtons = popup.querySelectorAll('button, input, select');
        popupButtons.forEach(button => {
            button.disabled = true;
            button.classList.add('opacity-50', 'cursor-not-allowed');
        });

        this.currentPopup = popupId;
    }

    /**
     * Enable popup interactions
     */
    enablePopupInteractions(popupId) {
        const popup = this.popups[popupId];
        if (!popup) return;

        // Enable popup buttons
        const popupButtons = popup.querySelectorAll('button, input, select');
        popupButtons.forEach(button => {
            button.disabled = false;
            button.classList.remove('opacity-50', 'cursor-not-allowed');
        });
    }

    /**
     * Popup visibility methods
     */
    showTraceroutePopup() {
        this.showPopupWithLoading('traceroute');
    }

    hideTraceroutePopup() {
        const popup = this.popups.traceroute;
        if (popup) {
            popup.style.opacity = '0';
            popup.style.visibility = 'hidden';
            popup.querySelector('.popup-content').style.transform = 'scale(0.9)';
            setTimeout(() => {
                popup.style.display = 'none';
            }, 300);
        }
        this.currentPopup = null;
    }

    showPingTestPopup() {
        this.showPopupWithLoading('pingTest');
    }

    hidePingTestPopup() {
        const popup = this.popups.pingTest;
        if (popup) {
            popup.style.opacity = '0';
            popup.style.visibility = 'hidden';
            popup.querySelector('.popup-content').style.transform = 'scale(0.9)';
            setTimeout(() => {
                popup.style.display = 'none';
            }, 300);
        }
        this.currentPopup = null;
    }

    showDnsLookupPopup() {
        this.showPopupWithLoading('dnsLookup');
    }

    hideDnsLookupPopup() {
        const popup = this.popups.dnsLookup;
        if (popup) {
            popup.style.opacity = '0';
            popup.style.visibility = 'hidden';
            popup.querySelector('.popup-content').style.transform = 'scale(0.9)';
            setTimeout(() => {
                popup.style.display = 'none';
            }, 300);
        }
        this.currentPopup = null;
    }

    showBandwidthTestPopup() {
        this.showPopupWithLoading('bandwidthTest');
    }

    hideBandwidthTestPopup() {
        const popup = this.popups.bandwidthTest;
        if (popup) {
            popup.style.opacity = '0';
            popup.style.visibility = 'hidden';
            popup.querySelector('.popup-content').style.transform = 'scale(0.9)';
            setTimeout(() => {
                popup.style.display = 'none';
            }, 300);
        }
        this.currentPopup = null;
    }

    hideCurrentPopup() {
        if (this.currentPopup) {
            this.hidePopup(this.currentPopup);
        }
    }

    hidePopup(popupId) {
        switch (popupId) {
            case 'traceroute':
                this.hideTraceroutePopup();
                break;
            case 'pingTest':
                this.hidePingTestPopup();
                break;
            case 'dnsLookup':
                this.hideDnsLookupPopup();
                break;
            case 'bandwidthTest':
                this.hideBandwidthTestPopup();
                break;
        }
    }

    /**
     * Show servers loading state
     */
    showServersLoading() {
        const serverTableBody = document.querySelector('#server-table-body');
        if (serverTableBody) {
            serverTableBody.innerHTML = `
                <tr>
                    <td colspan="6" class="px-6 py-12">
                        <div class="flex items-center justify-center">
                            <div class="text-center">
                                <div class="animate-spin rounded-full h-8 w-8 border-b-2 border-neutral-900 mx-auto mb-4"></div>
                                <p class="text-neutral-600">Loading server data...</p>
                            </div>
                        </div>
                    </td>
                </tr>
            `;
        }

        // Also update summary cards to show loading state
        const summaryCards = ['total-servers', 'online-servers', 'offline-servers', 'avg-ping'];
        summaryCards.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.innerHTML = '<div class="animate-pulse bg-neutral-200 h-8 w-16 rounded"></div>';
            }
        });

        // Update last updated time
        const lastUpdatedElement = document.getElementById('last-updated');
        if (lastUpdatedElement) {
            lastUpdatedElement.textContent = 'Loading...';
        }
    }

    /**
     * Show servers empty state
     */
    showServersEmpty() {
        const serverTableBody = document.querySelector('#server-table-body');
        if (serverTableBody) {
            serverTableBody.innerHTML = `
                <tr>
                    <td colspan="6" class="px-6 py-12">
                        <div class="text-center">
                            <i class="fa-solid fa-server text-4xl text-neutral-300 mb-4"></i>
                            <p class="text-neutral-600">No servers found</p>
                            <p class="text-neutral-500 text-sm mt-2">Try refreshing or adding new servers</p>
                        </div>
                    </td>
                </tr>
            `;
        }

        // Reset summary cards to show empty state
        const summaryCards = ['total-servers', 'online-servers', 'offline-servers', 'avg-ping'];
        const emptyValues = ['0', '0', '0', '--ms'];
        
        summaryCards.forEach((id, index) => {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = emptyValues[index];
            }
        });

        // Update last updated time
        const lastUpdatedElement = document.getElementById('last-updated');
        if (lastUpdatedElement) {
            lastUpdatedElement.textContent = 'No data';
        }
    }

    /**
     * Show statistics loading state
     */
    showStatsLoading() {
        const statsContainer = document.querySelector('#network-benchmark-container .stats-container');
        if (statsContainer) {
            statsContainer.innerHTML = `
                <div class="grid grid-cols-1 md:grid-cols-4 gap-4">
                    ${[1, 2, 3, 4].map(i => `
                        <div class="bg-white rounded-lg border border-neutral-200 p-4">
                            <div class="animate-pulse bg-neutral-200 h-6 w-12 rounded mb-2"></div>
                            <div class="animate-pulse bg-neutral-200 h-8 w-16 rounded"></div>
                        </div>
                    `).join('')}
                </div>
            `;
        }
    }

    /**
     * Show initial loading state
     */
    showInitialLoadingState() {
        this.showLoadingState();
        this.showServersLoading();
        this.showStatsLoading();
    }

    /**
     * Transfer popup content to runtime UI (delegate method)
     */
    transferPopupToRuntime(popupId, runtimeUI) {
        if (!runtimeUI || !this.popups[popupId]) return;
        
        // Signal runtime UI to take over popup content
        if (runtimeUI.initializePopupContent) {
            runtimeUI.initializePopupContent(popupId, this.popups[popupId]);
        }
    }

    /**
     * Destroy the loading UI
     */
    destroy() {
        // Remove all popups
        Object.values(this.popups).forEach(popup => {
            if (popup) {
                popup.remove();
            }
        });

        // Remove loading overlays
        this.hideLoadingOverlays();
        this.hideMainContentLoading();

        // Reset state
        this.currentPopup = null;
        this.isInitialized = false;
        this.isLoading = false;
    }
}

// Initialize the loading UI
window.networkBenchmarkLoadingUI = new NetworkBenchmarkLoadingUI();

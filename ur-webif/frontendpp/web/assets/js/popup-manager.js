/**
 * Advanced Modal Manager
 * Provides sophisticated modal functionality with custom content, promises, and proper UI
 */
class PopupManager {
    constructor() {
        this.activeModal = null;
        this.modalContainer = null;
        this.init();
    }
    
    init() {
        console.log('[POPUP-MANAGER] Initializing advanced modal system');
        this.createModalContainer();
    }
    
    createModalContainer() {
        // Create modal container if it doesn't exist
        if (!this.modalContainer) {
            this.modalContainer = document.createElement('div');
            this.modalContainer.id = 'modal-container';
            this.modalContainer.className = 'fixed inset-0 z-50 flex items-center justify-center hidden';
            document.body.appendChild(this.modalContainer);
        }
    }
    
    /**
     * Show a modal with custom options
     * @param {Object} options - Modal options
     * @returns {Promise} - Promise that resolves with the result
     */
    show(options = {}) {
        return new Promise((resolve) => {
            const modal = this.createModal(options, resolve);
            this.showModal(modal);
        });
    }
    
    /**
     * Show error modal
     */
    showError(options) {
        return this.show({
            icon: 'error',
            title: options.title || 'Error',
            description: options.message || options.description || 'An error occurred',
            buttonText: options.buttonText || 'OK',
            buttonClass: 'bg-red-600 hover:bg-red-700 text-white',
            ...options
        });
    }
    
    /**
     * Show success modal
     */
    showSuccess(options) {
        return this.show({
            icon: 'success',
            title: options.title || 'Success',
            description: options.message || options.description || 'Operation completed successfully',
            buttonText: options.buttonText || 'OK',
            buttonClass: 'bg-green-600 hover:bg-green-700 text-white',
            ...options
        });
    }
    
    /**
     * Show info modal
     */
    showInfo(options) {
        return this.show({
            icon: 'info',
            title: options.title || 'Information',
            description: options.message || options.description,
            buttonText: options.buttonText || 'OK',
            buttonClass: 'bg-blue-600 hover:bg-blue-700 text-white',
            ...options
        });
    }
    
    /**
     * Show custom modal with custom content and buttons
     */
    showCustom(options = {}) {
        return new Promise((resolve) => {
            const modal = this.createCustomModal(options, resolve);
            this.showModal(modal);
        });
    }
    
    createModal(options, resolve) {
        const modal = document.createElement('div');
        modal.className = 'bg-white rounded-2xl shadow-2xl max-w-md w-full mx-4 transform transition-all duration-300 scale-95 opacity-0';
        
        let content = '';
        
        // Create backdrop
        const backdrop = document.createElement('div');
        backdrop.className = 'absolute inset-0 bg-black bg-opacity-50 transition-opacity duration-300';
        if (options.closeOnBackdrop !== false) {
            backdrop.addEventListener('click', () => this.closeModal({ backdrop, modal }, resolve));
        }
        
        // Create modal content
        content = '<div class="relative p-6">';
        
        // Add icon if specified
        if (options.icon && !options.customContent) {
            content += this.createIcon(options.icon);
        }
        
        // Add custom content or title/description
        if (options.customContent) {
            content += '<div class="custom-content">';
            if (typeof options.customContent === 'string') {
                content += options.customContent;
            } else if (options.customContent.outerHTML) {
                content += options.customContent.outerHTML;
            } else if (options.customContent.innerHTML) {
                content += options.customContent.innerHTML;
            } else {
                content += options.customContent;
            }
            content += '</div>';
        } else {
            if (options.title !== false) {
                content += `<h2 class="text-xl font-bold text-gray-900 mb-2">${options.title || 'Modal'}</h2>`;
            }
            if (options.description !== false) {
                content += `<p class="text-gray-600 mb-6">${options.description || ''}</p>`;
            }
        }
        

        
        content += '</div>';
        
        modal.innerHTML = content;
        
        // Add event listeners
        const confirmBtn = modal.querySelector('.confirm-btn');
        const cancelBtn = modal.querySelector('.cancel-btn');
        
        if (confirmBtn) {
            confirmBtn.addEventListener('click', () => this.closeModal({ backdrop, modal }, resolve, 'confirm'));
        }
        
        if (cancelBtn) {
            cancelBtn.addEventListener('click', () => this.closeModal({ backdrop, modal }, resolve, 'cancel'));
        }
        
        // Close on escape
        if (options.closeOnEscape !== false) {
            const handleEscape = (e) => {
                if (e.key === 'Escape') {
                    this.closeModal({ backdrop, modal }, resolve, 'escape');
                    document.removeEventListener('keydown', handleEscape);
                }
            };
            document.addEventListener('keydown', handleEscape);
        }
        
        // Store resolve function for custom content buttons
        modal.resolve = resolve;
        
        return { backdrop, modal };
    }
    
    createCustomModal(options, resolve) {
        // Check if content is a complete modal and adjust container accordingly
        const isCompleteModal = options.content && (
            options.content.includes('settings-modal') || 
            options.content.includes('modal-header') ||
            options.content.includes('authentication-modal')
        );
        
        const modal = document.createElement('div');
        if (isCompleteModal) {
            // Use plain container for complete modal content
            modal.className = 'transform transition-all duration-300 scale-95 opacity-0';
        } else {
            // Use styled container for partial content
            modal.className = 'bg-white rounded-2xl shadow-2xl max-w-2xl w-full mx-4 max-h-[80vh] overflow-y-auto transform transition-all duration-300 scale-95 opacity-0';
        }
        
        // Create backdrop
        const backdrop = document.createElement('div');
        backdrop.className = 'absolute inset-0 bg-black bg-opacity-50 transition-opacity duration-300';
        if (options.closeOnBackdrop !== false) {
            backdrop.addEventListener('click', () => this.closeModal({ backdrop, modal }, resolve));
        }
        
        // Create modal content - check if content is a complete modal or needs wrapping
        let content = '';
        
        // Add custom content
        if (options.content) {
            // Check if content contains a complete modal structure
            if (isCompleteModal) {
                // Content is already a complete modal, don't wrap it
                content = options.content;
            } else {
                // Content needs wrapping
                content = '<div class="relative p-6">';
                content += options.content;
                content += '</div>';
            }
        }
        
        // Add buttons (only if content isn't already a complete modal)
        if (options.buttons && Array.isArray(options.buttons) && !options.content.includes('modal-footer')) {
            content += '<div class="flex justify-end space-x-3 p-6 pt-0">';
            options.buttons.forEach((button, index) => {
                const buttonClass = button.class || 'px-4 py-2 bg-blue-500 text-white rounded hover:bg-blue-600';
                content += `<button class="${buttonClass}" data-action="${button.text || 'button'}" data-index="${index}">${button.text}</button>`;
            });
            content += '</div>';
        }
        
        modal.innerHTML = content;
        
        // Add styling for authentication modal
        if (isCompleteModal && options.content.includes('authentication-modal')) {
            const authModal = modal.querySelector('.authentication-modal');
            if (authModal) {
                authModal.className = 'bg-white rounded-lg shadow-2xl w-[400px] max-w-[90vw]';
            }
        }
        
        // Add event listeners for custom buttons
        const buttons = modal.querySelectorAll('[data-action]');
        buttons.forEach((button, index) => {
            button.addEventListener('click', () => {
                const buttonConfig = options.buttons[index];
                if (buttonConfig && buttonConfig.action) {
                    buttonConfig.action();
                }
                this.closeModal({ backdrop, modal }, resolve, buttonConfig.text);
            });
        });
        
        // Close on escape
        if (options.closeOnEscape !== false) {
            const handleEscape = (e) => {
                if (e.key === 'Escape') {
                    this.closeModal({ backdrop, modal }, resolve, 'escape');
                    document.removeEventListener('keydown', handleEscape);
                }
            };
            document.addEventListener('keydown', handleEscape);
        }
        
        // Store resolve function for custom content buttons
        modal.resolve = resolve;
        
        return { backdrop, modal };
    }
    
    createIcon(type) {
        const icons = {
            success: '<div class="w-12 h-12 bg-green-100 rounded-full flex items-center justify-center mb-4"><i class="fas fa-check text-green-600 text-xl"></i></div>',
            error: '<div class="w-12 h-12 bg-red-100 rounded-full flex items-center justify-center mb-4"><i class="fas fa-exclamation text-red-600 text-xl"></i></div>',
            info: '<div class="w-12 h-12 bg-blue-100 rounded-full flex items-center justify-center mb-4"><i class="fas fa-info text-blue-600 text-xl"></i></div>',
            warning: '<div class="w-12 h-12 bg-yellow-100 rounded-full flex items-center justify-center mb-4"><i class="fas fa-exclamation-triangle text-yellow-600 text-xl"></i></div>'
        };
        
        return icons[type] || icons.info;
    }
    
    showModal(modalElements) {
        const { backdrop, modal } = modalElements;
        
        // Clear previous modal
        this.modalContainer.innerHTML = '';
        
        // Add backdrop and modal to container
        this.modalContainer.appendChild(backdrop);
        this.modalContainer.appendChild(modal);
        
        // Show container
        this.modalContainer.classList.remove('hidden');
        
        // Animate in
        setTimeout(() => {
            modal.classList.remove('scale-95', 'opacity-0');
            modal.classList.add('scale-100', 'opacity-100');
            backdrop.classList.remove('opacity-0');
            backdrop.classList.add('opacity-100');
            
            // Trigger custom content binding after animation
            this.bindCustomContent(modal);
        }, 10);
        
        this.activeModal = modalElements;
    }
    
    /**
     * Bind event listeners for custom content
     */
    bindCustomContent(modal) {
        // Find and bind custom buttons
        const customButtons = modal.querySelectorAll('[data-modal-action]');
        customButtons.forEach(button => {
            const action = button.getAttribute('data-modal-action');
            button.addEventListener('click', () => {
                if (modal.resolve) {
                    modal.resolve(action);
                }
                this.closeModal(this.activeModal, modal.resolve, action);
            });
        });
        
        // Emit event for external binding
        const event = new CustomEvent('modalShown', {
            detail: { modal, container: this.modalContainer }
        });
        document.dispatchEvent(event);
    }
    
    closeModal(modalElements, resolve, result = 'confirm') {
        // Error handling for undefined modalElements
        if (!modalElements || !modalElements.modal || !modalElements.backdrop) {
            console.error('[POPUP-MANAGER] Invalid modalElements passed to closeModal:', modalElements);
            if (resolve) resolve(result);
            return;
        }
        
        const { backdrop, modal } = modalElements;
        
        // Animate out
        modal.classList.remove('scale-100', 'opacity-100');
        modal.classList.add('scale-95', 'opacity-0');
        backdrop.classList.remove('opacity-100');
        backdrop.classList.add('opacity-0');
        
        // Hide container after animation
        setTimeout(() => {
            this.modalContainer.classList.add('hidden');
            this.modalContainer.innerHTML = '';
            this.activeModal = null;
        }, 300);
        
        // Resolve promise
        if (resolve) {
            resolve(result);
        }
    }
    
    /**
     * Close current modal
     */
    close() {
        if (this.activeModal) {
            this.closeModal(this.activeModal, null, 'close');
        }
    }
}

// Create global instance
window.popupManager = new PopupManager();

export { PopupManager };
export default PopupManager;

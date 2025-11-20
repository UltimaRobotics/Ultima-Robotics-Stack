/**
 * Login File Upload Handler
 * Manages authentication file upload, validation, and drag-drop functionality
 */
class LoginFileHandler {
    constructor(fileUploadArea, authFileInput, fileInfo) {
        this.fileUploadArea = fileUploadArea;
        this.authFileInput = authFileInput;
        this.fileInfo = fileInfo;
        this.init();
    }

    init() {
        this.bindEvents();
        console.log('[LOGIN-FILE] File handler initialized');
    }

    bindEvents() {
        // File input change
        this.authFileInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            this.handleFileUpload(file);
        });

        // Click to upload
        this.fileUploadArea.addEventListener('click', () => {
            this.authFileInput.click();
        });

        // Drag and drop
        this.fileUploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            this.fileUploadArea.classList.add('bg-gray-100');
        });

        this.fileUploadArea.addEventListener('dragleave', (e) => {
            e.preventDefault();
            this.fileUploadArea.classList.remove('bg-gray-100');
        });

        this.fileUploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            this.fileUploadArea.classList.remove('bg-gray-100');
            
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                this.authFileInput.files = files;
                const file = files[0];
                this.handleFileUpload(file);
            }
        });
    }

    handleFileUpload(file) {
        if (!file) {
            this.resetFileUpload();
            return;
        }

        // Validate file extension
        const validExtensions = ['.uacc'];
        const fileName = file.name.toLowerCase();
        const isValidExtension = validExtensions.some(ext => fileName.endsWith(ext));

        if (isValidExtension) {
            this.showValidFile(file);
            console.log('[LOGIN-FILE] Valid UACC file uploaded:', file.name, file.size);
        } else {
            this.showInvalidFile(file);
            console.log('[LOGIN-FILE] Invalid file uploaded:', file.name, 'Extension not supported');
            
            // Clear the invalid file after a delay
            setTimeout(() => {
                this.authFileInput.value = '';
                this.resetFileUpload();
            }, 3000);
        }
    }

    showValidFile(file) {
        this.fileInfo.innerHTML = `
            <div class="flex items-center space-x-2 text-green-600">
                <i class="fas fa-check-circle"></i>
                <span class="text-sm font-medium">Valid file: ${file.name} (${this.formatFileSize(file.size)})</span>
            </div>
        `;
        this.fileUploadArea.classList.add('border-green-500', 'bg-green-50');
        this.fileUploadArea.classList.remove('border-gray-300', 'border-red-500', 'bg-red-50');
    }

    showInvalidFile(file) {
        this.fileInfo.innerHTML = `
            <div class="flex items-center space-x-2 text-red-600">
                <i class="fas fa-exclamation-circle"></i>
                <span class="text-sm font-medium">Invalid file: ${file.name}. Only .uacc files are allowed.</span>
            </div>
        `;
        this.fileUploadArea.classList.add('border-red-500', 'bg-red-50');
        this.fileUploadArea.classList.remove('border-gray-300', 'border-green-500', 'bg-green-50');
    }

    resetFileUpload() {
        this.fileInfo.innerHTML = '';
        this.fileUploadArea.classList.remove('border-green-500', 'bg-green-50', 'border-red-500', 'bg-red-50');
        this.fileUploadArea.classList.add('border-gray-300', 'bg-gray-50');
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    readFileAsText(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => resolve(e.target.result);
            reader.onerror = (e) => reject(e);
            reader.readAsText(file);
        });
    }
}

// Export for use in other modules
export { LoginFileHandler };
export default LoginFileHandler;

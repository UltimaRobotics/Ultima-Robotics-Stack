# Backup & Restore Module

This module provides comprehensive backup and restore functionality for the Ultima Robotics web interface.

## Features

- **Full and Partial Backups**: Create complete system backups or select specific components
- **Encrypted Backups**: AES-256 encrypted backup support with password protection
- **Drag & Drop Upload**: Intuitive file upload interface for restore operations
- **Progress Monitoring**: Real-time progress tracking for backup and restore operations
- **Backup History**: View and manage recent backups with download/restore/delete options
- **File Validation**: Integrity checking for backup files before restore operations
- **Loading States**: Skeleton screens and loading indicators for better UX

## Module Structure

```
backup-restore/
├── index.js                    # Module entry point and initialization
├── backup-restore.js           # Core business logic and API management
├── backup-restore-ui.js        # UI controller and event handling
├── backup-restore-data-loading.js  # Loading states and skeleton screens
└── README.md                   # This documentation
```

## Usage

### Initialization

```javascript
import { initializeBackupRestore } from './backup-restore/index.js';

// Initialize the module
const backupRestoreComponents = await initializeBackupRestore(sourceManager);

// Access components
const { manager, ui } = backupRestoreComponents;
```

### Creating Backups

```javascript
// Standard backup
const backup = await manager.createBackup({
    fullBackup: true,
    includeNetworkConfig: true,
    includeLicense: true,
    includeAuth: true
});

// Encrypted backup
const encryptedBackup = await manager.createEncryptedBackup({
    password: 'strong-password',
    fullBackup: true
});
```

### Restoring from Backup

```javascript
// File validation and restore
const file = document.getElementById('backup-file-input').files[0];
const validation = await manager.validateBackupFile(file);

if (validation.isValid) {
    const restore = await manager.restoreFromBackup(file, {
        validateIntegrity: true
    });
}
```

### Managing Backup History

```javascript
// Get backup history
const history = await manager.getBackupHistory();

// Download backup
const download = await manager.downloadBackup(backupId);

// Delete backup
await manager.deleteBackup(backupId);
```

## API Endpoints

The module expects the following API endpoints (mocked in development):

- `GET /api/v1/backups` - Get backup history
- `POST /api/v1/backups` - Create new backup
- `POST /api/v1/backups/encrypted` - Create encrypted backup
- `POST /api/v1/backups/restore` - Restore from backup
- `GET /api/v1/backups/:id/download` - Download backup file
- `DELETE /api/v1/backups/:id` - Delete backup
- `POST /api/v1/backups/estimate` - Estimate backup size
- `POST /api/v1/backups/validate` - Validate backup file

## File Formats

Supported backup file formats:
- `.uhb` - Standard Ultima Robotics Backup format
- `.uhb.enc` - Encrypted backup format
- `.tar.gz` - Compressed tar archive
- `.bin` - Binary backup format

## Security Features

- **AES-256-CBC Encryption**: For encrypted backup files
- **PBKDF2 Key Derivation**: Secure password-based key generation
- **File Integrity Validation**: Checksum verification
- **Password Strength Indicators**: Real-time password feedback

## Events

The module emits the following custom events:

- `backupRestoreInitialized` - Module initialization complete
- `backupProgress` - Backup creation progress updates
- `restoreProgress` - Restore operation progress updates
- `backupRestoreError` - Error events

## Dependencies

- `http-client` - HTTP communication
- `jwt-token-manager` - Authentication token management

## Browser Compatibility

- Modern browsers with ES6 module support
- Drag & Drop API support
- File API support
- CSS Grid and Flexbox

## Development Notes

- Mock data is used for development and testing
- API endpoints return simulated responses
- Progress animations use CSS transitions
- Loading states provide visual feedback during operations


# Security Best Practices for ur-licence-mann

## Runtime Security Considerations

### 1. Key Management
**Critical**: Never store keys in version control or plaintext files in production.

**Recommended Approaches:**
- Use environment variables for keys in production
- Use Replit Secrets for secure key storage
- Consider hardware security modules (HSM) for production deployments
- Implement key rotation policies

### 2. File Permissions
All sensitive files have restricted permissions (0600):
- `encryption_keys.json` - Owner read/write only
- `private_key.pem` - Owner read/write only

### 3. Environment-Based Configuration
```bash
# Set these in Replit Secrets or environment
export MASTER_ENCRYPTION_KEY="your-secure-key-here"
export PRIVATE_KEY_PATH="/secure/path/to/private.pem"
```

### 4. Path Security
- All user-provided paths are validated against directory traversal
- Use `PathValidator::validate_path()` for any filesystem operations
- Never trust user input for file paths

### 5. Production Deployment Checklist
- [ ] Move all keys to Replit Secrets
- [ ] Enable key rotation
- [ ] Set up monitoring for unauthorized access attempts
- [ ] Use HTTPS for all network communications
- [ ] Regularly update dependencies
- [ ] Implement rate limiting for license verification

### 6. Encryption Keys at Rest
Current implementation stores keys in JSON. For production:
1. Encrypt the encryption_keys.json file itself
2. Use a key derivation function (KDF) from a passphrase
3. Store only derived keys, never master passwords

## Migration to Secure Storage

To use Replit Secrets instead of file-based keys:

1. Add keys to Secrets tool in Replit
2. Modify code to read from environment:
```cpp
const char* key_env = std::getenv("MASTER_ENCRYPTION_KEY");
if (key_env) {
    config.master_encryption_key = std::string(key_env);
}
```

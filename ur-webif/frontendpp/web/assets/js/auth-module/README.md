# WebModules JWT Authentication Module

A generic JWT-based authentication module in JavaScript with shared session storage support, allowing multiple scripts to use the same runtime session data.

## Features

- **JWT Authentication**: Complete JWT token creation, validation, and management
- **Shared Session Storage**: Multiple scripts can share the same authentication session
- **Configuration Management**: Flexible configuration for different environments (dev, prod, test)
- **Token Refresh**: Automatic and manual token refresh capabilities
- **Session Tracking**: Comprehensive session status tracking and management
- **Security Features**: Token validation, expiration handling, blacklist support
- **ES Modules**: Modern JavaScript with ES module support
- **Comprehensive Utils**: JWT utility functions for token manipulation

## Project Structure

```
auth-module/
├── lib/
│   ├── jwt-config.js       # JWT configuration management
│   ├── auth-storage.js     # Shared session storage
│   └── jwt-utils.js        # JWT token utilities
├── src/
│   └── JwtAuthManager.js   # Main JWT auth manager
├── examples/
│   ├── basic-usage.js                    # Basic usage examples
│   ├── shared-session-script1.js         # Script 1 for shared session demo
│   ├── shared-session-script2.js         # Script 2 for shared session demo
│   └── run-shared-session-example.js     # Runner for shared session demo
├── tests/
│   └── jwt-auth-manager.test.js          # Comprehensive test suite
├── package.json
└── README.md
```

## Installation

```bash
cd auth-module
npm install
```

## Basic Usage

### Simple JWT Authentication

```javascript
import { JwtAuthManager } from './src/JwtAuthManager.js';

// Create auth manager with default configuration
const authManager = new JwtAuthManager();
await authManager.initialize();

// Authenticate user
const authResult = await authManager.authenticate({
  username: 'demo',
  password: 'demo'
});

console.log('User authenticated:', authResult.user);
console.log('Access token:', authResult.tokens.accessToken);

// Check authentication status
if (authManager.isAuthenticated()) {
  const user = authManager.getCurrentUser();
  console.log('Current user:', user);
}

// Add authorization header to API requests
const requestOptions = authManager.addAuthorizationHeader({
  method: 'GET',
  url: '/api/protected-endpoint'
});

// Make authenticated request
const response = await fetch('/api/protected-endpoint', requestOptions);
```

### Advanced Configuration

```javascript
import { JwtAuthManager } from './src/JwtAuthManager.js';
import { JwtConfig } from './lib/jwt-config.js';

const config = new JwtConfig({
  secretKey: 'your-secret-key',
  accessTokenExpiry: '15m',
  refreshTokenExpiry: '7d',
  autoRefresh: true,
  issuer: 'your-app',
  audience: 'your-users'
})
.setSecurity({
  blacklist: true,
  rateLimit: true
})
.setEndpoints({
  login: '/api/v1/auth/login',
  refresh: '/api/v1/auth/refresh'
});

const authManager = new JwtAuthManager(config);
await authManager.initialize();
```

## Shared Session Usage

The key feature of this authentication module is the ability to share sessions across multiple scripts:

### Script 1

```javascript
// script1.js
import { JwtAuthManager } from './src/JwtAuthManager.js';

const sharedAuthManager = JwtAuthManager.getSharedAuthManager({
  secretKey: 'shared-secret',
  storageKey: 'my-app-session',
  accessTokenExpiry: '30m',
  autoRefresh: true
});

await sharedAuthManager.initialize();

// Authenticate user
await sharedAuthManager.authenticate({
  username: 'user1',
  password: 'password1'
});

console.log('Script 1: User authenticated');
console.log('Script 1: Session active for other scripts');
```

### Script 2

```javascript
// script2.js
import { JwtAuthManager } from './src/JwtAuthManager.js';

// Get the same shared auth manager (identical configuration)
const sharedAuthManager = JwtAuthManager.getSharedAuthManager({
  secretKey: 'shared-secret',
  storageKey: 'my-app-session',
  accessTokenExpiry: '30m',
  autoRefresh: true
});

await sharedAuthManager.initialize();

// Use the same authentication session
if (sharedAuthManager.isAuthenticated()) {
  const user = sharedAuthManager.getCurrentUser();
  console.log('Script 2: Same user session:', user.username);
  
  // Make authenticated requests
  const requestOptions = sharedAuthManager.addAuthorizationHeader({
    method: 'GET',
    url: '/api/user-data'
  });
}
```

Both scripts will use the same authentication session, sharing tokens and user data.

## API Reference

### JwtAuthManager

#### Constructor

```javascript
new JwtAuthManager(config|JwtConfig)
```

#### Methods

- `initialize()` - Initialize the auth manager
- `authenticate(credentials, options)` - Authenticate user with credentials
- `getCurrentSession()` - Get current session data
- `isAuthenticated()` - Check if user is authenticated
- `getCurrentUser()` - Get current user data
- `getAccessToken()` - Get current access token
- `getRefreshToken()` - Get current refresh token
- `validateToken(token)` - Validate JWT token
- `refreshTokens(refreshToken)` - Refresh access token
- `logout()` - Logout and clear session
- `addAuthorizationHeader(requestOptions)` - Add auth header to requests
- `getSessionStatus()` - Get detailed session status
- `updateConfig(newConfig)` - Update configuration
- `destroy()` - Destroy auth manager and cleanup

#### Static Methods

- `JwtAuthManager.getSharedAuthManager(config)` - Get or create shared auth manager
- `JwtAuthManager.getStorageStats()` - Get storage statistics
- `JwtAuthManager.cleanupExpiredSessions()` - Cleanup expired sessions

### JwtConfig

#### Static Methods

- `JwtConfig.forDevelopment(options)` - Create development configuration
- `JwtConfig.forProduction(options)` - Create production configuration
- `JwtConfig.forTesting(options)` - Create testing configuration

#### Methods

- `setSecretKey(secretKey)` - Set JWT secret key
- `setTokenExpiry(accessExpiry, refreshExpiry)` - Set token expiration times
- `setSessionTimeout(timeoutMs)` - Set session timeout
- `setAutoRefresh(enabled)` - Enable/disable auto refresh
- `setRefreshConfig(options)` - Configure refresh behavior
- `setEndpoints(endpoints)` - Set authentication endpoints
- `setHeaders(headers)` - Set custom headers
- `setSecurity(options)` - Configure security features
- `setValidation(options)` - Configure validation options
- `clone()` - Clone configuration
- `validate()` - Validate configuration
- `toObject()` - Convert to plain object

### JwtUtils

#### Static Methods

- `JwtUtils.createToken(payload, secret, options)` - Create JWT token
- `JwtUtils.verifyToken(token, secret, options)` - Verify JWT token
- `JwtUtils.decodeToken(token)` - Decode token without verification
- `JwtUtils.isTokenExpired(token, clockSkew)` - Check if token is expired
- `JwtUtils.willTokenExpireWithin(token, withinSeconds)` - Check expiration window
- `JwtUtils.getTokenRemainingTime(token)` - Get remaining time in seconds
- `JwtUtils.refreshToken(token, secret, options)` - Refresh JWT token
- `JwtUtils.createTokenPair(payload, secret, options)` - Create access/refresh token pair
- `JwtUtils.extractTokenFromHeader(authHeader)` - Extract token from Authorization header
- `JwtUtils.generateJwtId()` - Generate random JWT ID
- `JwtUtils.isValidTokenFormat(token)` - Validate token format
- `JwtUtils.getTokenInfo(token)` - Get token information

## Configuration Options

### JWT Configuration

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `secretKey` | string | 'default-secret-key' | JWT secret key |
| `algorithm` | string | 'HS256' | JWT signing algorithm |
| `issuer` | string | 'webmodules-auth' | JWT issuer |
| `audience` | string | 'webmodules-client' | JWT audience |
| `accessTokenExpiry` | string | '15m' | Access token expiration |
| `refreshTokenExpiry` | string | '7d' | Refresh token expiration |
| `sessionTimeout` | number | 3600000 | Session timeout in ms |
| `autoRefresh` | boolean | true | Enable automatic token refresh |

### Security Configuration

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `blacklistEnabled` | boolean | false | Enable token blacklist |
| `rateLimitEnabled` | boolean | false | Enable rate limiting |
| `maxLoginAttempts` | number | 5 | Maximum login attempts |
| `lockoutDuration` | number | 900000 | Account lockout duration |
| `validateIssuer` | boolean | true | Validate token issuer |
| `validateAudience` | boolean | true | Validate token audience |
| `validateExpiration` | boolean | true | Validate token expiration |

## Environment-Specific Configurations

### Development

```javascript
const devConfig = JwtConfig.forDevelopment({
  secretKey: 'dev-secret-key',
  accessTokenExpiry: '1h',
  refreshTokenExpiry: '1d'
});
```

### Production

```javascript
const prodConfig = JwtConfig.forProduction({
  secretKey: process.env.JWT_SECRET,
  issuer: 'my-production-app',
  audience: 'production-users'
})
.setSecurity({
  blacklist: true,
  rateLimit: true
});
```

### Testing

```javascript
const testConfig = JwtConfig.forTesting({
  secretKey: 'test-secret-key',
  accessTokenExpiry: '60s',
  refreshTokenExpiry: '5m',
  autoRefresh: false
});
```

## Running Examples

### Basic Usage Example

```bash
node examples/basic-usage.js
```

### Shared Session Example

```bash
# Run scripts sequentially
node examples/run-shared-session-example.js sequential

# Run scripts concurrently
node examples/run-shared-session-example.js concurrent
```

## Running Tests

```bash
npm test
```

## Token Management

### Creating Tokens

```javascript
import { JwtUtils } from './lib/jwt-utils.js';

const payload = {
  sub: 'user-id',
  username: 'johndoe',
  email: 'john@example.com',
  roles: ['user', 'admin']
};

const token = JwtUtils.createToken(payload, secretKey, {
  expiresIn: '1h',
  issuer: 'my-app',
  audience: 'my-users'
});
```

### Validating Tokens

```javascript
try {
  const payload = JwtUtils.verifyToken(token, secretKey, {
    issuer: 'my-app',
    audience: 'my-users'
  });
  console.log('Token valid for user:', payload.username);
} catch (error) {
  console.log('Token invalid:', error.message);
}
```

### Refreshing Tokens

```javascript
const refreshedToken = JwtUtils.refreshToken(oldToken, secretKey, {
  expiresIn: '2h'
});
```

### Creating Token Pairs

```javascript
const tokenPair = JwtUtils.createTokenPair(payload, secretKey, {
  accessTokenExpiry: '15m',
  refreshTokenExpiry: '7d'
});

console.log('Access token:', tokenPair.accessToken);
console.log('Refresh token:', tokenPair.refreshToken);
```

## Session Management

### Session Status

```javascript
const sessionStatus = authManager.getSessionStatus();
console.log('Authenticated:', sessionStatus.authenticated);
console.log('User:', sessionStatus.user);
console.log('Expires at:', sessionStatus.expiresAt);
console.log('Token remaining time:', sessionStatus.tokenInfo.remainingTime);
```

### Storage Statistics

```javascript
const stats = JwtAuthManager.getStorageStats();
console.log('Total sessions:', stats.totalSessions);
console.log('Active sessions:', stats.activeCount);
console.log('Expired sessions:', stats.expiredCount);
```

### Cleanup Operations

```javascript
// Clean up expired sessions
const expiredCleaned = JwtAuthManager.cleanupExpiredSessions();

// Clean up inactive sessions (older than 1 hour)
const inactiveCleaned = JwtAuthManager.cleanupInactiveSessions(60 * 60 * 1000);
```

## Error Handling

```javascript
try {
  await authManager.authenticate(credentials);
} catch (error) {
  if (error.message.includes('Authentication failed')) {
    console.log('Invalid credentials');
  } else if (error.message.includes('must be initialized')) {
    console.log('Auth manager not initialized');
  } else {
    console.log('Authentication error:', error.message);
  }
}

// Token validation error handling
const payload = authManager.validateToken();
if (!payload) {
  console.log('Token is invalid or expired');
  // Attempt refresh or logout
  try {
    await authManager.refreshTokens();
  } catch (refreshError) {
    authManager.logout();
  }
}
```

## Security Best Practices

1. **Use strong secret keys**: At least 32 characters for production
2. **Set appropriate token expiration**: Short-lived access tokens (15m), longer refresh tokens (7d)
3. **Enable validation**: Always validate issuer and audience in production
4. **Use HTTPS**: Always transmit tokens over secure connections
5. **Implement logout**: Properly clear sessions and tokens on logout
6. **Monitor sessions**: Regular cleanup of expired and inactive sessions
7. **Rate limiting**: Enable rate limiting to prevent brute force attacks

## License

MIT License

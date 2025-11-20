import { JwtAuthManager } from '../src/JwtAuthManager.js';
import { JwtConfig } from '../lib/jwt-config.js';

/**
 * Basic JWT Authentication Usage Example
 * Demonstrates how to use the JWT auth manager for authentication
 */

async function basicAuthenticationExample() {
  console.log('=== Basic JWT Authentication Example ===\n');

  try {
    // Example 1: Create auth manager with default configuration
    console.log('1. Creating JWT auth manager...');
    const authManager = new JwtAuthManager();
    await authManager.initialize();

    // Example 2: Authenticate user
    console.log('2. Authenticating user...');
    const authResult = await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    console.log('Authentication successful!');
    console.log('User:', authResult.user);
    console.log('Token type:', authResult.tokens.tokenType);
    console.log('Session key:', authResult.sessionKey);

    // Example 3: Check authentication status
    console.log('\n3. Checking authentication status...');
    console.log('Is authenticated:', authManager.isAuthenticated());
    console.log('Current user:', authManager.getCurrentUser());

    // Example 4: Get and validate tokens
    console.log('\n4. Working with tokens...');
    const accessToken = authManager.getAccessToken();
    const refreshToken = authManager.getRefreshToken();
    
    console.log('Access token exists:', !!accessToken);
    console.log('Refresh token exists:', !!refreshToken);

    // Validate access token
    const tokenPayload = authManager.validateToken();
    if (tokenPayload) {
      console.log('Token is valid for user:', tokenPayload.username);
      console.log('Token roles:', tokenPayload.roles);
    }

    // Example 5: Add authorization header to request
    console.log('\n5. Adding authorization header...');
    const requestOptions = authManager.addAuthorizationHeader({
      method: 'GET',
      url: 'https://api.example.com/protected'
    });
    
    console.log('Authorization header:', requestOptions.headers.Authorization);

    // Example 6: Get session status
    console.log('\n6. Session status...');
    const sessionStatus = authManager.getSessionStatus();
    console.log('Session status:', sessionStatus.status);
    console.log('Session expires at:', sessionStatus.expiresAt);
    console.log('Token remaining time:', sessionStatus.tokenInfo.remainingTime, 'seconds');

    // Example 7: Refresh tokens
    console.log('\n7. Refreshing tokens...');
    const refreshResult = await authManager.refreshTokens();
    console.log('Tokens refreshed successfully');
    console.log('New access token length:', refreshResult.accessToken.length);

    // Example 8: Logout
    console.log('\n8. Logging out...');
    authManager.logout();
    console.log('Is authenticated after logout:', authManager.isAuthenticated());

    console.log('\n=== Basic authentication example completed successfully ===');

  } catch (error) {
    console.error('Error in basic authentication example:', error.message);
  }
}

async function configurationExample() {
  console.log('\n=== Configuration Example ===\n');

  try {
    // Example 1: Development configuration
    console.log('1. Creating auth manager with development config...');
    const devConfig = JwtConfig.forDevelopment()
      .setSecretKey('dev-secret-key')
      .setTokenExpiry('1h', '1d')
      .setAutoRefresh(true);

    const devAuthManager = new JwtAuthManager(devConfig);
    await devAuthManager.initialize();

    console.log('Development config created');
    console.log('Access token expiry:', devConfig.accessTokenExpiry);
    console.log('Auto refresh enabled:', devConfig.autoRefresh);

    // Example 2: Production configuration
    console.log('\n2. Creating auth manager with production config...');
    try {
      const prodConfig = JwtConfig.forProduction({
        secretKey: 'prod-secret-key-change-in-production',
        issuer: 'my-app',
        audience: 'my-users'
      })
      .setSecurity({
        blacklist: true,
        rateLimit: true
      })
      .setEndpoints({
        login: '/api/v1/auth/login',
        refresh: '/api/v1/auth/refresh'
      });

      const prodAuthManager = new JwtAuthManager(prodConfig);
      await prodAuthManager.initialize();

      console.log('Production config created');
      console.log('Security features enabled:', {
        blacklist: prodConfig.blacklistEnabled,
        rateLimit: prodConfig.rateLimitEnabled
      });
    } catch (error) {
      console.log('Production config error (expected):', error.message);
    }

    // Example 3: Testing configuration
    console.log('\n3. Creating auth manager with testing config...');
    const testConfig = JwtConfig.forTesting()
      .setTokenExpiry('30s', '2m')
      .setAutoRefresh(false);

    const testAuthManager = new JwtAuthManager(testConfig);
    await testAuthManager.initialize();

    console.log('Testing config created');
    console.log('Access token expiry:', testConfig.accessTokenExpiry);
    console.log('Auto refresh disabled:', !testConfig.autoRefresh);

    console.log('\n=== Configuration example completed ===');

  } catch (error) {
    console.error('Error in configuration example:', error.message);
  }
}

async function tokenUtilityExample() {
  console.log('\n=== Token Utility Example ===\n');

  try {
    const { JwtUtils } = await import('../lib/jwt-utils.js');
    const secretKey = 'example-secret';

    // Example 1: Create and verify tokens
    console.log('1. Creating and verifying tokens...');
    const payload = {
      sub: '1234567890',
      username: 'johndoe',
      email: 'john@example.com',
      roles: ['user', 'admin']
    };

    const token = JwtUtils.createToken(payload, secretKey, {
      expiresIn: '1h',
      issuer: 'my-app',
      audience: 'my-users'
    });

    console.log('Token created:', token.substring(0, 50) + '...');

    const verifiedPayload = JwtUtils.verifyToken(token, secretKey, {
      issuer: 'my-app',
      audience: 'my-users'
    });

    console.log('Token verified for user:', verifiedPayload.username);

    // Example 2: Token information
    console.log('\n2. Getting token information...');
    const tokenInfo = JwtUtils.getTokenInfo(token);
    console.log('Is expired:', tokenInfo.isExpired);
    console.log('Expires at:', tokenInfo.expiresAt);
    console.log('Remaining time:', tokenInfo.remainingTime, 'seconds');

    // Example 3: Token refresh
    console.log('\n3. Refreshing token...');
    const refreshedToken = JwtUtils.refreshToken(token, secretKey, {
      expiresIn: '2h'
    });

    const refreshedInfo = JwtUtils.getTokenInfo(refreshedToken);
    console.log('Original token remaining time:', tokenInfo.remainingTime, 'seconds');
    console.log('Refreshed token remaining time:', refreshedInfo.remainingTime, 'seconds');

    // Example 4: Token pair creation
    console.log('\n4. Creating token pair...');
    const tokenPair = JwtUtils.createTokenPair(payload, secretKey, {
      accessTokenExpiry: '15m',
      refreshTokenExpiry: '7d'
    });

    console.log('Access token length:', tokenPair.accessToken.length);
    console.log('Refresh token length:', tokenPair.refreshToken.length);
    console.log('Token type:', tokenPair.tokenType);
    console.log('Expires in:', tokenPair.expiresIn, 'seconds');

    // Example 5: Extract token from header
    console.log('\n5. Extracting token from authorization header...');
    const authHeader = 'Bearer ' + token;
    const extractedToken = JwtUtils.extractTokenFromHeader(authHeader);
    console.log('Token extracted successfully:', !!extractedToken);

    console.log('\n=== Token utility example completed ===');

  } catch (error) {
    console.error('Error in token utility example:', error.message);
  }
}

// Run all examples
async function runAllExamples() {
  await basicAuthenticationExample();
  await configurationExample();
  await tokenUtilityExample();
}

// Execute examples
runAllExamples().catch(console.error);

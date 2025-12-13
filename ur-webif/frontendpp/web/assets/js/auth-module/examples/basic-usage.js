import { JwtAuthManager } from '../src/JwtAuthManager.js';
import { JwtConfig } from '../lib/jwt-config.js';

/**
 * Basic JWT Authentication Usage Example
 * Demonstrates how to use the JWT auth manager for authentication
 */

async function basicAuthenticationExample() {

  try {
    // Example 1: Create auth manager with default configuration
    const authManager = new JwtAuthManager();
    await authManager.initialize();

    // Example 2: Authenticate user
    const authResult = await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });


    // Example 3: Check authentication status

    // Example 4: Get and validate tokens
    const accessToken = authManager.getAccessToken();
    const refreshToken = authManager.getRefreshToken();
    

    // Validate access token
    const tokenPayload = authManager.validateToken();
    if (tokenPayload) {
    }

    // Example 5: Add authorization header to request
    const requestOptions = authManager.addAuthorizationHeader({
      method: 'GET',
      url: 'https://api.example.com/protected'
    });
    

    // Example 6: Get session status
    const sessionStatus = authManager.getSessionStatus();

    // Example 7: Refresh tokens
    const refreshResult = await authManager.refreshTokens();

    // Example 8: Logout
    authManager.logout();


  } catch (error) {
    console.error('Error in basic authentication example:', error.message);
  }
}

async function configurationExample() {

  try {
    // Example 1: Development configuration
    const devConfig = JwtConfig.forDevelopment()
      .setSecretKey('dev-secret-key')
      .setTokenExpiry('1h', '1d')
      .setAutoRefresh(true);

    const devAuthManager = new JwtAuthManager(devConfig);
    await devAuthManager.initialize();


    // Example 2: Production configuration
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

        blacklist: prodConfig.blacklistEnabled,
        rateLimit: prodConfig.rateLimitEnabled
      });
    } catch (error) {
    }

    // Example 3: Testing configuration
    const testConfig = JwtConfig.forTesting()
      .setTokenExpiry('30s', '2m')
      .setAutoRefresh(false);

    const testAuthManager = new JwtAuthManager(testConfig);
    await testAuthManager.initialize();



  } catch (error) {
    console.error('Error in configuration example:', error.message);
  }
}

async function tokenUtilityExample() {

  try {
    const { JwtUtils } = await import('../lib/jwt-utils.js');
    const secretKey = 'example-secret';

    // Example 1: Create and verify tokens
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


    const verifiedPayload = JwtUtils.verifyToken(token, secretKey, {
      issuer: 'my-app',
      audience: 'my-users'
    });


    // Example 2: Token information
    const tokenInfo = JwtUtils.getTokenInfo(token);

    // Example 3: Token refresh
    const refreshedToken = JwtUtils.refreshToken(token, secretKey, {
      expiresIn: '2h'
    });

    const refreshedInfo = JwtUtils.getTokenInfo(refreshedToken);

    // Example 4: Token pair creation
    const tokenPair = JwtUtils.createTokenPair(payload, secretKey, {
      accessTokenExpiry: '15m',
      refreshTokenExpiry: '7d'
    });


    // Example 5: Extract token from header
    const authHeader = 'Bearer ' + token;
    const extractedToken = JwtUtils.extractTokenFromHeader(authHeader);


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

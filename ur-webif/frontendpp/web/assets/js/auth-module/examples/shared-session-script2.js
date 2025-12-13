import { JwtAuthManager } from '../src/JwtAuthManager.js';

/**
 * Script 2: Second script using the same shared JWT authentication session
 * Demonstrates how multiple scripts can share the same authentication session
 */

async function script2() {

  try {
    // Wait a moment to ensure Script 1 has created the shared session
    await new Promise(resolve => setTimeout(resolve, 1000));

    // Get the same shared auth manager with identical configuration
    const sharedAuthManager = JwtAuthManager.getSharedAuthManager({
      secretKey: 'shared-secret-key',
      storageKey: 'shared-jwt-session',
      accessTokenExpiry: '30m',
      refreshTokenExpiry: '1h',
      autoRefresh: true
    });

    await sharedAuthManager.initialize();

    // Check if we can access the existing session
    const isAuth = sharedAuthManager.isAuthenticated();

    if (isAuth) {
      const currentUser = sharedAuthManager.getCurrentUser();
      
      // Get the same tokens that were created by Script 1
      const accessToken = sharedAuthManager.getAccessToken();
      const refreshToken = sharedAuthManager.getRefreshToken();
      
    }

    // Validate the shared token
    const tokenPayload = sharedAuthManager.validateToken();
    if (tokenPayload) {
    }

    // Use the shared session for authenticated operations
    
    if (sharedAuthManager.isAuthenticated()) {
      // Simulate different authenticated operations
      const operations = [
        { name: 'Get user data', endpoint: '/api/user/profile', method: 'GET' },
        { name: 'Update user settings', endpoint: '/api/user/settings', method: 'PUT' },
        { name: 'Fetch user notifications', endpoint: '/api/notifications', method: 'GET' },
        { name: 'Post user activity', endpoint: '/api/activity', method: 'POST' }
      ];

      for (const operation of operations) {
        const requestOptions = sharedAuthManager.addAuthorizationHeader({
          method: operation.method,
          url: operation.endpoint
        });

        
        // Simulate processing time
        await new Promise(resolve => setTimeout(resolve, 400));
      }
    }

    // Check session status
    const sessionStatus = sharedAuthManager.getSessionStatus();

    // Test token refresh
    if (sharedAuthManager.shouldRefreshToken()) {
      const refreshResult = await sharedAuthManager.refreshTokens();
    } else {
    }

    // Show final storage statistics
    const stats = JwtAuthManager.getStorageStats();
      key: s.key,
      status: s.status,
      expired: s.expired
    })));


  } catch (error) {
    console.error('Script 2 error:', error.message);
  }
}

// Execute script 2
script2().catch(console.error);

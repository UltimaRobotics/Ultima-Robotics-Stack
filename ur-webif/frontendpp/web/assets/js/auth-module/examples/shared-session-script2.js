import { JwtAuthManager } from '../src/JwtAuthManager.js';

/**
 * Script 2: Second script using the same shared JWT authentication session
 * Demonstrates how multiple scripts can share the same authentication session
 */

async function script2() {
  console.log('\n=== Script 2: Shared JWT Authentication Session ===\n');

  try {
    // Wait a moment to ensure Script 1 has created the shared session
    await new Promise(resolve => setTimeout(resolve, 1000));

    // Get the same shared auth manager with identical configuration
    console.log('1. Script 2: Getting shared JWT auth manager...');
    const sharedAuthManager = JwtAuthManager.getSharedAuthManager({
      secretKey: 'shared-secret-key',
      storageKey: 'shared-jwt-session',
      accessTokenExpiry: '30m',
      refreshTokenExpiry: '1h',
      autoRefresh: true
    });

    await sharedAuthManager.initialize();

    // Check if we can access the existing session
    console.log('2. Script 2: Checking existing session...');
    const isAuth = sharedAuthManager.isAuthenticated();
    console.log('Script 2: Is authenticated (from shared session):', isAuth);

    if (isAuth) {
      const currentUser = sharedAuthManager.getCurrentUser();
      console.log('Script 2: Current user (from shared session):', currentUser?.username);
      
      // Get the same tokens that were created by Script 1
      const accessToken = sharedAuthManager.getAccessToken();
      const refreshToken = sharedAuthManager.getRefreshToken();
      
      console.log('Script 2: Access token exists:', !!accessToken);
      console.log('Script 2: Refresh token exists:', !!refreshToken);
      console.log('Script 2: Access token length:', accessToken?.length || 0);
    }

    // Validate the shared token
    console.log('\n3. Script 2: Validating shared token...');
    const tokenPayload = sharedAuthManager.validateToken();
    if (tokenPayload) {
      console.log('Script 2: Token is valid for user:', tokenPayload.username);
      console.log('Script 2: Token roles:', tokenPayload.roles);
      console.log('Script 2: Token subject:', tokenPayload.sub);
    }

    // Use the shared session for authenticated operations
    console.log('\n4. Script 2: Using shared session for operations...');
    
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

        console.log(`Script 2: ${operation.name} - ${operation.method} ${operation.endpoint}`);
        console.log(`Script 2: Authorization: ${requestOptions.headers.Authorization.substring(0, 30)}...`);
        
        // Simulate processing time
        await new Promise(resolve => setTimeout(resolve, 400));
      }
    }

    // Check session status
    console.log('\n5. Script 2: Session status...');
    const sessionStatus = sharedAuthManager.getSessionStatus();
    console.log('Script 2: Session status:', sessionStatus.status);
    console.log('Script 2: Session authenticated:', sessionStatus.authenticated);
    console.log('Script 2: Token expires at:', sessionStatus.expiresAt);
    console.log('Script 2: Token remaining time:', sessionStatus.tokenInfo.remainingTime, 'seconds');

    // Test token refresh
    console.log('\n6. Script 2: Testing token refresh...');
    if (sharedAuthManager.shouldRefreshToken()) {
      console.log('Script 2: Token should be refreshed');
      const refreshResult = await sharedAuthManager.refreshTokens();
      console.log('Script 2: Token refreshed successfully');
      console.log('Script 2: New token length:', refreshResult.accessToken.length);
    } else {
      console.log('Script 2: Token does not need refresh yet');
    }

    // Show final storage statistics
    console.log('\n7. Script 2: Final storage statistics...');
    const stats = JwtAuthManager.getStorageStats();
    console.log('Script 2: Total sessions in storage:', stats.totalSessions);
    console.log('Script 2: Active sessions:', stats.activeCount);
    console.log('Script 2: Session details:', stats.sessions.map(s => ({
      key: s.key,
      status: s.status,
      expired: s.expired
    })));

    console.log('\nScript 2 completed successfully!');
    console.log('Both scripts have successfully shared the same JWT authentication session.');

  } catch (error) {
    console.error('Script 2 error:', error.message);
  }
}

// Execute script 2
script2().catch(console.error);

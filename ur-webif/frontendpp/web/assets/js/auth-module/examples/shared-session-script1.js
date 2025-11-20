import { JwtAuthManager } from '../src/JwtAuthManager.js';

/**
 * Script 1: First script using shared JWT authentication session
 * Demonstrates how multiple scripts can share the same authentication session
 */

async function script1() {
  console.log('=== Script 1: Shared JWT Authentication Session ===\n');

  try {
    // Get shared auth manager with specific configuration
    console.log('1. Script 1: Getting shared JWT auth manager...');
    const sharedAuthManager = JwtAuthManager.getSharedAuthManager({
      secretKey: 'shared-secret-key',
      storageKey: 'shared-jwt-session',
      accessTokenExpiry: '30m',
      refreshTokenExpiry: '1h',
      autoRefresh: true
    });

    await sharedAuthManager.initialize();

    // Authenticate user
    console.log('2. Script 1: Authenticating user...');
    const authResult = await sharedAuthManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    console.log('Script 1: Authentication successful!');
    console.log('Script 1: User:', authResult.user.username);
    console.log('Script 1: Session key:', authResult.sessionKey);

    // Check authentication status
    console.log('\n3. Script 1: Checking authentication status...');
    console.log('Script 1: Is authenticated:', sharedAuthManager.isAuthenticated());
    console.log('Script 1: Current user:', sharedAuthManager.getCurrentUser()?.username);

    // Get session information
    console.log('\n4. Script 1: Session information...');
    const sessionStatus = sharedAuthManager.getSessionStatus();
    console.log('Script 1: Session status:', sessionStatus.status);
    console.log('Script 1: Token remaining time:', sessionStatus.tokenInfo.remainingTime, 'seconds');

    // Simulate some work with the authenticated session
    console.log('\n5. Script 1: Simulating authenticated operations...');
    
    // Add authorization header to mock API requests
    const apiRequests = [
      { method: 'GET', url: '/api/user/profile' },
      { method: 'POST', url: '/api/data/create' },
      { method: 'PUT', url: '/api/user/update' }
    ];

    for (const request of apiRequests) {
      const authorizedRequest = sharedAuthManager.addAuthorizationHeader(request);
      console.log(`Script 1: ${request.method} ${request.url} - Auth: ${authorizedRequest.headers.Authorization.substring(0, 20)}...`);
      
      // Simulate API call delay
      await new Promise(resolve => setTimeout(resolve, 500));
    }

    // Show storage statistics
    console.log('\n6. Script 1: Storage statistics...');
    const stats = JwtAuthManager.getStorageStats();
    console.log('Script 1: Total sessions:', stats.totalSessions);
    console.log('Script 1: Active sessions:', stats.activeCount);
    console.log('Script 1: Session keys:', stats.sessions.map(s => s.key));

    console.log('\nScript 1 completed. Keeping session alive for Script 2...');
    
    // Keep the session alive for a bit to allow Script 2 to use it
    await new Promise(resolve => setTimeout(resolve, 3000));

    // Check session status before ending
    const finalStatus = sharedAuthManager.getSessionStatus();
    console.log('Script 1: Final session status:', finalStatus.status);

  } catch (error) {
    console.error('Script 1 error:', error.message);
  }
}

// Execute script 1
script1().catch(console.error);

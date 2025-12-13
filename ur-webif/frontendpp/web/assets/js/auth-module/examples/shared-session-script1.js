import { JwtAuthManager } from '../src/JwtAuthManager.js';

/**
 * Script 1: First script using shared JWT authentication session
 * Demonstrates how multiple scripts can share the same authentication session
 */

async function script1() {

  try {
    // Get shared auth manager with specific configuration
    const sharedAuthManager = JwtAuthManager.getSharedAuthManager({
      secretKey: 'shared-secret-key',
      storageKey: 'shared-jwt-session',
      accessTokenExpiry: '30m',
      refreshTokenExpiry: '1h',
      autoRefresh: true
    });

    await sharedAuthManager.initialize();

    // Authenticate user
    const authResult = await sharedAuthManager.authenticate({
      username: 'demo',
      password: 'demo'
    });


    // Check authentication status

    // Get session information
    const sessionStatus = sharedAuthManager.getSessionStatus();

    // Simulate some work with the authenticated session
    
    // Add authorization header to mock API requests
    const apiRequests = [
      { method: 'GET', url: '/api/user/profile' },
      { method: 'POST', url: '/api/data/create' },
      { method: 'PUT', url: '/api/user/update' }
    ];

    for (const request of apiRequests) {
      const authorizedRequest = sharedAuthManager.addAuthorizationHeader(request);
      
      // Simulate API call delay
      await new Promise(resolve => setTimeout(resolve, 500));
    }

    // Show storage statistics
    const stats = JwtAuthManager.getStorageStats();

    
    // Keep the session alive for a bit to allow Script 2 to use it
    await new Promise(resolve => setTimeout(resolve, 3000));

    // Check session status before ending
    const finalStatus = sharedAuthManager.getSessionStatus();

  } catch (error) {
    console.error('Script 1 error:', error.message);
  }
}

// Execute script 1
script1().catch(console.error);

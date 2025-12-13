import { HttpClient } from '../src/HttpClient.js';

/**
 * Script 1: First script using shared HTTP client
 * Demonstrates how multiple scripts can share the same HTTP client instance
 */

async function script1() {

  try {
    // Get shared client with specific configuration
    const sharedClient = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 10000,
      headers: {
        'User-Agent': 'Script-1/1.0',
        'X-Script-ID': 'script1'
      }
    });

    
    // Make multiple requests
    const postsResponse = await sharedClient.get('/posts');

    const usersResponse = await sharedClient.get('/users');

    // Create a new post
    const newPost = {
      title: 'Post from Script 1',
      body: 'This post was created by Script 1 using shared client',
      userId: 1
    };
    
    const createResponse = await sharedClient.post('/posts', newPost);

    // Show connection status
    const status = sharedClient.getConnectionStatus();

    // Show storage statistics
    const stats = HttpClient.getStorageStats();

    
    // Keep the client alive for a bit to allow Script 2 to use it
    await new Promise(resolve => setTimeout(resolve, 2000));

  } catch (error) {
    console.error('Script 1 error:', error.message);
  }
}

// Execute script 1
script1().catch(console.error);

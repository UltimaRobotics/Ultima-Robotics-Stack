import { HttpClient } from '../src/HttpClient.js';

/**
 * Script 2: Second script using the same shared HTTP client
 * Demonstrates how multiple scripts can share the same HTTP client instance
 */

async function script2() {

  try {
    // Wait a moment to ensure Script 1 has created the shared client
    await new Promise(resolve => setTimeout(resolve, 1000));

    // Get the same shared client with identical configuration
    const sharedClient = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 10000,
      headers: {
        'User-Agent': 'Script-1/1.0', // Same config as Script 1
        'X-Script-ID': 'script1'
      }
    });

    
    // Make requests with the same client
    const commentsResponse = await sharedClient.get('/comments');

    const albumsResponse = await sharedClient.get('/albums');

    // Get a specific post that might have been created by Script 1
    const postResponse = await sharedClient.get('/posts/1');

    // Update a post
    const updatedPost = {
      ...postResponse.data,
      title: 'Updated by Script 2',
      body: 'This post was updated by Script 2 using the shared client'
    };
    
    const updateResponse = await sharedClient.put(`/posts/${postResponse.data.id}`, updatedPost);

    // Show connection status
    const status = sharedClient.getConnectionStatus();

    // Show storage statistics
    const stats = HttpClient.getStorageStats();


  } catch (error) {
    console.error('Script 2 error:', error.message);
  }
}

// Execute script 2
script2().catch(console.error);

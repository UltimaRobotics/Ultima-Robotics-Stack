import { HttpClient } from '../src/HttpClient.js';

/**
 * Script 1: First script using shared HTTP client
 * Demonstrates how multiple scripts can share the same HTTP client instance
 */

async function script1() {
  console.log('=== Script 1: Shared HTTP Client Usage ===\n');

  try {
    // Get shared client with specific configuration
    console.log('1. Script 1: Getting shared HTTP client...');
    const sharedClient = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 10000,
      headers: {
        'User-Agent': 'Script-1/1.0',
        'X-Script-ID': 'script1'
      }
    });

    console.log('2. Script 1: Making requests with shared client...');
    
    // Make multiple requests
    const postsResponse = await sharedClient.get('/posts');
    console.log(`Script 1: Retrieved ${postsResponse.data.length} posts`);

    const usersResponse = await sharedClient.get('/users');
    console.log(`Script 1: Retrieved ${usersResponse.data.length} users`);

    // Create a new post
    const newPost = {
      title: 'Post from Script 1',
      body: 'This post was created by Script 1 using shared client',
      userId: 1
    };
    
    const createResponse = await sharedClient.post('/posts', newPost);
    console.log('Script 1: Created post with ID:', createResponse.data.id);

    // Show connection status
    console.log('\n3. Script 1: Connection status:');
    const status = sharedClient.getConnectionStatus();
    console.log('Status:', status.status);
    console.log('Connected at:', status.connectedAt);

    // Show storage statistics
    console.log('\n4. Script 1: Storage statistics:');
    const stats = HttpClient.getStorageStats();
    console.log('Total clients in storage:', stats.totalClients);
    console.log('Client keys:', stats.clientKeys);

    console.log('\nScript 1 completed. Keeping client alive for Script 2...');
    
    // Keep the client alive for a bit to allow Script 2 to use it
    await new Promise(resolve => setTimeout(resolve, 2000));

  } catch (error) {
    console.error('Script 1 error:', error.message);
  }
}

// Execute script 1
script1().catch(console.error);

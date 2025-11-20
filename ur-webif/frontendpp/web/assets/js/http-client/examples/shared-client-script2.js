import { HttpClient } from '../src/HttpClient.js';

/**
 * Script 2: Second script using the same shared HTTP client
 * Demonstrates how multiple scripts can share the same HTTP client instance
 */

async function script2() {
  console.log('\n=== Script 2: Shared HTTP Client Usage ===\n');

  try {
    // Wait a moment to ensure Script 1 has created the shared client
    await new Promise(resolve => setTimeout(resolve, 1000));

    // Get the same shared client with identical configuration
    console.log('1. Script 2: Getting shared HTTP client...');
    const sharedClient = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 10000,
      headers: {
        'User-Agent': 'Script-1/1.0', // Same config as Script 1
        'X-Script-ID': 'script1'
      }
    });

    console.log('2. Script 2: Using the same shared client instance...');
    
    // Make requests with the same client
    const commentsResponse = await sharedClient.get('/comments');
    console.log(`Script 2: Retrieved ${commentsResponse.data.length} comments`);

    const albumsResponse = await sharedClient.get('/albums');
    console.log(`Script 2: Retrieved ${albumsResponse.data.length} albums`);

    // Get a specific post that might have been created by Script 1
    const postResponse = await sharedClient.get('/posts/1');
    console.log('Script 2: Retrieved post title:', postResponse.data.title);

    // Update a post
    const updatedPost = {
      ...postResponse.data,
      title: 'Updated by Script 2',
      body: 'This post was updated by Script 2 using the shared client'
    };
    
    const updateResponse = await sharedClient.put(`/posts/${postResponse.data.id}`, updatedPost);
    console.log('Script 2: Updated post, new title:', updateResponse.data.title);

    // Show connection status
    console.log('\n3. Script 2: Connection status:');
    const status = sharedClient.getConnectionStatus();
    console.log('Status:', status.status);
    console.log('Connected at:', status.connectedAt);

    // Show storage statistics
    console.log('\n4. Script 2: Storage statistics:');
    const stats = HttpClient.getStorageStats();
    console.log('Total clients in storage:', stats.totalClients);
    console.log('Client keys:', stats.clientKeys);

    console.log('\nScript 2 completed successfully!');

  } catch (error) {
    console.error('Script 2 error:', error.message);
  }
}

// Execute script 2
script2().catch(console.error);

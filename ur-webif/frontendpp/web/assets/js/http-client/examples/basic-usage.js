import { HttpClient } from '../src/HttpClient.js';
import { HttpConfig } from '../lib/config.js';

/**
 * Basic HTTP Client Usage Example
 * Demonstrates how to use the HTTP client for common operations
 */

async function basicUsageExample() {

  try {
    // Example 1: Simple GET request
    const client = new HttpClient('https://jsonplaceholder.typicode.com');
    
    const response = await client.get('/posts/1');

    // Example 2: POST request with data
    const newPost = {
      title: 'Test Post',
      body: 'This is a test post from our HTTP client',
      userId: 1
    };
    
    const postResponse = await client.post('/posts', newPost);

    // Example 3: Using configuration with authentication
    const config = HttpConfig.forServer('https://jsonplaceholder.typicode.com')
      .setTimeout(5000)
      .setHeaders({
        'User-Agent': 'WebModules-HTTP-Client/1.0',
        'Accept': 'application/json'
      });
    
    const configuredClient = new HttpClient(config);
    const configResponse = await configuredClient.get('/users/1');


  } catch (error) {
    console.error('Error in basic usage example:', error.message);
    if (error.response) {
      console.error('Response details:', error.response);
    }
  }
}

async function sharedClientExample() {

  try {
    // Example 1: Get shared client
    const sharedClient1 = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 8000
    });

    const sharedClient2 = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 8000
    });

    // Both references should point to the same instance

    // Use the shared client
    const response1 = await sharedClient1.get('/posts/1');

    const response2 = await sharedClient2.get('/posts/2');

    // Check storage statistics
    const stats = HttpClient.getStorageStats();


  } catch (error) {
    console.error('Error in shared client example:', error.message);
  }
}

async function errorHandlingExample() {

  try {
    // Example 1: Handling 404 errors
    const client = new HttpClient('https://jsonplaceholder.typicode.com');
    
    try {
      await client.get('/nonexistent-endpoint');
    } catch (error) {
    }

    // Example 2: Handling timeout errors
    const timeoutClient = new HttpClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 1 // 1ms timeout to force timeout
    });
    
    try {
      await timeoutClient.get('/posts/1');
    } catch (error) {
    }


  } catch (error) {
    console.error('Unexpected error:', error.message);
  }
}

// Run all examples
async function runAllExamples() {
  await basicUsageExample();
  await sharedClientExample();
  await errorHandlingExample();
}

// Execute examples
runAllExamples().catch(console.error);

import { HttpClient } from '../src/HttpClient.js';
import { HttpConfig } from '../lib/config.js';

/**
 * Basic HTTP Client Usage Example
 * Demonstrates how to use the HTTP client for common operations
 */

async function basicUsageExample() {
  console.log('=== Basic HTTP Client Usage Example ===\n');

  try {
    // Example 1: Simple GET request
    console.log('1. Creating a simple HTTP client...');
    const client = new HttpClient('https://jsonplaceholder.typicode.com');
    
    console.log('2. Making a GET request...');
    const response = await client.get('/posts/1');
    console.log('Response:', JSON.stringify(response.data, null, 2));
    console.log('Status:', response.status);

    // Example 2: POST request with data
    console.log('\n3. Making a POST request with data...');
    const newPost = {
      title: 'Test Post',
      body: 'This is a test post from our HTTP client',
      userId: 1
    };
    
    const postResponse = await client.post('/posts', newPost);
    console.log('Created post:', JSON.stringify(postResponse.data, null, 2));

    // Example 3: Using configuration with authentication
    console.log('\n4. Creating client with custom configuration...');
    const config = HttpConfig.forServer('https://jsonplaceholder.typicode.com')
      .setTimeout(5000)
      .setHeaders({
        'User-Agent': 'WebModules-HTTP-Client/1.0',
        'Accept': 'application/json'
      });
    
    const configuredClient = new HttpClient(config);
    const configResponse = await configuredClient.get('/users/1');
    console.log('User data:', JSON.stringify(configResponse.data, null, 2));

    console.log('\n=== Basic example completed successfully ===');

  } catch (error) {
    console.error('Error in basic usage example:', error.message);
    if (error.response) {
      console.error('Response details:', error.response);
    }
  }
}

async function sharedClientExample() {
  console.log('\n=== Shared Client Usage Example ===\n');

  try {
    // Example 1: Get shared client
    console.log('1. Getting shared HTTP client...');
    const sharedClient1 = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 8000
    });

    console.log('2. Getting same shared client again...');
    const sharedClient2 = HttpClient.getSharedClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 8000
    });

    // Both references should point to the same instance
    console.log('Same instance?', sharedClient1 === sharedClient2);

    // Use the shared client
    console.log('3. Using shared client to make requests...');
    const response1 = await sharedClient1.get('/posts/1');
    console.log('Response from client 1:', response1.data.title);

    const response2 = await sharedClient2.get('/posts/2');
    console.log('Response from client 2:', response2.data.title);

    // Check storage statistics
    console.log('\n4. Storage statistics:');
    const stats = HttpClient.getStorageStats();
    console.log('Total clients:', stats.totalClients);
    console.log('Client keys:', stats.clientKeys);

    console.log('\n=== Shared client example completed successfully ===');

  } catch (error) {
    console.error('Error in shared client example:', error.message);
  }
}

async function errorHandlingExample() {
  console.log('\n=== Error Handling Example ===\n');

  try {
    // Example 1: Handling 404 errors
    console.log('1. Testing 404 error handling...');
    const client = new HttpClient('https://jsonplaceholder.typicode.com');
    
    try {
      await client.get('/nonexistent-endpoint');
    } catch (error) {
      console.log('Caught 404 error:', error.message);
      console.log('Error status:', error.response?.status);
    }

    // Example 2: Handling timeout errors
    console.log('\n2. Testing timeout error handling...');
    const timeoutClient = new HttpClient({
      baseURL: 'https://jsonplaceholder.typicode.com',
      timeout: 1 // 1ms timeout to force timeout
    });
    
    try {
      await timeoutClient.get('/posts/1');
    } catch (error) {
      console.log('Caught timeout error:', error.message);
    }

    console.log('\n=== Error handling example completed ===');

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

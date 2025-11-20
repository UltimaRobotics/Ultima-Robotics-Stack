# WebModules HTTP Client

A generic HTTP client implementation in JavaScript with shared storage support, allowing multiple scripts to use the same connected client instance.

## Features

- **Generic HTTP Client**: Supports GET, POST, PUT, PATCH, DELETE operations
- **Shared Storage**: Multiple scripts can share the same HTTP client instance
- **Configuration Management**: Flexible configuration with authentication, timeouts, retries
- **Error Handling**: Comprehensive error handling with retry logic
- **Connection Management**: Track connection status and cleanup inactive clients
- **ES Modules**: Modern JavaScript with ES module support

## Project Structure

```
http-client/
├── lib/
│   ├── config.js          # HTTP configuration management
│   └── http-storage.js    # Shared client storage
├── src/
│   └── HttpClient.js      # Main HTTP client implementation
├── examples/
│   ├── basic-usage.js                 # Basic usage examples
│   ├── shared-client-script1.js       # Script 1 for shared client demo
│   ├── shared-client-script2.js       # Script 2 for shared client demo
│   └── run-shared-example.js          # Runner for shared client demo
├── tests/
│   └── http-client.test.js            # Basic tests
├── package.json
└── README.md
```

## Installation

```bash
cd http-client
npm install
```

## Basic Usage

### Simple HTTP Client

```javascript
import { HttpClient } from './src/HttpClient.js';

// Create a client with base URL
const client = new HttpClient('https://api.example.com');

// Make requests
const response = await client.get('/users');
console.log(response.data);

const newUser = await client.post('/users', {
  name: 'John Doe',
  email: 'john@example.com'
});
```

### Advanced Configuration

```javascript
import { HttpClient } from './src/HttpClient.js';
import { HttpConfig } from './lib/config.js';

const config = HttpConfig.forServer('https://api.example.com')
  .setTimeout(10000)
  .setAuth('bearer', 'your-token')
  .setHeaders({
    'User-Agent': 'MyApp/1.0',
    'Accept': 'application/json'
  })
  .setRetryConfig({
    maxRetries: 3,
    retryDelay: 1000
  });

const client = new HttpClient(config);
```

## Shared Client Usage

The key feature of this HTTP client is the ability to share connections across multiple scripts:

### Script 1

```javascript
// script1.js
import { HttpClient } from './src/HttpClient.js';

const sharedClient = HttpClient.getSharedClient({
  baseURL: 'https://api.example.com',
  timeout: 10000,
  headers: { 'X-Script-ID': 'script1' }
});

// Use the client
const posts = await sharedClient.get('/posts');
console.log(`Script 1: Retrieved ${posts.data.length} posts`);
```

### Script 2

```javascript
// script2.js
import { HttpClient } from './src/HttpClient.js';

// Get the same shared client (identical configuration)
const sharedClient = HttpClient.getSharedClient({
  baseURL: 'https://api.example.com',
  timeout: 10000,
  headers: { 'X-Script-ID': 'script1' }
});

// Use the same client instance
const users = await sharedClient.get('/users');
console.log(`Script 2: Retrieved ${users.data.length} users`);
```

Both scripts will use the same HTTP client instance, sharing the connection and configuration.

## API Reference

### HttpClient

#### Constructor

```javascript
new HttpClient(baseURL|config|HttpConfig)
```

#### Methods

- `get(url, options)` - Make GET request
- `post(url, data, options)` - Make POST request
- `put(url, data, options)` - Make PUT request
- `patch(url, data, options)` - Make PATCH request
- `delete(url, options)` - Make DELETE request
- `request(method, url, options)` - Make custom request
- `updateConfig(newConfig)` - Update client configuration
- `getConnectionStatus()` - Get connection status
- `close()` - Close client and remove from storage

#### Static Methods

- `HttpClient.getSharedClient(config)` - Get or create shared client
- `HttpClient.getStorageStats()` - Get storage statistics
- `HttpClient.cleanupInactiveClients(timeoutMs)` - Cleanup inactive clients

### HttpConfig

#### Static Methods

- `HttpConfig.forServer(serverUrl, options)` - Create config for server
- `HttpConfig.forLocalhost(port, options)` - Create config for localhost

#### Methods

- `setAuth(type, token, options)` - Set authentication
- `setHeaders(headers)` - Set default headers
- `setTimeout(timeoutMs)` - Set request timeout
- `setRetryConfig(options)` - Set retry configuration
- `setSslConfig(options)` - Set SSL configuration
- `setProxy(proxyUrl, options)` - Set proxy configuration
- `clone()` - Clone configuration
- `validate()` - Validate configuration
- `toObject()` - Convert to plain object

## Running Examples

### Basic Usage Example

```bash
node examples/basic-usage.js
```

### Shared Client Example

```bash
# Run scripts sequentially
node examples/run-shared-example.js sequential

# Run scripts concurrently
node examples/run-shared-example.js concurrent
```

## Running Tests

```bash
npm test
```

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `baseURL` | string | '' | Base URL for all requests |
| `timeout` | number | 10000 | Request timeout in milliseconds |
| `headers` | object | {} | Default headers for all requests |
| `auth` | object | null | Authentication configuration |
| `maxRetries` | number | 3 | Maximum number of retry attempts |
| `retryDelay` | number | 1000 | Delay between retries in milliseconds |
| `followRedirects` | boolean | true | Whether to follow HTTP redirects |
| `maxRedirects` | number | 5 | Maximum number of redirects to follow |

## Authentication Types

### Bearer Token

```javascript
config.setAuth('bearer', 'your-token');
```

### Basic Authentication

```javascript
config.setAuth('basic', null, {
  username: 'user',
  password: 'pass'
});
```

### API Key

```javascript
config.setAuth('apikey', 'your-api-key', {
  key: 'X-API-Key' // Custom header name (optional)
});
```

## Error Handling

The client provides comprehensive error handling:

```javascript
try {
  const response = await client.get('/endpoint');
} catch (error) {
  if (error.response) {
    console.log('HTTP Error:', error.response.status);
    console.log('Response data:', error.response.data);
  } else if (error.message.includes('timeout')) {
    console.log('Request timeout');
  } else {
    console.log('Network error:', error.message);
  }
}
```

## Storage Management

The shared storage automatically manages client instances:

```javascript
// Get storage statistics
const stats = HttpClient.getStorageStats();
console.log('Active clients:', stats.totalClients);

// Cleanup inactive clients older than 30 minutes
const cleanedUp = HttpClient.cleanupInactiveClients(30 * 60 * 1000);
console.log('Cleaned up clients:', cleanedUp);
```

## License

MIT License

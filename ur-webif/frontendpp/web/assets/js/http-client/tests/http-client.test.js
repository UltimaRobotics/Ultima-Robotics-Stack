/**
 * HTTP Client Tests
 * Basic tests for the HTTP client implementation
 */

import { test, describe } from 'node:test';
import assert from 'node:assert';
import { HttpClient } from '../src/HttpClient.js';
import { HttpConfig } from '../lib/config.js';
import { httpStorage } from '../lib/http-storage.js';

describe('HttpClient', () => {
  test('should create client with string baseURL', () => {
    const client = new HttpClient('https://api.example.com');
    assert.strictEqual(client.config.baseURL, 'https://api.example.com');
  });

  test('should create client with configuration object', () => {
    const config = { baseURL: 'https://api.example.com', timeout: 5000 };
    const client = new HttpClient(config);
    assert.strictEqual(client.config.baseURL, 'https://api.example.com');
    assert.strictEqual(client.config.timeout, 5000);
  });

  test('should create client with HttpConfig instance', () => {
    const httpConfig = new HttpConfig({ baseURL: 'https://api.example.com' });
    const client = new HttpClient(httpConfig);
    assert.strictEqual(client.config, httpConfig);
  });

  test('should generate unique storage key', () => {
    const client1 = new HttpClient('https://api.example.com');
    const client2 = new HttpClient('https://api.example.com');
    const client3 = new HttpClient('https://different.example.com');
    
    assert.strictEqual(client1.storageKey, client2.storageKey);
    assert.notStrictEqual(client1.storageKey, client3.storageKey);
  });

  test('should build URLs correctly', () => {
    const client = new HttpClient('https://api.example.com');
    
    assert.strictEqual(client.buildUrl('/users'), 'https://api.example.com/users');
    assert.strictEqual(client.buildUrl('users'), 'https://api.example.com/users');
    assert.strictEqual(client.buildUrl('https://other.com/test'), 'https://other.com/test');
  });

  test('should build request options correctly', () => {
    const client = new HttpClient('https://api.example.com');
    const options = client.buildRequestOptions('POST', {
      data: { name: 'test' },
      headers: { 'Custom-Header': 'value' }
    });
    
    assert.strictEqual(options.method, 'POST');
    assert.ok(options.body.includes('test'));
    assert.strictEqual(options.headers['Custom-Header'], 'value');
    assert.strictEqual(options.headers['Content-Type'], 'application/json');
  });

  test('should handle shared client creation', () => {
    const client1 = HttpClient.getSharedClient({ baseURL: 'https://api.example.com' });
    const client2 = HttpClient.getSharedClient({ baseURL: 'https://api.example.com' });
    const client3 = HttpClient.getSharedClient({ baseURL: 'https://different.com' });
    
    assert.strictEqual(client1, client2);
    assert.notStrictEqual(client1, client3);
  });

  test('should update configuration', () => {
    const client = new HttpClient('https://api.example.com');
    const newTimeout = 15000;
    
    client.updateConfig({ timeout: newTimeout });
    assert.strictEqual(client.config.timeout, newTimeout);
  });

  test('should close and remove from storage', () => {
    const client = HttpClient.getSharedClient({ baseURL: 'https://api.example.com' });
    const storageKey = client.storageKey;
    
    assert.ok(httpStorage.hasClient(storageKey));
    
    client.close();
    assert.ok(!httpStorage.hasClient(storageKey));
  });
});

describe('HttpConfig', () => {
  test('should create config with default values', () => {
    const config = new HttpConfig();
    assert.strictEqual(config.timeout, 10000);
    assert.strictEqual(config.followRedirects, true);
    assert.strictEqual(config.maxRedirects, 5);
  });

  test('should create config for server', () => {
    const config = HttpConfig.forServer('https://api.example.com', { timeout: 5000 });
    assert.strictEqual(config.baseURL, 'https://api.example.com');
    assert.strictEqual(config.timeout, 5000);
  });

  test('should create config for localhost', () => {
    const config = HttpConfig.forLocalhost(3000);
    assert.strictEqual(config.baseURL, 'http://localhost:3000');
    assert.strictEqual(config.timeout, 5000);
  });

  test('should set bearer authentication', () => {
    const config = new HttpConfig();
    config.setAuth('bearer', 'token123');
    
    assert.strictEqual(config.auth.type, 'bearer');
    assert.strictEqual(config.auth.token, 'token123');
    assert.strictEqual(config.headers.Authorization, 'Bearer token123');
  });

  test('should set basic authentication', () => {
    const config = new HttpConfig();
    config.setAuth('basic', null, { username: 'user', password: 'pass' });
    
    assert.strictEqual(config.auth.type, 'basic');
    assert.strictEqual(config.auth.username, 'user');
    assert.strictEqual(config.auth.password, 'pass');
    assert.ok(config.headers.Authorization.startsWith('Basic '));
  });

  test('should set API key authentication', () => {
    const config = new HttpConfig();
    config.setAuth('apikey', 'key123', { key: 'X-Custom-Key' });
    
    assert.strictEqual(config.auth.type, 'apikey');
    assert.strictEqual(config.auth.value, 'key123');
    assert.strictEqual(config.headers['X-Custom-Key'], 'key123');
  });

  test('should chain configuration methods', () => {
    const config = new HttpConfig()
      .setTimeout(5000)
      .setHeaders({ 'User-Agent': 'test' })
      .setAuth('bearer', 'token');
    
    assert.strictEqual(config.timeout, 5000);
    assert.strictEqual(config.headers['User-Agent'], 'test');
    assert.strictEqual(config.headers.Authorization, 'Bearer token');
  });

  test('should clone configuration', () => {
    const original = new HttpConfig({ timeout: 5000 });
    const cloned = original.clone();
    
    assert.strictEqual(cloned.timeout, original.timeout);
    assert.notStrictEqual(cloned, original);
  });

  test('should validate configuration', () => {
    const config = new HttpConfig({ baseURL: 'https://api.example.com' });
    assert.doesNotThrow(() => config.validate());
    
    const invalidConfig = new HttpConfig({ timeout: -1 });
    assert.throws(() => invalidConfig.validate());
  });
});

describe('HttpStorage', () => {
  beforeEach(() => {
    // Clear storage before each test
    httpStorage.clients.clear();
    httpStorage.connections.clear();
  });

  test('should store and retrieve clients', () => {
    const client = new HttpClient('https://api.example.com');
    const config = { baseURL: 'https://api.example.com' };
    
    httpStorage.setClient('test-key', client, config);
    
    assert.strictEqual(httpStorage.getClient('test-key'), client);
    assert.ok(httpStorage.hasClient('test-key'));
  });

  test('should remove clients', () => {
    const client = new HttpClient('https://api.example.com');
    httpStorage.setClient('test-key', client, {});
    
    const removed = httpStorage.removeClient('test-key');
    assert.ok(removed);
    assert.ok(!httpStorage.hasClient('test-key'));
  });

  test('should get all client keys', () => {
    httpStorage.setClient('key1', {}, {});
    httpStorage.setClient('key2', {}, {});
    
    const keys = httpStorage.getAllClientKeys();
    assert.strictEqual(keys.length, 2);
    assert.ok(keys.includes('key1'));
    assert.ok(keys.includes('key2'));
  });

  test('should update connection status', () => {
    const client = new HttpClient('https://api.example.com');
    httpStorage.setClient('test-key', client, {});
    
    httpStorage.updateConnectionStatus('test-key', 'error');
    const status = httpStorage.getConnectionStatus('test-key');
    
    assert.strictEqual(status.status, 'error');
  });

  test('should provide storage statistics', () => {
    httpStorage.setClient('key1', {}, {});
    httpStorage.setClient('key2', {}, {});
    
    const stats = httpStorage.getStats();
    assert.strictEqual(stats.totalClients, 2);
    assert.strictEqual(stats.totalConnections, 2);
  });

  test('should cleanup inactive clients', () => {
    const client = new HttpClient('https://api.example.com');
    httpStorage.setClient('active-key', client, {});
    
    // Manually set lastUsed to be old
    const clientData = httpStorage.clients.get('active-key');
    clientData.lastUsed = new Date(Date.now() - 60 * 60 * 1000); // 1 hour ago
    
    const cleanedUp = httpStorage.cleanupInactiveClients(30 * 60 * 1000); // 30 minutes timeout
    assert.strictEqual(cleanedUp.length, 1);
    assert.ok(cleanedUp.includes('active-key'));
  });
});

console.log('All HTTP client tests completed!');

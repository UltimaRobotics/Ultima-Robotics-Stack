/**
 * JWT Auth Manager Tests
 * Basic tests for the JWT authentication manager implementation
 */

import { test, describe, beforeEach } from 'node:test';
import assert from 'node:assert';
import { JwtAuthManager } from '../src/JwtAuthManager.js';
import { JwtConfig } from '../lib/jwt-config.js';
import { authStorage } from '../lib/auth-storage.js';
import { JwtUtils } from '../lib/jwt-utils.js';

describe('JwtAuthManager', () => {
  let authManager;
  let testConfig;

  beforeEach(() => {
    // Clear storage before each test
    authStorage.clearAllSessions();
    
    testConfig = new JwtConfig({
      secretKey: 'test-secret-key',
      accessTokenExpiry: '1h',
      refreshTokenExpiry: '1d',
      autoRefresh: false
    });
    
    authManager = new JwtAuthManager(testConfig);
  });

  test('should create auth manager with configuration', () => {
    assert.ok(authManager instanceof JwtAuthManager);
    assert.strictEqual(authManager.config.secretKey, 'test-secret-key');
    assert.strictEqual(authManager.config.autoRefresh, false);
  });

  test('should initialize successfully', async () => {
    await authManager.initialize();
    assert.strictEqual(authManager.isInitialized, true);
  });

  test('should not initialize twice', async () => {
    await authManager.initialize();
    await authManager.initialize(); // Should not throw
    assert.strictEqual(authManager.isInitialized, true);
  });

  test('should authenticate user with valid credentials', async () => {
    await authManager.initialize();
    
    const authResult = await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    assert.strictEqual(authResult.success, true);
    assert.ok(authResult.user);
    assert.strictEqual(authResult.user.username, 'demo');
    assert.ok(authResult.tokens.accessToken);
    assert.ok(authResult.tokens.refreshToken);
    assert.strictEqual(authResult.tokens.tokenType, 'Bearer');
  });

  test('should fail authentication with invalid credentials', async () => {
    await authManager.initialize();
    
    await assert.rejects(
      async () => {
        await authManager.authenticate({
          username: 'invalid',
          password: 'invalid'
        });
      },
      /Authentication failed/
    );
  });

  test('should track authentication status', async () => {
    await authManager.initialize();
    
    assert.strictEqual(authManager.isAuthenticated(), false);
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    assert.strictEqual(authManager.isAuthenticated(), true);
  });

  test('should get current user after authentication', async () => {
    await authManager.initialize();
    
    assert.strictEqual(authManager.getCurrentUser(), null);
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    const user = authManager.getCurrentUser();
    assert.ok(user);
    assert.strictEqual(user.username, 'demo');
  });

  test('should get access and refresh tokens', async () => {
    await authManager.initialize();
    
    assert.strictEqual(authManager.getAccessToken(), null);
    assert.strictEqual(authManager.getRefreshToken(), null);
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    const accessToken = authManager.getAccessToken();
    const refreshToken = authManager.getRefreshToken();
    
    assert.ok(accessToken);
    assert.ok(refreshToken);
    assert(typeof accessToken === 'string');
    assert(typeof refreshToken === 'string');
  });

  test('should validate tokens', async () => {
    await authManager.initialize();
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    const payload = authManager.validateToken();
    assert.ok(payload);
    assert.strictEqual(payload.username, 'demo');
    assert.ok(payload.sub);
  });

  test('should refresh tokens', async () => {
    await authManager.initialize();
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    const originalToken = authManager.getAccessToken();
    
    const refreshResult = await authManager.refreshTokens();
    
    assert.ok(refreshResult.accessToken);
    assert.strictEqual(refreshResult.refreshToken, authManager.getRefreshToken());
    assert.notStrictEqual(refreshResult.accessToken, originalToken);
  });

  test('should add authorization header', async () => {
    await authManager.initialize();
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    const requestOptions = authManager.addAuthorizationHeader({
      method: 'GET',
      url: '/api/test'
    });

    assert.ok(requestOptions.headers.Authorization);
    assert.ok(requestOptions.headers.Authorization.startsWith('Bearer '));
  });

  test('should logout and clear session', async () => {
    await authManager.initialize();
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    assert.strictEqual(authManager.isAuthenticated(), true);
    
    authManager.logout();
    
    assert.strictEqual(authManager.isAuthenticated(), false);
    assert.strictEqual(authManager.getAccessToken(), null);
    assert.strictEqual(authManager.getRefreshToken(), null);
  });

  test('should handle shared auth manager creation', async () => {
    const shared1 = JwtAuthManager.getSharedAuthManager({
      secretKey: 'shared-secret',
      storageKey: 'test-shared-session'
    });

    const shared2 = JwtAuthManager.getSharedAuthManager({
      secretKey: 'shared-secret',
      storageKey: 'test-shared-session'
    });

    const shared3 = JwtAuthManager.getSharedAuthManager({
      secretKey: 'different-secret',
      storageKey: 'different-session'
    });

    assert.strictEqual(shared1, shared2);
    assert.notStrictEqual(shared1, shared3);
  });

  test('should get session status', async () => {
    await authManager.initialize();
    
    let status = authManager.getSessionStatus();
    assert.strictEqual(status.authenticated, false);
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    status = authManager.getSessionStatus();
    assert.strictEqual(status.authenticated, true);
    assert.ok(status.user);
    assert.ok(status.tokenInfo);
  });

  test('should update configuration', async () => {
    await authManager.initialize();
    
    authManager.updateConfig({
      accessTokenExpiry: '2h',
      autoRefresh: true
    });

    assert.strictEqual(authManager.config.accessTokenExpiry, '2h');
    assert.strictEqual(authManager.config.autoRefresh, true);
  });

  test('should destroy auth manager', async () => {
    await authManager.initialize();
    
    await authManager.authenticate({
      username: 'demo',
      password: 'demo'
    });

    assert.strictEqual(authManager.isAuthenticated(), true);
    
    authManager.destroy();
    
    assert.strictEqual(authManager.isInitialized, false);
    assert.strictEqual(authManager.isAuthenticated(), false);
  });

  test('should throw error when not initialized', async () => {
    const nonInitializedAuthManager = new JwtAuthManager(testConfig);
    
    await assert.rejects(
      async () => {
        await nonInitializedAuthManager.authenticate({ username: 'demo', password: 'demo' });
      },
      /JwtAuthManager must be initialized/
    );
  });
});

describe('JwtConfig', () => {
  test('should create config with default values', () => {
    const config = new JwtConfig();
    assert.strictEqual(config.secretKey, 'default-secret-key');
    assert.strictEqual(config.algorithm, 'HS256');
    assert.strictEqual(config.accessTokenExpiry, '15m');
    assert.strictEqual(config.refreshTokenExpiry, '7d');
  });

  test('should create development config', () => {
    const config = JwtConfig.forDevelopment();
    assert.strictEqual(config.secretKey, 'dev-secret-key');
    assert.strictEqual(config.accessTokenExpiry, '1h');
    assert.strictEqual(config.validateIssuer, false);
  });

  test('should create production config', () => {
    const config = JwtConfig.forProduction({
      secretKey: 'prod-secret'
    });
    assert.strictEqual(config.secretKey, 'prod-secret');
    assert.strictEqual(config.validateIssuer, true);
    assert.strictEqual(config.blacklistEnabled, true);
  });

  test('should create testing config', () => {
    const config = JwtConfig.forTesting();
    assert.strictEqual(config.secretKey, 'test-secret-key');
    assert.strictEqual(config.accessTokenExpiry, '60s');
    assert.strictEqual(config.autoRefresh, false);
  });

  test('should chain configuration methods', () => {
    const config = new JwtConfig()
      .setSecretKey('new-secret')
      .setTokenExpiry('2h', '30d')
      .setAutoRefresh(true);

    assert.strictEqual(config.secretKey, 'new-secret');
    assert.strictEqual(config.accessTokenExpiry, '2h');
    assert.strictEqual(config.refreshTokenExpiry, '30d');
    assert.strictEqual(config.autoRefresh, true);
  });

  test('should validate configuration', () => {
    const validConfig = new JwtConfig({ secretKey: 'valid-secret' });
    assert.doesNotThrow(() => validConfig.validate());

    const invalidConfig = new JwtConfig({ secretKey: '' });
    assert.throws(() => invalidConfig.validate());
  });

  test('should clone configuration', () => {
    const original = new JwtConfig({ secretKey: 'test-secret' });
    const cloned = original.clone();
    
    assert.strictEqual(cloned.secretKey, original.secretKey);
    assert.notStrictEqual(cloned, original);
  });

  test('should convert time strings to milliseconds', () => {
    assert.strictEqual(JwtConfig.timeToMilliseconds('60s'), 60000);
    assert.strictEqual(JwtConfig.timeToMilliseconds('5m'), 300000);
    assert.strictEqual(JwtConfig.timeToMilliseconds('2h'), 7200000);
    assert.strictEqual(JwtConfig.timeToMilliseconds('1d'), 86400000);
  });
});

describe('JwtUtils', () => {
  const secretKey = 'test-secret';

  test('should create and verify JWT token', () => {
    const payload = { sub: '123', username: 'test' };
    const token = JwtUtils.createToken(payload, secretKey, {
      expiresIn: '1h',
      issuer: 'test-issuer',
      audience: 'test-audience'
    });

    assert.ok(token);
    assert(typeof token === 'string');

    const verified = JwtUtils.verifyToken(token, secretKey, {
      issuer: 'test-issuer',
      audience: 'test-audience'
    });

    assert.strictEqual(verified.sub, '123');
    assert.strictEqual(verified.username, 'test');
    assert.strictEqual(verified.iss, 'test-issuer');
    assert.strictEqual(verified.aud, 'test-audience');
  });

  test('should decode token without verification', () => {
    const payload = { sub: '123', username: 'test' };
    const token = JwtUtils.createToken(payload, secretKey);

    const decoded = JwtUtils.decodeToken(token);
    
    assert.strictEqual(decoded.payload.sub, '123');
    assert.strictEqual(decoded.payload.username, 'test');
    assert.strictEqual(decoded.header.typ, 'JWT');
  });

  test('should check token expiration', () => {
    const payload = { sub: '123' };
    const expiredToken = JwtUtils.createToken(payload, secretKey, {
      expiresIn: '1s'
    });

    // Wait for token to expire
    setTimeout(() => {
      assert.strictEqual(JwtUtils.isTokenExpired(expiredToken), true);
    }, 1100);

    // Fresh token should not be expired
    const freshToken = JwtUtils.createToken(payload, secretKey, {
      expiresIn: '1h'
    });

    assert.strictEqual(JwtUtils.isTokenExpired(freshToken), false);
  });

  test('should refresh token', () => {
    const payload = { sub: '123', username: 'test' };
    const originalToken = JwtUtils.createToken(payload, secretKey, {
      expiresIn: '1h'
    });

    const refreshedToken = JwtUtils.refreshToken(originalToken, secretKey, {
      expiresIn: '2h'
    });

    assert.notStrictEqual(refreshedToken, originalToken);
    
    const originalInfo = JwtUtils.getTokenInfo(originalToken);
    const refreshedInfo = JwtUtils.getTokenInfo(refreshedToken);
    
    assert.ok(refreshedInfo.remainingTime > originalInfo.remainingTime);
  });

  test('should create token pair', () => {
    const payload = { sub: '123', username: 'test' };
    const tokenPair = JwtUtils.createTokenPair(payload, secretKey, {
      accessTokenExpiry: '15m',
      refreshTokenExpiry: '7d'
    });

    assert.ok(tokenPair.accessToken);
    assert.ok(tokenPair.refreshToken);
    assert.strictEqual(tokenPair.tokenType, 'Bearer');
    assert.ok(tokenPair.expiresIn > 0);
  });

  test('should extract token from authorization header', () => {
    const token = 'test.jwt.token';
    const authHeader = `Bearer ${token}`;
    
    const extracted = JwtUtils.extractTokenFromHeader(authHeader);
    assert.strictEqual(extracted, token);

    assert.strictEqual(JwtUtils.extractTokenFromHeader('invalid'), null);
    assert.strictEqual(JwtUtils.extractTokenFromHeader('Basic dGVzdA=='), null);
  });

  test('should validate token format', () => {
    const validToken = JwtUtils.createToken({ sub: '123' }, secretKey);
    assert.strictEqual(JwtUtils.isValidTokenFormat(validToken), true);
    
    assert.strictEqual(JwtUtils.isValidTokenFormat('invalid'), false);
    assert.strictEqual(JwtUtils.isValidTokenFormat('a.b'), false);
    assert.strictEqual(JwtUtils.isValidTokenFormat('a.b.c.d'), false);
  });

  test('should generate JWT ID', () => {
    const jti1 = JwtUtils.generateJwtId();
    const jti2 = JwtUtils.generateJwtId();
    
    assert.strictEqual(jti1.length, 32); // 16 bytes * 2 (hex)
    assert.strictEqual(jti2.length, 32);
    assert.notStrictEqual(jti1, jti2);
  });

  test('should get token information', () => {
    const payload = { sub: '123', username: 'test' };
    const token = JwtUtils.createToken(payload, secretKey, {
      expiresIn: '1h',
      issuer: 'test-issuer'
    });

    const info = JwtUtils.getTokenInfo(token);
    
    assert.strictEqual(info.payload.sub, '123');
    assert.strictEqual(info.payload.username, 'test');
    assert.strictEqual(info.isExpired, false);
    assert.ok(info.expiresAt);
    assert.ok(info.remainingTime > 0);
  });
});

describe('AuthStorage', () => {
  beforeEach(() => {
    authStorage.clearAllSessions();
  });

  test('should store and retrieve sessions', () => {
    const sessionData = {
      user: { id: '1', username: 'test' },
      accessToken: 'access-token',
      refreshToken: 'refresh-token',
      isAuthenticated: true
    };

    authStorage.setSession('test-session', sessionData, {});
    
    const retrieved = authStorage.getSession('test-session');
    assert.strictEqual(retrieved.user.username, 'test');
    assert.strictEqual(retrieved.accessToken, 'access-token');
  });

  test('should get session by access token', () => {
    const sessionData = {
      user: { id: '1' },
      accessToken: 'test-access-token',
      refreshToken: 'test-refresh-token',
      isAuthenticated: true
    };

    authStorage.setSession('test-session', sessionData);
    
    const retrieved = authStorage.getSessionByAccessToken('test-access-token');
    assert.strictEqual(retrieved.user.id, '1');
  });

  test('should update session tokens', () => {
    const sessionData = {
      user: { id: '1' },
      accessToken: 'old-access-token',
      refreshToken: 'old-refresh-token',
      isAuthenticated: true
    };

    authStorage.setSession('test-session', sessionData);
    
    const updated = authStorage.updateSessionTokens('test-session', {
      accessToken: 'new-access-token',
      refreshToken: 'new-refresh-token'
    });

    assert.strictEqual(updated, true);
    
    const session = authStorage.getSession('test-session');
    assert.strictEqual(session.accessToken, 'new-access-token');
    assert.strictEqual(session.refreshToken, 'new-refresh-token');
  });

  test('should remove sessions', () => {
    const sessionData = {
      user: { id: '1' },
      accessToken: 'test-token',
      isAuthenticated: true
    };

    authStorage.setSession('test-session', sessionData);
    assert.ok(authStorage.hasSession('test-session'));
    
    const removed = authStorage.removeSession('test-session');
    assert.strictEqual(removed, true);
    assert.ok(!authStorage.hasSession('test-session'));
  });

  test('should check session expiration', () => {
    const sessionData = {
      user: { id: '1' },
      accessToken: 'test-token',
      isAuthenticated: true,
      expiresAt: new Date(Date.now() - 1000) // Expired 1 second ago
    };

    authStorage.setSession('expired-session', sessionData);
    
    assert.strictEqual(authStorage.isSessionExpired('expired-session'), true);
    assert.strictEqual(authStorage.isSessionExpired('nonexistent'), true);
  });

  test('should provide storage statistics', () => {
    const session1 = { user: { id: '1' }, accessToken: 'token1', isAuthenticated: true };
    const session2 = { user: { id: '2' }, accessToken: 'token2', isAuthenticated: true };

    authStorage.setSession('session1', session1);
    authStorage.setSession('session2', session2);

    const stats = authStorage.getStats();
    
    assert.strictEqual(stats.totalSessions, 2);
    assert.strictEqual(stats.activeTokens, 2);
    assert.strictEqual(stats.sessions.length, 2);
  });

  test('should cleanup expired sessions', () => {
    const expiredSession = {
      user: { id: '1' },
      accessToken: 'expired-token',
      isAuthenticated: true,
      expiresAt: new Date(Date.now() - 1000)
    };

    const validSession = {
      user: { id: '2' },
      accessToken: 'valid-token',
      isAuthenticated: true,
      expiresAt: new Date(Date.now() + 60000)
    };

    authStorage.setSession('expired', expiredSession);
    authStorage.setSession('valid', validSession);

    const cleanedUp = authStorage.cleanupExpiredSessions();
    
    assert.strictEqual(cleanedUp.length, 1);
    assert.ok(cleanedUp.includes('expired'));
    assert.ok(authStorage.hasSession('valid'));
    assert.ok(!authStorage.hasSession('expired'));
  });
});

console.log('All JWT auth manager tests completed!');

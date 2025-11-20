/**
 * WebSocket Connection Test
 * Tests various connection scenarios to diagnose issues
 */

import { WebSocketClient } from '../src/WebSocketClient.js';

class WebSocketConnectionTest {
    constructor() {
        this.testResults = [];
    }

    async runTests() {
        console.log('=== WebSocket Connection Tests ===');
        
        // Test 1: Basic connection to localhost:9002
        await this.testBasicConnection();
        
        // Test 2: Connection with different protocols
        await this.testProtocolConnection();
        
        // Test 3: Connection without protocols
        await this.testNoProtocolConnection();
        
        // Test 4: Test with different host formats
        await this.testHostFormats();
        
        // Test 5: Test connection timeout behavior
        await this.testConnectionTimeout();
        
        this.printResults();
    }

    async testBasicConnection() {
        console.log('\n--- Test 1: Basic Connection ---');
        
        return new Promise((resolve) => {
            const startTime = Date.now();
            const client = new WebSocketClient({
                url: 'ws://localhost:9002',
                debug: true,
                timeout: 5000,
                autoConnect: false
            });

            let connected = false;
            let error = null;

            client.on('open', () => {
                connected = true;
                const duration = Date.now() - startTime;
                console.log(`✅ Connected in ${duration}ms`);
                this.testResults.push({ test: 'Basic Connection', status: 'PASS', duration });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                error = err;
                console.log('❌ Connection error:', err);
                this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected && !error) {
                    console.log(`❌ Connection closed without success: ${event.code} - ${event.reason}`);
                    this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            // Start connection
            try {
                client.connect();
            } catch (err) {
                console.log('❌ Connection failed immediately:', err);
                this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: err.message });
                resolve();
            }

            // Force resolve after timeout
            setTimeout(() => {
                if (!connected && !error) {
                    console.log('❌ Connection timeout');
                    this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 6000);
        });
    }

    async testProtocolConnection() {
        console.log('\n--- Test 2: Protocol Connection ---');
        
        return new Promise((resolve) => {
            const client = new WebSocketClient({
                url: 'ws://localhost:9002',
                protocols: ['ur-webif-protocol'],
                debug: true,
                timeout: 5000,
                autoConnect: false
            });

            let connected = false;

            client.on('open', () => {
                connected = true;
                console.log('✅ Connected with protocol');
                this.testResults.push({ test: 'Protocol Connection', status: 'PASS' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                console.log('❌ Protocol connection error:', err);
                this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected) {
                    console.log(`❌ Protocol connection closed: ${event.code} - ${event.reason}`);
                    this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            try {
                client.connect();
            } catch (err) {
                console.log('❌ Protocol connection failed:', err);
                this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            }

            setTimeout(() => {
                if (!connected) {
                    console.log('❌ Protocol connection timeout');
                    this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 6000);
        });
    }

    async testNoProtocolConnection() {
        console.log('\n--- Test 3: No Protocol Connection ---');
        
        return new Promise((resolve) => {
            const client = new WebSocketClient({
                url: 'ws://localhost:9002',
                protocols: [],
                debug: true,
                timeout: 5000,
                autoConnect: false
            });

            let connected = false;

            client.on('open', () => {
                connected = true;
                console.log('✅ Connected without protocol');
                this.testResults.push({ test: 'No Protocol Connection', status: 'PASS' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                console.log('❌ No protocol connection error:', err);
                this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected) {
                    console.log(`❌ No protocol connection closed: ${event.code} - ${event.reason}`);
                    this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            try {
                client.connect();
            } catch (err) {
                console.log('❌ No protocol connection failed:', err);
                this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            }

            setTimeout(() => {
                if (!connected) {
                    console.log('❌ No protocol connection timeout');
                    this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 6000);
        });
    }

    async testHostFormats() {
        console.log('\n--- Test 4: Host Formats ---');
        
        const hosts = [
            'ws://localhost:9002',
            'ws://127.0.0.1:9002',
            'ws://0.0.0.0:9002'
        ];

        for (const host of hosts) {
            await this.testHostFormat(host);
        }
    }

    async testHostFormat(host) {
        console.log(`Testing ${host}...`);
        
        return new Promise((resolve) => {
            const client = new WebSocketClient({
                url: host,
                debug: false,
                timeout: 3000,
                autoConnect: false
            });

            let connected = false;

            client.on('open', () => {
                connected = true;
                console.log(`✅ Connected to ${host}`);
                this.testResults.push({ test: `Host: ${host}`, status: 'PASS' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                console.log(`❌ Error connecting to ${host}:`, err.message);
                this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected) {
                    console.log(`❌ Connection to ${host} closed: ${event.code}`);
                    this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            try {
                client.connect();
            } catch (err) {
                console.log(`❌ Failed to connect to ${host}:`, err.message);
                this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: err.message });
                resolve();
            }

            setTimeout(() => {
                if (!connected) {
                    console.log(`❌ Timeout connecting to ${host}`);
                    this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 4000);
        });
    }

    async testConnectionTimeout() {
        console.log('\n--- Test 5: Connection Timeout ---');
        
        return new Promise((resolve) => {
            const client = new WebSocketClient({
                url: 'ws://localhost:9999', // Non-existent port
                debug: false,
                timeout: 2000,
                autoConnect: false
            });

            let connected = false;
            let timedOut = false;

            client.on('open', () => {
                connected = true;
                console.log('❌ Unexpected connection to non-existent port');
                this.testResults.push({ test: 'Connection Timeout', status: 'FAIL', error: 'Unexpected success' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                if (!timedOut) {
                    console.log('Expected error for timeout test:', err.message);
                }
            });

            try {
                client.connect();
            } catch (err) {
                if (err.message.includes('timeout')) {
                    timedOut = true;
                    console.log('✅ Connection timeout working correctly');
                    this.testResults.push({ test: 'Connection Timeout', status: 'PASS' });
                    resolve();
                } else {
                    console.log('❌ Unexpected error:', err.message);
                    this.testResults.push({ test: 'Connection Timeout', status: 'FAIL', error: err.message });
                    resolve();
                }
            }

            setTimeout(() => {
                if (!connected && !timedOut) {
                    console.log('✅ Connection timeout working correctly');
                    this.testResults.push({ test: 'Connection Timeout', status: 'PASS' });
                    client.disconnect();
                    resolve();
                }
            }, 3000);
        });
    }

    printResults() {
        console.log('\n=== Test Results ===');
        
        const passed = this.testResults.filter(r => r.status === 'PASS').length;
        const failed = this.testResults.filter(r => r.status === 'FAIL').length;
        
        console.log(`Total: ${this.testResults.length}, Passed: ${passed}, Failed: ${failed}`);
        
        this.testResults.forEach(result => {
            const icon = result.status === 'PASS' ? '✅' : '❌';
            console.log(`${icon} ${result.test}: ${result.status}${result.error ? ` - ${result.error}` : ''}`);
        });

        // Recommendations
        console.log('\n=== Recommendations ===');
        if (failed === 0) {
            console.log('✅ All tests passed! WebSocket connection is working correctly.');
        } else {
            console.log('❌ Some tests failed. Check the following:');
            
            const failedTests = this.testResults.filter(r => r.status === 'FAIL');
            failedTests.forEach(test => {
                if (test.test.includes('Host:')) {
                    console.log(`- Try using a different host format than ${test.test.split(': ')[1]}`);
                }
                if (test.error && test.error.includes('1006')) {
                    console.log('- Error 1006 indicates connection was closed abnormally - check server logs');
                }
                if (test.error && test.error.includes('Timeout')) {
                    console.log('- Connection timeout - server may not be responding or firewall blocking');
                }
            });
            
            console.log('\nSuggested fixes:');
            console.log('1. Try using ws://localhost:9002 instead of ws://0.0.0.0:9002');
            console.log('2. Check if WebSocket server is running and accepting connections');
            console.log('3. Verify no firewall is blocking port 9002');
            console.log('4. Try connecting without protocols first');
        }
    }
}

// Run tests if this file is executed directly
if (typeof window !== 'undefined') {
    // Browser environment
    window.runWebSocketTests = () => {
        const tester = new WebSocketConnectionTest();
        return tester.runTests();
    };
    console.log('WebSocket tests loaded. Run window.runWebSocketTests() to start.');
}

export { WebSocketConnectionTest };

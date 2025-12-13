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
                this.testResults.push({ test: 'Basic Connection', status: 'PASS', duration });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                error = err;
                this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected && !error) {
                    this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            // Start connection
            try {
                client.connect();
            } catch (err) {
                this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: err.message });
                resolve();
            }

            // Force resolve after timeout
            setTimeout(() => {
                if (!connected && !error) {
                    this.testResults.push({ test: 'Basic Connection', status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 6000);
        });
    }

    async testProtocolConnection() {
        
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
                this.testResults.push({ test: 'Protocol Connection', status: 'PASS' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected) {
                    this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            try {
                client.connect();
            } catch (err) {
                this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            }

            setTimeout(() => {
                if (!connected) {
                    this.testResults.push({ test: 'Protocol Connection', status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 6000);
        });
    }

    async testNoProtocolConnection() {
        
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
                this.testResults.push({ test: 'No Protocol Connection', status: 'PASS' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected) {
                    this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            try {
                client.connect();
            } catch (err) {
                this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: err.message });
                resolve();
            }

            setTimeout(() => {
                if (!connected) {
                    this.testResults.push({ test: 'No Protocol Connection', status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 6000);
        });
    }

    async testHostFormats() {
        
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
                this.testResults.push({ test: `Host: ${host}`, status: 'PASS' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: err.message });
                resolve();
            });

            client.on('close', (event) => {
                if (!connected) {
                    this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: `Closed: ${event.code}` });
                }
                resolve();
            });

            try {
                client.connect();
            } catch (err) {
                this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: err.message });
                resolve();
            }

            setTimeout(() => {
                if (!connected) {
                    this.testResults.push({ test: `Host: ${host}`, status: 'FAIL', error: 'Timeout' });
                    client.disconnect();
                    resolve();
                }
            }, 4000);
        });
    }

    async testConnectionTimeout() {
        
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
                this.testResults.push({ test: 'Connection Timeout', status: 'FAIL', error: 'Unexpected success' });
                client.disconnect();
                resolve();
            });

            client.on('error', (err) => {
                if (!timedOut) {
                }
            });

            try {
                client.connect();
            } catch (err) {
                if (err.message.includes('timeout')) {
                    timedOut = true;
                    this.testResults.push({ test: 'Connection Timeout', status: 'PASS' });
                    resolve();
                } else {
                    this.testResults.push({ test: 'Connection Timeout', status: 'FAIL', error: err.message });
                    resolve();
                }
            }

            setTimeout(() => {
                if (!connected && !timedOut) {
                    this.testResults.push({ test: 'Connection Timeout', status: 'PASS' });
                    client.disconnect();
                    resolve();
                }
            }, 3000);
        });
    }

    printResults() {
        
        const passed = this.testResults.filter(r => r.status === 'PASS').length;
        const failed = this.testResults.filter(r => r.status === 'FAIL').length;
        
        
        this.testResults.forEach(result => {
            const icon = result.status === 'PASS' ? '✅' : '❌';
        });

        // Recommendations
        if (failed === 0) {
        } else {
            
            const failedTests = this.testResults.filter(r => r.status === 'FAIL');
            failedTests.forEach(test => {
                if (test.test.includes('Host:')) {
                }
                if (test.error && test.error.includes('1006')) {
                }
                if (test.error && test.error.includes('Timeout')) {
                }
            });
            
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
}

export { WebSocketConnectionTest };

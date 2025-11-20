/**
 * Simple WebSocket test using browser WebSocket API
 * Tests basic WebSocket connectivity to localhost:9002
 */

// Use global WebSocket in browser or create a mock for Node.js testing
const WebSocket = typeof WebSocket !== 'undefined' ? WebSocket : class {
    constructor(url) {
        this.url = url;
        this.readyState = 0;
        setTimeout(() => {
            this.onerror(new Error('WebSocket not available in this environment'));
        }, 100);
    }
    send() {}
    close() {}
    terminate() {}
};

function testConnection(url, description) {
    return new Promise((resolve) => {
        console.log(`\n--- Testing: ${description} ---`);
        console.log(`URL: ${url}`);
        
        const startTime = Date.now();
        const ws = new WebSocket(url);
        
        let connected = false;
        let error = null;
        
        const timeout = setTimeout(() => {
            if (!connected) {
                console.log('❌ Connection timeout (5 seconds)');
                ws.terminate();
                resolve({ success: false, error: 'Timeout', duration: 5000 });
            }
        }, 5000);
        
        ws.on('open', () => {
            connected = true;
            const duration = Date.now() - startTime;
            console.log(`✅ Connected successfully in ${duration}ms`);
            
            // Send a test message
            ws.send(JSON.stringify({ type: 'test', timestamp: Date.now() }));
            console.log('📤 Sent test message');
            
            setTimeout(() => {
                ws.close();
                clearTimeout(timeout);
                resolve({ success: true, duration });
            }, 1000);
        });
        
        ws.on('message', (data) => {
            console.log('📥 Received message:', data.toString());
        });
        
        ws.on('error', (err) => {
            error = err;
            console.log('❌ Connection error:', err.message);
            clearTimeout(timeout);
            resolve({ success: false, error: err.message });
        });
        
        ws.on('close', (code, reason) => {
            if (!connected && !error) {
                console.log(`❌ Connection closed without success: ${code} - ${reason}`);
                clearTimeout(timeout);
                resolve({ success: false, error: `Closed: ${code}` });
            }
        });
    });
}

async function runAllTests() {
    console.log('=== WebSocket Connection Tests ===');
    console.log('Testing WebSocket connectivity to localhost:9002\n');
    
    const tests = [
        {
            url: 'ws://localhost:9002',
            description: 'Basic connection to localhost:9002'
        },
        {
            url: 'ws://localhost:9002/',
            description: 'Connection with trailing slash'
        },
        {
            url: 'ws://127.0.0.1:9002',
            description: 'Connection to 127.0.0.1:9002'
        },
        {
            url: 'ws://0.0.0.0:9002',
            description: 'Connection to 0.0.0.0:9002'
        },
        {
            url: 'ws://localhost:9002',
            description: 'Connection with protocol',
            protocols: ['ur-webif-protocol']
        }
    ];
    
    const results = [];
    
    for (const test of tests) {
        try {
            const result = await testConnection(test.url, test.description);
            results.push({ ...test, ...result });
        } catch (error) {
            console.log('❌ Test failed with exception:', error.message);
            results.push({ ...test, success: false, error: error.message });
        }
        
        // Small delay between tests
        await new Promise(resolve => setTimeout(resolve, 1000));
    }
    
    console.log('\n=== Test Results Summary ===');
    
    const passed = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;
    
    console.log(`Total tests: ${results.length}`);
    console.log(`Passed: ${passed} ✅`);
    console.log(`Failed: ${failed} ❌\n`);
    
    results.forEach(result => {
        const status = result.success ? '✅ PASS' : '❌ FAIL';
        const duration = result.duration ? ` (${result.duration}ms)` : '';
        const error = result.error ? ` - ${result.error}` : '';
        console.log(`${status} ${result.description}${duration}${error}`);
    });
    
    console.log('\n=== Recommendations ===');
    
    if (passed > 0) {
        console.log('✅ WebSocket server is accessible!');
        
        const bestResult = results.find(r => r.success);
        if (bestResult) {
            console.log(`\nRecommended URL for frontend: ${bestResult.url}`);
            console.log('Update your source.js to use this URL:');
            console.log(`url: '${bestResult.url}',`);
        }
        
        if (failed > 0) {
            console.log('\n⚠️  Some connection methods failed, but at least one worked.');
            console.log('The failing methods might be due to:');
            console.log('- Host binding issues (0.0.0.0 vs localhost)');
            console.log('- Protocol requirements');
            console.log('- Network configuration');
        }
    } else {
        console.log('❌ All connection attempts failed!');
        console.log('\nPossible issues:');
        console.log('1. WebSocket server is not running properly');
        console.log('2. Firewall is blocking port 9002');
        console.log('3. Server requires specific protocols or authentication');
        console.log('4. Server is only listening on specific interfaces');
        
        console.log('\nTroubleshooting steps:');
        console.log('1. Check server logs for connection attempts');
        console.log('2. Verify server is listening on the correct interface');
        console.log('3. Test with a WebSocket client tool');
        console.log('4. Check for any required headers or protocols');
    }
}

// Auto-run tests when loaded
console.log('Starting WebSocket connection tests...');
runAllTests().catch(console.error);

export { testConnection, runAllTests };

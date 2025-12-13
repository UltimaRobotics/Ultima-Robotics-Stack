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
        
        const startTime = Date.now();
        const ws = new WebSocket(url);
        
        let connected = false;
        let error = null;
        
        const timeout = setTimeout(() => {
            if (!connected) {
                ws.terminate();
                resolve({ success: false, error: 'Timeout', duration: 5000 });
            }
        }, 5000);
        
        ws.on('open', () => {
            connected = true;
            const duration = Date.now() - startTime;
            
            // Send a test message
            ws.send(JSON.stringify({ type: 'test', timestamp: Date.now() }));
            
            setTimeout(() => {
                ws.close();
                clearTimeout(timeout);
                resolve({ success: true, duration });
            }, 1000);
        });
        
        ws.on('message', (data) => {
        });
        
        ws.on('error', (err) => {
            error = err;
            clearTimeout(timeout);
            resolve({ success: false, error: err.message });
        });
        
        ws.on('close', (code, reason) => {
            if (!connected && !error) {
                clearTimeout(timeout);
                resolve({ success: false, error: `Closed: ${code}` });
            }
        });
    });
}

async function runAllTests() {
    
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
            results.push({ ...test, success: false, error: error.message });
        }
        
        // Small delay between tests
        await new Promise(resolve => setTimeout(resolve, 1000));
    }
    
    
    const passed = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;
    
    
    results.forEach(result => {
        const status = result.success ? '✅ PASS' : '❌ FAIL';
        const duration = result.duration ? ` (${result.duration}ms)` : '';
        const error = result.error ? ` - ${result.error}` : '';
    });
    
    
    if (passed > 0) {
        
        const bestResult = results.find(r => r.success);
        if (bestResult) {
        }
        
        if (failed > 0) {
        }
    } else {
        
    }
}

// Auto-run tests when loaded
runAllTests().catch(console.error);

export { testConnection, runAllTests };

/**
 * WebSocket Client for UR Web Interface
 * Provides WebSocket connectivity with reconnection and authentication
 */

class WebSocketClient {
    constructor(options = {}) {
        this.url = options.url || `ws://${window.location.host}/ws`;
        this.protocols = options.protocols || [];
        this.reconnectInterval = options.reconnectInterval || 5000;
        this.maxReconnectAttempts = options.maxReconnectAttempts || 10;
        this.heartbeatInterval = options.heartbeatInterval || 30000;
        
        this.ws = null;
        this.reconnectAttempts = 0;
        this.heartbeatTimer = null;
        this.isConnected = false;
        this.eventListeners = new Map();
        
        this.init();
    }
    
    init() {
        console.log('[WEBSOCKET-CLIENT] Initializing WebSocket client...');
        this.connect();
    }
    
    connect() {
        try {
            console.log(`[WEBSOCKET-CLIENT] Connecting to ${this.url}...`);
            
            this.ws = new WebSocket(this.url, this.protocols);
            
            this.ws.onopen = () => this.handleOpen();
            this.ws.onmessage = (event) => this.handleMessage(event);
            this.ws.onclose = (event) => this.handleClose(event);
            this.ws.onerror = (error) => this.handleError(error);
            
        } catch (error) {
            console.error('[WEBSOCKET-CLIENT] Connection error:', error);
            this.scheduleReconnect();
        }
    }
    
    handleOpen() {
        console.log('[WEBSOCKET-CLIENT] WebSocket connected');
        this.isConnected = true;
        this.reconnectAttempts = 0;
        
        // Start heartbeat
        this.startHeartbeat();
        
        // Emit open event
        this.emit('open');
    }
    
    handleMessage(event) {
        try {
            const data = JSON.parse(event.data);
            this.emit('message', data);
        } catch (error) {
            console.error('[WEBSOCKET-CLIENT] Failed to parse message:', error);
            this.emit('message', event.data);
        }
    }
    
    handleClose(event) {
        console.log(`[WEBSOCKET-CLIENT] WebSocket closed: ${event.code} - ${event.reason}`);
        this.isConnected = false;
        this.stopHeartbeat();
        
        // Emit close event
        this.emit('close', event);
        
        // Schedule reconnection if not a normal closure
        if (event.code !== 1000) {
            this.scheduleReconnect();
        }
    }
    
    handleError(error) {
        console.error('[WEBSOCKET-CLIENT] WebSocket error:', error);
        this.emit('error', error);
    }
    
    scheduleReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error('[WEBSOCKET-CLIENT] Max reconnection attempts reached');
            this.emit('maxReconnectAttemptsReached');
            return;
        }
        
        this.reconnectAttempts++;
        console.log(`[WEBSOCKET-CLIENT] Scheduling reconnection attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts} in ${this.reconnectInterval}ms`);
        
        setTimeout(() => {
            this.connect();
        }, this.reconnectInterval);
    }
    
    startHeartbeat() {
        this.stopHeartbeat();
        
        this.heartbeatTimer = setInterval(() => {
            if (this.isConnected && this.ws.readyState === WebSocket.OPEN) {
                this.send({ type: 'heartbeat', timestamp: Date.now() }).catch(error => {
                    console.error('[WEBSOCKET-CLIENT] Heartbeat failed:', error);
                });
            }
        }, this.heartbeatInterval);
    }
    
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
    }
    
    async send(data) {
        if (!this.isConnected || this.ws.readyState !== WebSocket.OPEN) {
            throw new Error('WebSocket is not connected');
        }
        
        try {
            const message = typeof data === 'string' ? data : JSON.stringify(data);
            this.ws.send(message);
            console.log('[WEBSOCKET-CLIENT] Message sent:', data);
        } catch (error) {
            console.error('[WEBSOCKET-CLIENT] Failed to send message:', error);
            throw error;
        }
    }
    
    disconnect() {
        console.log('[WEBSOCKET-CLIENT] Disconnecting WebSocket...');
        this.stopHeartbeat();
        
        if (this.ws) {
            this.ws.close(1000, 'Client disconnect');
            this.ws = null;
        }
        
        this.isConnected = false;
    }
    
    on(event, callback) {
        if (!this.eventListeners.has(event)) {
            this.eventListeners.set(event, []);
        }
        this.eventListeners.get(event).push(callback);
    }
    
    off(event, callback) {
        if (this.eventListeners.has(event)) {
            const listeners = this.eventListeners.get(event);
            const index = listeners.indexOf(callback);
            if (index > -1) {
                listeners.splice(index, 1);
            }
        }
    }
    
    emit(event, data) {
        if (this.eventListeners.has(event)) {
            this.eventListeners.get(event).forEach(callback => {
                try {
                    callback(data);
                } catch (error) {
                    console.error(`[WEBSOCKET-CLIENT] Error in ${event} listener:`, error);
                }
            });
        }
    }
    
    // Utility methods
    isConnecting() {
        return this.ws && this.ws.readyState === WebSocket.CONNECTING;
    }
    
    getReadyState() {
        return this.ws ? this.ws.readyState : WebSocket.CLOSED;
    }
    
    getReconnectAttempts() {
        return this.reconnectAttempts;
    }
}

// Export for use in other modules
export { WebSocketClient };
export default WebSocketClient;

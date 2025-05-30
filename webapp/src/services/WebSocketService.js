/**
 * WebSocketService.js
 * A reusable WebSocket service for NogasmLink
 */
class WebSocketService {
    constructor() {
        this.websocket = null;
        this.reconnectAttempts = 0;
        this.reconnectTimeout = null;
        this.maxReconnectDelay = 5000; // Maximum reconnect delay in ms
        this.status = 'disconnected';

        // Callbacks for different message types
        this.callbacks = {
            'ble_status': [],
            'arousal_status': [],
            'status': [] // For connection status updates
        };

        // Bind methods to this instance
        this.connect = this.connect.bind(this);
        this.disconnect = this.disconnect.bind(this);
        this.onWebSocketOpen = this.onWebSocketOpen.bind(this);
        this.onWebSocketClose = this.onWebSocketClose.bind(this);
        this.onWebSocketError = this.onWebSocketError.bind(this);
        this.onWebSocketMessage = this.onWebSocketMessage.bind(this);
    }

    /**
     * Connect to the WebSocket server
     * @returns {Promise} Resolves when connected, rejects on failure
     */
    connect() {
        return new Promise((resolve, reject) => {
            if (this.websocket &&
                (this.websocket.readyState === WebSocket.OPEN ||
                    this.websocket.readyState === WebSocket.CONNECTING)) {
                // Already connected or connecting
                if (this.websocket.readyState === WebSocket.OPEN) {
                    resolve();
                }
                return;
            }

            // Determine WebSocket URL (using same host/port as current page)
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;

            this.updateStatus('connecting');

            try {
                this.websocket = new WebSocket(wsUrl);

                // Setup one-time event handler for this connection attempt
                this.websocket.onopen = () => {
                    this.onWebSocketOpen();
                    resolve();
                };

                this.websocket.onerror = (error) => {
                    this.onWebSocketError(error);
                    reject(error);
                };

                // Setup persistent event handlers
                this.websocket.onclose = this.onWebSocketClose;
                this.websocket.onmessage = this.onWebSocketMessage;
            } catch (error) {
                this.updateStatus('error');
                reject(error);
            }
        });
    }

    /**
     * Disconnect from the WebSocket server
     */
    disconnect() {
        if (this.reconnectTimeout) {
            clearTimeout(this.reconnectTimeout);
            this.reconnectTimeout = null;
        }

        if (this.websocket) {
            // Remove event handlers to prevent memory leaks
            this.websocket.onopen = null;
            this.websocket.onclose = null;
            this.websocket.onerror = null;
            this.websocket.onmessage = null;

            // Close connection if still open
            if (this.websocket.readyState === WebSocket.OPEN ||
                this.websocket.readyState === WebSocket.CONNECTING) {
                this.websocket.close();
            }

            this.websocket = null;
            this.updateStatus('disconnected');
        }
    }

    /**
     * Subscribe to a specific message type
     * @param {string} type - The message type to subscribe to (e.g., 'ble_status')
     * @param {function} callback - The callback function to invoke when a message of this type is received
     * @returns {function} Unsubscribe function
     */
    subscribe(type, callback) {
        if (!this.callbacks[type]) {
            this.callbacks[type] = [];
        }

        this.callbacks[type].push(callback);

        // Return unsubscribe function
        return () => {
            const index = this.callbacks[type].indexOf(callback);
            if (index !== -1) {
                this.callbacks[type].splice(index, 1);
            }
        };
    }

    /**
     * Get the current WebSocket connection status
     * @returns {string} The current status ('connected', 'connecting', 'disconnected', 'error')
     */
    getStatus() {
        return this.status;
    }

    /**
     * @returns a CSS class for the current status
     */
    getWebSocketStatusClass() {
        if (this.status === 'connected') {
            return 'bg-success';
        } else if (this.status === 'connecting' || this.status.startsWith('reconnecting:')) {
            return 'bg-warning';
        } else {
            return 'bg-danger';
        }
    }

    /**
     * @returns a display name for the status
     */
    formatWebSocketStatus() {
        if (this.status === 'connected') {
            return 'Websocket Connected';
        } else if (this.status === 'connecting') {
            return 'Websocket Connecting';
        } else if (this.status === 'disconnected') {
            return 'Websocket Disconnected';
        } else if (this.status === 'error') {
            return 'Websocket Error';
        } else if (this.status.startsWith('reconnecting:')) {
            const delay = parseInt(this.status.split(':')[1]);
            return `Websocket Reconnecting in ${Math.round(delay / 1000)}s`;
        }
        return this.status;
    }

    // Private methods

    /**
     * Update the WebSocket status and notify subscribers
     * @private
     * @param {string} status - The new status
     */
    updateStatus(status) {
        this.status = status;

        // Notify status subscribers
        if (this.callbacks['status']) {
            this.callbacks['status'].forEach(callback => {
                try {
                    callback(status);
                } catch (error) {
                    console.error('Error in status callback:', error);
                }
            });
        }
    }

    /**
     * Handle WebSocket open event
     * @private
     */
    onWebSocketOpen() {
        this.updateStatus('connected');
        this.reconnectAttempts = 0;
    }

    /**
     * Handle WebSocket close event
     * @private
     */
    // eslint-disable-next-line no-unused-vars
    onWebSocketClose(event) {
        this.updateStatus('disconnected');

        // Try to reconnect with exponential backoff
        this.scheduleReconnect();
    }

    /**
     * Handle WebSocket error event
     * @private
     */
    // eslint-disable-next-line no-unused-vars
    onWebSocketError(error) {
        this.updateStatus('error');

        // Close the socket and try to reconnect
        this.disconnect();
        this.scheduleReconnect();
    }

    /**
     * Handle WebSocket message event
     * @private
     */
    onWebSocketMessage(event) {
        try {
            const data = JSON.parse(event.data);
            const type = data.type;

            // Call the appropriate callbacks based on message type
            if (type && this.callbacks[type]) {
                this.callbacks[type].forEach(callback => {
                    try {
                        callback(data);
                    } catch (error) {
                        console.error(`Error in ${type} callback:`, error);
                    }
                });
            }
        } catch (error) {
            console.error('Error processing WebSocket message:', error);
        }
    }

    /**
     * Schedule a reconnection attempt with exponential backoff
     * @private
     */
    scheduleReconnect() {
        if (this.reconnectTimeout) {
            clearTimeout(this.reconnectTimeout);
        }

        // Calculate backoff delay (100ms, 200ms, 400ms, etc. up to maxReconnectDelay)
        const delay = Math.min(100 * Math.pow(2, this.reconnectAttempts), this.maxReconnectDelay);
        this.reconnectAttempts++;

        this.updateStatus(`reconnecting:${Math.round(delay)}`);

        // Schedule reconnect
        this.reconnectTimeout = setTimeout(() => {
            this.connect().catch(error => {
                console.error('Reconnection attempt failed:', error);
            });
        }, delay);
    }
}

// Create a singleton instance
const websocketService = new WebSocketService();

export default websocketService;
<template>
    <div class="card">
        <div class="card-header">
            <div class="card-title">
                <i class="fas fa-search"></i>
                Scanner
            </div>
            <div class="scan-status">
                <span v-if="connecting" class="status-indicator status-connecting">
                    <span class="spinner"/>
                    {{ connectionStateMessage }}
                </span>
                <button v-else @click="onStartScan" class="btn btn-primary" :disabled="scanning || connecting">
                    <span v-if="scanning" class="spinner"/>
                    <i v-else class="fas fa-solid fa-magnifying-glass"/>
                    {{ scanning ? 'Scanning...' : 'Scan for Devices' }}
                </button>
            </div>
        </div>

        <div class="device-list">
            <div v-if="devices.length === 0" class="empty-state">
                <div class="empty-icon">
                    <i class="fas fa-broadcast-tower" v-if="!scanning"></i>
                    <div v-else class="scanning-animation">
                        <i class="fas fa-wifi"></i>
                    </div>
                </div>
                <p v-if="scanning">Searching for devices...</p>
                <p v-else>No devices found. Press Scan to search for devices.</p>
            </div>
            <div v-else class="devices-grid">
                <div v-for="device in devices" :key="device.address"
                     class="device-item"
                     :class="{
                        'device-connected': device.connected,
                        'device-connecting': isConnectingTo(device.address),
                        'device-disabled': !canConnect(device)
                     }"
                     @click="canConnect(device) && onConnectDevice(device)">

                    <div class="device-info">
                        <div class="device-icon">
                            <i class="fas fa-circle-nodes"></i>
                        </div>
                        <div class="device-details">
                            <h4>{{ device.name }}</h4>
                            <p>{{ device.address }}</p>
                            <span class="device-type">{{ device.addressType }}</span>
                        </div>
                    </div>

                    <div class="device-actions">
                        <span v-if="device.connected" class="status-badge status-connected">
                            <i class="fas fa-check-circle"></i>
                            Connected
                        </span>
                        <div v-else-if="isConnectingTo(device.address)" class="status-badge status-connecting">
                            <div class="spinner"></div>
                            Connecting
                        </div>
                        <button v-else class="btn btn-primary btn-connect"
                                :disabled="!canConnect(device)"
                                @click.stop="onConnectDevice(device)">
                            <i class="fas fa-link"></i>
                            Connect
                        </button>
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>

<script>
export default {
    name: 'DeviceScanner',
    props: {
        devices: {
            type: Array,
            required: true
        },
        scanning: {
            type: Boolean,
            default: false
        },
        connectionStateString: {
            type: String,
            default: 'IDLE'
        }
    },
    data() {
        return {
            localConnectingAddress: ''
        }
    },
    computed: {
        connecting() {
            return ['CONNECTING', 'FINDING_SERVICE', 'RECONNECTING'].includes(this.connectionStateString);
        },
        connectionStateMessage() {
            switch (this.connectionStateString) {
                case 'CONNECTING':
                    return 'Connecting to device...';
                case 'FINDING_SERVICE':
                    return 'Setting up device...';
                case 'RECONNECTING':
                    return 'Trying to reconnect...';
                default:
                    return 'Processing...';
            }
        },
        anyDeviceConnected() {
            return this.devices.some(device => device.connected);
        }
    },
    watch: {
        connectionStateString(newState) {
            if (newState === 'CONNECTED' || newState === 'FAILED' || newState === 'IDLE') {
                this.localConnectingAddress = '';
            }
        }
    },
    methods: {
        canConnect(device) {
            return !device.connected && !this.anyDeviceConnected && !this.connecting;
        },

        onStartScan() {
            this.$emit('start-scan');
        },

        onConnectDevice(device) {
            if (!this.canConnect(device)) {
                return;
            }

            this.localConnectingAddress = device.address;
            this.$emit('connect-device', device.address);
        },

        isConnectingTo(address) {
            return this.localConnectingAddress === address;
        }
    }
}
</script>

<style scoped>
.scan-status {
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.status-connecting {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    color: var(--text-secondary);
    font-size: 0.875rem;
}

.device-list {
    min-height: 200px;
}

.empty-state {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 3rem 1rem;
    color: var(--text-secondary);
    text-align: center;
}

.empty-icon {
    width: 80px;
    height: 80px;
    background: var(--surface-elevated);
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 1rem;
    border: 1px solid var(--border);
}

.empty-icon i {
    font-size: 2rem;
    color: var(--text-secondary);
}

.scanning-animation {
    animation: scanPulse 2s ease-in-out infinite;
}

@keyframes scanPulse {
    0%, 100% {
        transform: scale(1);
        opacity: 0.7;
    }
    50% {
        transform: scale(1.1);
        opacity: 1;
    }
}

.devices-grid {
    display: grid;
    gap: 1rem;
}

.device-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 1.5rem;
    background: var(--surface-elevated);
    border: 1px solid var(--border);
    border-radius: var(--border-radius);
    transition: all 0.2s ease;
    cursor: pointer;
}

.device-item:hover:not(.device-disabled) {
    background: #f1f5f9;
    transform: translateX(4px);
    box-shadow: var(--shadow-sm);
}

.device-item.device-connected {
    background: linear-gradient(135deg, #d4edda 0%, #c3e6cb 100%);
    border-color: #28a745;
}

.device-item.device-connecting {
    background: linear-gradient(135deg, #fff3cd 0%, #ffeaa7 100%);
    border-color: #ffc107;
}

.device-item.device-disabled {
    cursor: not-allowed;
    opacity: 0.6;
    background: #f8f9fa;
}

.device-info {
    display: flex;
    align-items: center;
    flex: 1;
}

.device-icon {
    width: 50px;
    height: 50px;
    background: var(--primary-gradient);
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    color: white;
    margin-right: 1rem;
    font-size: 1.25rem;
}

.device-details {
    flex: 1;
}

.device-details h4 {
    font-size: 1.125rem;
    font-weight: 600;
    margin-bottom: 0.25rem;
    color: var(--text-primary);
}

.device-details p {
    font-size: 0.875rem;
    color: var(--text-secondary);
    margin-bottom: 0.25rem;
    font-family: monospace;
}

.device-type {
    font-size: 0.75rem;
    background: var(--surface);
    color: var(--text-secondary);
    padding: 0.25rem 0.5rem;
    border-radius: 4px;
    border: 1px solid var(--border);
}

.device-actions {
    display: flex;
    align-items: center;
}

.status-badge {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.5rem 1rem;
    border-radius: 50px;
    font-size: 0.875rem;
    font-weight: 500;
}

.status-connected {
    background: var(--success-gradient);
    color: white;
}

.btn-connect {
    padding: 0.5rem 1rem;
    font-size: 0.875rem;
}

@media (max-width: 768px) {
    .device-item {
        flex-direction: column;
        align-items: stretch;
        gap: 1rem;
    }

    .device-info {
        justify-content: center;
        text-align: center;
    }

    .device-actions {
        justify-content: center;
    }
}
</style>
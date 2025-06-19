<template>
    <div class="app-container">
        <div class="header">
            <h1>
                <img src="/favicon.svg" alt="NogasmLink" height="40" width="40">
                NogasmLink
            </h1>
            <p>Intelligent Arousal Management System</p>
        </div>

        <div class="dashboard">
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <i class="fas fa-wifi"></i>
                        Status
                    </div>
                    <div class="status-indicator" :class="websocketService.getWebSocketStatusClass()">
                        <i class="fas fa-circle"></i>
                        {{ websocketService.formatWebSocketStatus() }}
                    </div>
                </div>

                <div class="metric-grid">
                    <div class="metric">
                        <div class="metric-value">
                            {{ status.wifi.connected ? formatSignalStrength(status.wifi.rssi) : 'Disconnected' }}
                        </div>
                        <div class="metric-label">WiFi Signal</div>
                    </div>
                    <div class="metric">
                        <div class="metric-value">
                            <span class="status-badge" :class="getBleStatusClass()">{{
                                    status.ble.stateString || 'IDLE'
                                }}</span>
                        </div>
                        <div class="metric-label">Bluetooth Status</div>
                    </div>
                </div>

                <div v-if="status.wifi.connected" class="connection-info">
                    <p class="text-secondary"><strong>Network: </strong>{{ status.wifi.ssid }}</p>
                    <p class="text-secondary"><strong>IP Address: </strong>{{ status.wifi.ip }}</p>
                </div>

                <div v-if="status.ble.connected" class="device-item">
                    <div class="device-info">
                        <div class="device-icon">
                            <i class="fas fa-circle-nodes"></i>
                        </div>
                        <div class="device-details">
                            <h4>{{ status.device.name }}</h4>
                            <p>
                                {{ status.device.address }} • {{
                                    status.device.firmwareVersion
                                        ? 'v' + (Number(status.device.firmwareVersion) / 100).toFixed(2)
                                        : ''
                                }}
                            </p>
                            <div class="device-meta-info">
                                <i class="fas fa-battery-three-quarters"></i>
                                {{ status.device.battery || '--' }}%
                                <i class="fas fa-signal"></i>
                                {{ formatSignalStrength(status.device.rssi) }}
                            </div>
                        </div>
                    </div>
                </div>

                <config-settings @send-notification="notify"/>
            </div>

            <nogasm-update/>

            <device-control v-if="status.ble.connected"
                            :battery-level="status.device.battery"
                            :device-model="status.device.model"
                            :device-firmware="status.device.firmwareVersion"
                            @set-vibration="setVibrationLevel"
                            @set-rotation="setRotationLevel"
                            @change-rotation-direction="changeRotationDirection"
                            @set-air-level="setAirLevel"
                            @adjust-air="adjustAir"
                            @power-off="powerOffDevice"
                            @disconnect="onDisconnect"
                            @send-notification="notify"/>
            <device-scanner v-else
                            :devices="devices"
                            :scanning="status.ble.scanning"
                            :connection-state-string="status.ble.stateString"
                            @start-scan="startScan"
                            @connect-device="connectToDevice"
                            @send-notification="notify"/>

            <arousal-control class="full-width" @send-notification="notify"/>
            <arousal-tracker class="full-width" @send-notification="notify"/>
        </div>

        <div class="notifications">
            <div v-for="notification in notifications" :key="notification.id"
                 class="notification" :class="notification.type">
                <i class="fas fa-circle-info"></i>
                {{ notification.text }}
            </div>
        </div>
    </div>
</template>

<style>
@import './assets/nogasmlink-theme.css';
</style>

<script>
import DeviceScanner from './components/DeviceScanner.vue'
import DeviceControl from './components/DeviceControl.vue'
import ConfigSettings from './components/ConfigSettings.vue'
import ArousalControl from './components/ArousalControl.vue'
import ArousalTracker from './components/ArousalTracker.vue'
import websocketService from './services/WebSocketService.js'
import NogasmUpdate from "@/components/NogasmUpdate.vue";

export default {
    name: 'App',
    components: {
        NogasmUpdate,
        DeviceScanner,
        DeviceControl,
        ConfigSettings,
        ArousalControl,
        ArousalTracker
    },
    data() {
        return {
            status: {
                wifi: {
                    connected: false,
                    ssid: '',
                    ip: '',
                    rssi: -1
                },
                ble: {
                    scanning: false,
                    connected: false,
                    stateString: ''
                },
                device: {
                    name: '',
                    address: '',
                    addressType: '',
                    battery: -1,
                    firmwareVersion: '',
                    model: '',
                    rssi: -1
                }
            },
            devices: [],
            lastNotificationId: 0,
            notifications: [],
            lastStateString: null,
            unsubscribeFunctions: [],
            websocketService
        }
    },
    mounted() {
        this.initWebSocket();
        this.fetchStatus();
        this.fetchDevices();
    },
    beforeUnmount() {
        this.unsubscribeFunctions.forEach(unsubscribe => unsubscribe());
    },
    methods: {
        initWebSocket() {
            websocketService.connect().catch(() => {
                this.notify('Failed to connect to WebSocket', 'error');
            });

            this.unsubscribeFunctions.push(
                websocketService.subscribe('ble_status', (data) => {
                    this.updateBleStatus(data);
                })
            );
        },

        formatSignalStrength(strength) {
            strength = Math.abs(strength);
            if (strength < 70) {
                return 'Excellent';
            } else if (strength >= 70 && strength < 85) {
                return 'Good';
            } else if (strength >= 85 && strength < 100) {
                return 'Fair';
            } else if (strength >= 100 && strength < 115) {
                return 'Poor';
            } else {
                return 'Bad signal (' + strength + ')';
            }
        },

        notify(message, type = 'info') {
            this.notifications.push({
                id: ++this.lastNotificationId,
                text: message,
                type: type
            });

            const lastId = this.lastNotificationId;
            setTimeout(() => {
                const index = this.notifications.map(n => n.id).indexOf(lastId);
                this.notifications.splice(index, 1);
            }, 4000);
        },

        updateBleStatus(data) {
            this.status.ble.scanning = data.scanning;
            this.status.ble.connected = data.connected;
            this.status.ble.stateString = data.stateString;
            this.status.wifi.rssi = data.wifi.rssi;

            if (data.device) {
                this.status.device.name = data.device.name;
                this.status.device.address = data.device.address;
                this.status.device.addressType = data.device.addressType;
                this.status.device.firmwareVersion = data.device.firmwareVersion;
                this.status.device.battery = data.device.battery;
                this.status.device.model = data.device.model;
                this.status.device.rssi = data.device.rssi;
            }

            const wasScanning = this.status.ble.scanning;
            if (wasScanning) {
                this.fetchDevices();
            }

            if (this.lastStateString !== this.status.ble.stateString) {
                // If we've reached a terminal state, reset connecting address
                if (this.status.ble.stateString === 'IDLE' || this.status.ble.stateString === 'CONNECTED' || this.status.ble.stateString === 'FAILED') {
                    // and fetch the device list again
                    this.fetchDevices();
                }
            }

            this.lastStateString = data.stateString;
        },

        async fetchStatus() {
            try {
                const response = await fetch('/api/status');
                if (!response.ok) {
                    throw new Error('Failed to fetch status');
                }

                const data = await response.json();

                this.status.wifi = data.wifi;
                this.status.ble = data.ble;

                if (data.device) {
                    this.status.device = data.device;
                }
            } catch (error) {
                this.notify('Error fetching status', 'error');
            }
        },

        async fetchDevices() {
            try {
                const response = await fetch('/api/devices');
                if (!response.ok) {
                    throw new Error('Failed to fetch devices');
                }

                this.devices = await response.json();
            } catch (error) {
                this.notify('Error fetching devices', 'error');
            }
        },

        async startScan() {
            try {
                const response = await fetch('/api/scan');
                if (!response.ok) {
                    throw new Error('Failed to start scan');
                }
            } catch (error) {
                this.notify('Error starting scan', 'error');
            }
        },

        async connectToDevice(deviceAddress) {
            try {
                const response = await fetch('/api/connect', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({address: deviceAddress})
                });

                if (!response.ok) {
                    throw new Error('Failed to connect to device');
                }
            } catch (error) {
                this.notify('Error connecting to device', 'error');
            }
        },

        async onDisconnect() {
            try {
                const response = await fetch('/api/disconnect', {
                    method: 'POST'
                });

                if (!response.ok) {
                    throw new Error('Failed to disconnect device');
                }
            } catch (error) {
                this.notify('Error disconnecting device', 'error');
            }
        },

        async setVibrationLevel(level) {
            try {
                const response = await fetch('/api/vibrate', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({level: parseInt(level)})
                });

                if (!response.ok) {
                    throw new Error('Failed to set vibration');
                }
            } catch (error) {
                this.notify('Error setting vibration', 'error');
            }
        },

        async setRotationLevel(level) {
            try {
                const response = await fetch('/api/rotate', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({level: parseInt(level)})
                });

                if (!response.ok) {
                    throw new Error('Failed to set rotation');
                }
            } catch (error) {
                this.notify('Error setting rotation', 'error');
            }
        },

        async changeRotationDirection() {
            try {
                const response = await fetch('/api/rotate-direction', {
                    method: 'POST'
                });

                if (!response.ok) {
                    throw new Error('Failed to change rotation direction');
                }
            } catch (error) {
                this.notify('Error changing rotation direction', 'error');
            }
        },

        async setAirLevel(level) {
            try {
                const response = await fetch('/api/air-level', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({level: parseInt(level)})
                });

                if (!response.ok) {
                    throw new Error('Failed to set air level');
                }
            } catch (error) {
                this.notify('Error setting air level', 'error');
            }
        },

        async adjustAir(inflate, amount) {
            try {
                const response = await fetch('/api/adjust-air', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({
                        inflate: inflate,
                        amount: parseInt(amount)
                    })
                });

                if (!response.ok) {
                    throw new Error('Failed to adjust air');
                }
            } catch (error) {
                this.notify('Error adjusting air', 'error');
            }
        },

        async powerOffDevice() {
            try {
                const response = await fetch('/api/power-off', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    }
                });

                if (!response.ok) {
                    throw new Error('Failed to power off device');
                }

                setTimeout(() => {
                    this.startScan();
                }, 2000);
            } catch (error) {
                this.notify('Error powering off device', 'error');
            }
        },

        getBleStatusClass() {
            switch (this.status.ble.stateString) {
                case 'CONNECTED':
                    return 'bg-success';
                case 'FAILED':
                    return 'bg-danger';
                case 'SCANNING':
                case 'CONNECTING':
                    return 'bg-primary';
                default:
                    return 'bg-secondary';
            }
        },
    }
}
</script>
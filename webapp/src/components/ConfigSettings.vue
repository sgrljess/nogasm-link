<template>
    <div class="config-settings">
        <div class="config-header">
            <h5>Configuration Settings</h5>
            <button @click="toggleExpanded" class="btn btn-secondary btn-sm">
                <i :class="expanded ? 'fas fa-chevron-up' : 'fas fa-chevron-down'"></i>
                {{ expanded ? 'Collapse' : 'Expand' }}
            </button>
        </div>

        <div v-if="expanded" class="config-content">
            <div v-if="loading" class="loading-state">
                <div class="spinner-large"></div>
                <p>Loading configuration...</p>
            </div>

            <div v-else class="config-sections">
                <div class="config-section">
                    <h6><i class="fas fa-wifi"></i> Connection Settings</h6>

                    <SliderInput id="scanDuration" v-model="config.connection.scanDuration" unit="s"
                                 title="The duration in which to scan for devices"
                                 :min="5000" min-title="Short"
                                 :max="60000" max-title="Long"
                                 :step="1000"
                                 :display-transform="value => Math.round(value / 1000)">
                        <template v-slot:label>
                            Bluetooth scan duration
                        </template>
                    </SliderInput>

                    <SliderInput id="connectionTimeout" v-model="config.connection.connectionTimeout" unit="s"
                                 title="The time to wait for BLE operations to complete before timing out"
                                 :min="5000" min-title="Short"
                                 :max="30000" max-title="Long"
                                 :step="1000"
                                 :display-transform="value => Math.round(value / 1000)">
                        <template v-slot:label>
                            Connection timeout
                        </template>
                    </SliderInput>
                </div>

                <div class="config-section">
                    <h6><i class="fas fa-cog"></i> Device Settings</h6>

                    <SliderInput id="defaultVibrationLevel" v-model="config.device.defaultVibrationLevel"
                                 :min="0" min-title="Off"
                                 :max="20" max-title="Max"
                                 :display-transform="value => `Level ${value}`">
                        <template v-slot:label>
                            Default Vibration Level
                        </template>
                    </SliderInput>
                </div>

                <div class="config-section">
                    <h6><i class="fas fa-user-cog"></i> Behavior Settings</h6>

                    <div class="config-switches">
                        <label class="switch-label">
                            <input type="checkbox" class="switch-input" v-model="config.ui.autoConnect">
                            <span class="switch-slider"></span>
                            <span class="switch-text">Auto-connect to device</span>
                        </label>
                        <p class="switch-description">Automatically connect when device is found or when only one device
                            is available</p>

                        <label class="switch-label">
                            <input type="checkbox" class="switch-input" v-model="config.ui.autoReconnect">
                            <span class="switch-slider"></span>
                            <span class="switch-text">Auto-reconnect on disconnect</span>
                        </label>
                        <p class="switch-description">Automatically attempt to reconnect if connection is lost</p>
                    </div>
                </div>

                <div class="config-actions">
                    <button @click="saveConfig" class="btn btn-primary" :disabled="saving">
                        <div v-if="saving" class="spinner"></div>
                        <i v-else class="fas fa-save"></i>
                        {{ saving ? 'Saving...' : 'Save Settings' }}
                    </button>
                    <button @click="resetWifi" class="btn btn-secondary">
                        <i class="fas fa-wifi"></i>
                        Reset WiFi
                    </button>
                    <button @click="resetConfig" class="btn btn-secondary">
                        <i class="fas fa-undo"></i>
                        Reset to Defaults
                    </button>
                </div>
            </div>
        </div>
    </div>
</template>

<script>
import SliderInput from "@/components/SliderInput.vue";

export default {
    name: 'ConfigSettings',
    components: {SliderInput},
    data() {
        return {
            expanded: false,
            loading: false,
            saving: false,
            config: {
                connection: {
                    scanDuration: 15000,
                    connectionTimeout: 15000
                },
                device: {
                    defaultVibrationLevel: 0
                },
                ui: {
                    autoConnect: true,
                    autoReconnect: true
                }
            },
            defaultConfig: {
                connection: {
                    scanDuration: 15000,
                    connectionTimeout: 15000
                },
                device: {
                    defaultVibrationLevel: 0
                },
                ui: {
                    autoConnect: true,
                    autoReconnect: true
                }
            }
        }
    },
    methods: {
        toggleExpanded() {
            this.expanded = !this.expanded;
            if (this.expanded) {
                this.fetchConfig();
            }
        },
        async fetchConfig() {
            this.loading = true;
            try {
                const response = await fetch('/api/config');
                if (!response.ok) {
                    throw new Error('Failed to fetch configuration');
                }

                const data = await response.json();

                this.config = {
                    connection: {
                        scanDuration: data.connection?.scanDuration || this.defaultConfig.connection.scanDuration,
                        connectionTimeout: data.connection?.connectionTimeout || this.defaultConfig.connection.connectionTimeout
                    },
                    device: {
                        defaultVibrationLevel: data.device?.defaultVibrationLevel || this.defaultConfig.device.defaultVibrationLevel
                    },
                    ui: {
                        autoConnect: data.ui?.autoConnect ?? this.defaultConfig.ui.autoConnect,
                        autoReconnect: data.ui?.autoReconnect ?? this.defaultConfig.ui.autoReconnect
                    }
                };
            } catch (error) {
                this.$emit('send-notification', 'Error fetching configuration', 'error');
            } finally {
                this.loading = false;
            }
        },
        async saveConfig() {
            this.saving = true;
            try {
                const configToSave = {
                    connection: {
                        scanDuration: parseInt(this.config.connection.scanDuration),
                        connectionTimeout: parseInt(this.config.connection.connectionTimeout)
                    },
                    device: {
                        defaultVibrationLevel: parseInt(this.config.device.defaultVibrationLevel)
                    },
                    ui: {
                        autoConnect: !!this.config.ui.autoConnect,
                        autoReconnect: !!this.config.ui.autoReconnect
                    }
                };

                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(configToSave)
                });

                if (!response.ok) {
                    throw new Error('Failed to save configuration');
                }

                const result = await response.json();
                if (result.success) {
                    this.$emit('send-notification', 'Configuration saved successfully.');
                } else {
                    throw new Error(result.message || 'Failed to save configuration');
                }
            } catch (error) {
                this.$emit('send-notification', 'Failed to save configuration', 'error');
            } finally {
                this.saving = false;
            }
        },
        resetConfig() {
            if (confirm('Are you sure you want to reset all settings to default values?')) {
                this.config = JSON.parse(JSON.stringify(this.defaultConfig));
            }
        },
        async resetWifi() {
            if (confirm('Are you sure you want to reset WiFi settings? You will need to reconnect to the access point.')) {
                try {
                    const response = await fetch('/api/reset-wifi', {
                        method: 'POST'
                    });
                    if (!response.ok) {
                        throw new Error('Failed to reset WiFi');
                    }
                } catch (error) {
                    this.$emit('send-notification', 'Error resetting WiFi', 'error');
                }
            }
        },
    }
}
</script>

<style scoped>
.config-settings {
    margin-top: 1rem;
    padding-top: 1rem;
    border-top: 1px solid var(--border);
}

.config-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.config-header h5 {
    margin: 0;
    color: var(--text-primary);
    font-weight: 600;
}

.config-content {
    background: var(--surface-elevated);
    border: 1px solid var(--border);
    border-radius: var(--border-radius);
    padding: 1.5rem;
}

.loading-state {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 3rem;
    color: var(--text-secondary);
}

.config-sections {
    display: grid;
    gap: 2rem;
}

.config-section {
    padding-bottom: 1.5rem;
    border-bottom: 1px solid var(--border);
}

.config-section:last-of-type {
    border-bottom: none;
    padding-bottom: 0;
}

.config-section h6 {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-bottom: 1rem;
    color: var(--text-primary);
    font-weight: 600;
    font-size: 1rem;
}

.config-section h6 i {
    color: var(--text-secondary);
}

.config-switches {
    display: flex;
    flex-direction: column;
    gap: 1.5rem;
}

.config-actions {
    display: flex;
    justify-content: space-between;
    gap: 1rem;
    margin-top: 2rem;
    padding-top: 1.5rem;
    border-top: 1px solid var(--border);
}

@media (max-width: 768px) {
    .config-actions {
        flex-direction: column;
    }
}
</style>
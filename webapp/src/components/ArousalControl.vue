<template>
    <div class="card">
        <div class="card-header">
            <div class="card-title">
                <i class="fas fa-play-circle"></i>
                Session
            </div>
            <div class="status-indicator" :class="arousalActive ? 'status-active' : 'status-inactive'">
                <i class="fas fa-circle"></i>
                {{ arousalActive ? 'Active' : 'Inactive' }}
            </div>
        </div>

        <div class="progress-container">
            <div class="progress-label">
                <span>Arousal level</span>
                <span class="progress-value">{{ arousalPercent.toFixed(1) }}%</span>
            </div>
            <div class="progress-bar">
                <div class="progress-fill" :class="getProgressBarClass()"
                     :style="{ width: arousalPercent + '%' }"/>
            </div>
        </div>

        <div class="progress-container">
            <div class="progress-label">
                <span>Pressure</span>
                <span class="progress-value">{{ currentPressure.toFixed(0) }} / {{ config.maxPressureLimit }}</span>
            </div>
            <div class="progress-bar">
                <div class="progress-fill pressure-fill"
                     :style="{ width: (currentPressure / config.maxPressureLimit * 100) + '%' }"/>
                <div v-if="clenchThreshold" class="clench-threshold-marker"
                     :style="{ left: (clenchThreshold / config.maxPressureLimit * 100) + '%' }"/>
            </div>
            <div v-if="clenchThreshold" class="clench-label">
                Clench threshold: {{ clenchThreshold.toFixed(0) }}
            </div>
        </div>

        <div class="metric-grid">
            <div class="metric">
                <div class="metric-value">{{ limitExceededCounter }}</div>
                <div class="metric-label">Edges</div>
            </div>
            <div class="metric">
                <div class="metric-value">
                    {{ limitExceededCounter >= config.targetEdgeCount ? 'Orgasm allowed' : config.targetEdgeCount }}
                </div>
                <div class="metric-label">Target</div>
            </div>
            <div class="metric">
                <div class="metric-value">{{ lastClenchDuration }} ms</div>
                <div class="metric-label">Last clench</div>
            </div>
            <div class="metric">
                <div class="metric-value">{{ sessionDurationDisplay }}</div>
                <div class="metric-label">Session duration</div>
            </div>
        </div>

        <div v-if="cooldownActive" class="alert alert-warning">
            <i class="fas fa-clock"></i>
            <strong>Cooldown active:</strong> {{ cooldownRemainingSeconds }} seconds remaining
        </div>

        <div v-if="cooldownJustEnded" class="alert alert-success fade-out">
            <i class="fas fa-check-circle"></i>
            <strong>Cooldown ended!</strong> session resumed
        </div>

        <div class="controls">
            <button @click="toggleArousalManager" class="btn"
                    :class="arousalActive ? 'btn-danger' : 'btn-success'">
                <i :class="arousalActive ? 'fas fa-stop' : 'fas fa-play'"></i>
                {{ arousalActive ? 'Stop' : 'Start' }}
            </button>
            <button @click="resetArousal" class="btn btn-secondary" :disabled="!arousalActive">
                <i class="fas fa-redo"></i>
                Reset
            </button>
            <button @click="toggleConfigSection" class="btn btn-secondary">
                <i class="fas fa-cogs"></i>
                {{ showConfig ? 'Hide Config' : 'Config' }}
            </button>
        </div>

        <div class="slider-container">
            <div class="slider-label">
                <span>Arousal sensitivity</span>
            </div>
            <input type="range" class="slider" min="0" max="255" v-model="sensitivityInput"/>
            <div class="slider-range">
                <span>Sensitive</span>
                <span>{{ sensitivityInput }}</span>
                <span>Insensitive</span>
            </div>
            <div v-if="config.sensitivityAfterEdgeDecayRate < 1" class="alert alert-success">
                <p>Auto-reduce is switched <strong>on</strong></p>
                <p >
                    Every time an edge is detected, sensitivity will reduce by
                    <strong>{{ ((1 - config.sensitivityAfterEdgeDecayRate) * 100).toFixed(1) }}%</strong>
                </p>
            </div>
        </div>

        <div v-if="showConfig" class="config-section">
            <div class="tabs">
                <div class="tab" :class="{ active: configTab === 'general' }" @click="configTab = 'general'">
                    General
                </div>
                <div class="tab" :class="{ active: configTab === 'sensitivity' }" @click="configTab = 'sensitivity'">
                    Sensitivity
                </div>
                <div class="tab" :class="{ active: configTab === 'advanced' }" @click="configTab = 'advanced'">
                    Advanced
                </div>
            </div>

            <div v-if="configTab === 'general'" class="config-grid">
                <div class="config-item">
                    <label title="The limit for the vibration level">
                        Vibration level maximum
                    </label>
                    <input type="range" class="slider" v-model="config.maxVibrationLevel"
                           min="0" max="20"/>
                    <div class="slider-range">
                        <span>Off</span>
                        <span>{{ config.maxVibrationLevel }}</span>
                        <span>Faster</span>
                    </div>
                </div>
                <div class="config-item">
                    <label title="Time it takes the device to reach max speed">
                        Vibration speed ramp time
                    </label>
                    <input type="range" class="slider" v-model="config.rampTimeSeconds"
                           min="5" max="300"/>
                    <div class="slider-range">
                        <span>Shorter</span>
                        <span>{{ config.rampTimeSeconds }}s</span>
                        <span>Longer</span>
                    </div>
                </div>

                <div class="config-item">
                    <label title="How much time to wait before restarting vibration after edge has been detected">
                        Edge cooldown time
                    </label>
                    <input type="range" class="slider" v-model="config.coolTimeSeconds"
                           min="5" max="300"/>
                    <div class="slider-range">
                        <span>Shorter</span>
                        <span>{{ config.coolTimeSeconds }}s</span>
                        <span>Longer</span>
                    </div>
                </div>
                <div class="config-item">
                    <label title="How many edges are needed before orgasm is allowed">Edge count needed</label>
                    <input type="range" class="slider" v-model="config.targetEdgeCount"
                           min="1" max="500"/>
                    <div class="slider-range">
                        <span>Less</span>
                        <span>{{ config.targetEdgeCount }}</span>
                        <span>More</span>
                    </div>
                </div>
            </div>
            <div v-else-if="configTab === 'sensitivity'" class="config-grid">
                <div class="config-item">
                    <label>Clench sensitivity</label>
                    <input type="range" class="slider" v-model="config.clenchPressureSensitivity"
                           min="5" max="200"/>
                    <div class="slider-range">
                        <span>Sensitive</span>
                        <span>{{ config.clenchPressureSensitivity }}</span>
                        <span>Insensitive</span>
                    </div>
                </div>
                <div class="config-item">
                    <label>Peak sensitivity</label>
                    <input type="range" class="slider" v-model="config.sensitivityThreshold"
                           min="10" max="200"/>
                    <div class="slider-range">
                        <span>Sensitive</span>
                        <span>{{ config.sensitivityThreshold }}</span>
                        <span>Insensitive</span>
                    </div>
                </div>

                <div class="config-item">
                    <label>Minimum clench detection time</label>
                    <input type="range" class="slider" v-model="config.clenchTimeMinThresholdMs"
                           min="50" max="1000" step="10"/>
                    <div class="slider-range">
                        <span>Less</span>
                        <span>{{ config.clenchTimeMinThresholdMs }}ms</span>
                        <span>More</span>
                    </div>
                </div>
                <div class="config-item">
                    <label>Maximum clench detection time</label>
                    <input type="range" class="slider" v-model="config.clenchTimeMaxThresholdMs"
                           min="1500" max="10000" step="100">
                    <div class="slider-range">
                        <span>Less</span>
                        <span>{{ config.clenchTimeMaxThresholdMs }}ms</span>
                        <span>More</span>
                    </div>
                </div>
            </div>
            <div v-else-if="configTab === 'advanced'" class="config-grid">
                <div class="config-item">
                    <label title="Rate at which the arousal level will decay at the specified frequency">
                        Arousal decay rate
                    </label>
                    <input type="range" class="slider" v-model="config.arousalDecayRate"
                           min="0.100" max="0.999" step="0.001"/>
                    <div class="slider-range">
                        <span>Faster</span>
                        <span>{{ config.arousalDecayRate }}</span>
                        <span>Slower</span>
                    </div>
                </div>
                <div class="config-item">
                    <label title="The max that the arousal sensitivity is based on">
                        Maximum arousal limit
                    </label>
                    <input type="range" class="slider" v-model="config.maxArousalLimit"
                           min="100" max="4000"/>
                    <div class="slider-range">
                        <span>Less</span>
                        <span>{{ config.maxArousalLimit }}</span>
                        <span>More</span>
                    </div>
                </div>

                <div class="config-item">
                    <label title="Determines by how much the sensitivity would reduce after an edge has been detected">
                        Sensitivity after edge decay rate
                    </label>
                    <input type="range" class="slider" v-model="config.sensitivityAfterEdgeDecayRate"
                           min="0.100" max="1.00" step="0.001"/>
                    <div class="slider-range">
                        <span>Faster</span>
                        <span>{{ config.sensitivityAfterEdgeDecayRate }}</span>
                        <span>Slower / Off</span>
                    </div>
                </div>
                <div class="config-item">
                    <label title="Sensitivity won't decay to less than this limit">
                        Minimum sensitivity after edge limit
                    </label>
                    <input type="range" class="slider" v-model="config.minSensitivityWhileDecaying"
                           min="1" max="255"/>
                    <div class="slider-range">
                        <span>Less</span>
                        <span>{{ config.minSensitivityWhileDecaying }}</span>
                        <span>More</span>
                    </div>
                </div>
            </div>

            <div class="config-actions">
                <button @click="saveConfig" class="btn btn-primary" :disabled="saving">
                    <span v-if="saving" class="spinner"></span>
                    <i v-else class="fas fa-save"></i>
                    {{ saving ? 'Saving...' : 'Save Settings' }}
                </button>
                <button @click="resetConfig" class="btn btn-secondary">
                    <i class="fas fa-undo"></i>
                    Reset to Defaults
                </button>
            </div>
        </div>
    </div>
</template>

<script>
import websocketService from '../services/WebSocketService';

export default {
    name: 'ArousalControl',
    data() {
        return {
            arousalStateString: null,
            arousalActive: false,
            arousalPercent: 0,
            currentPressure: 0,
            arousalLimit: 500,
            limitExceededCounter: 0,
            clenchThreshold: null,
            lastClenchDuration: null,
            currentSessionDuration: 0,
            sensitivity: 128,
            pendingUpdate: false,
            showConfig: false,
            configTab: 'general',
            saving: false,
            updateTimeout: null,
            unsubscribeFunctions: [],
            websocketService,

            config: {
                arousalDecayRate: 0.99,
                sensitivityAfterEdgeDecayRate: 0.99,
                minSensitivityWhileDecaying: 40,
                sensitivityThreshold: 70,
                maxArousalLimit: 4000,
                maxPressureLimit: 4030,
                maxVibrationLevel: 20,
                frequency: 60,
                rampTimeSeconds: 50.0,
                coolTimeSeconds: 15.0,
                clenchPressureSensitivity: 20,
                clenchTimeMinThresholdMs: 250,
                clenchTimeMaxThresholdMs: 3500,
                targetEdgeCount: 20,
            },
            defaultConfig: {
                arousalDecayRate: 0.99,
                sensitivityAfterEdgeDecayRate: 0.99,
                minSensitivityWhileDecaying: 40,
                sensitivityThreshold: 70,
                maxArousalLimit: 4000,
                maxPressureLimit: 4030,
                maxVibrationLevel: 20,
                frequency: 60,
                rampTimeSeconds: 50.0,
                coolTimeSeconds: 15.0,
                clenchPressureSensitivity: 20,
                clenchTimeMinThresholdMs: 250,
                clenchTimeMaxThresholdMs: 3500,
                targetEdgeCount: 20,
            },

            cooldownActive: false,
            cooldownRemainingSeconds: 0,
            cooldownJustEnded: false,
            cooldownEndTime: 0,
        }
    },
    computed: {
        sensitivityInput: {
            get() {
                return this.sensitivity;
            },
            set(value) {
                this.sensitivity = value;
                this.pendingUpdate = true;

                if (this.updateTimeout) {
                    clearTimeout(this.updateTimeout);
                }

                this.updateTimeout = setTimeout(() => {
                    this.setSensitivity(value);

                    setTimeout(() => {
                        this.pendingUpdate = false;
                    }, 500);
                }, 300);
            }
        },
        sessionDurationDisplay: {
            get() {
                if (this.currentSessionDuration <= 0) {
                    return '00:00';
                }

                const elapsed = this.currentSessionDuration;
                const minutes = Math.floor(elapsed / 60000);
                const seconds = Math.floor((elapsed % 60000) / 1000);
                return `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
            }
        }
    },
    mounted() {
        this.initWebSocket();
        this.fetchArousalConfig();
    },
    beforeUnmount() {
        this.unsubscribeFunctions.forEach(unsubscribe => unsubscribe());

        if (this.updateTimeout) {
            clearTimeout(this.updateTimeout);
        }
    },
    methods: {
        initWebSocket() {
            websocketService.connect().catch(() => {
                this.$emit('send-notification', 'Failed to connect to WebSocket', 'error');
            });

            this.unsubscribeFunctions.push(
                websocketService.subscribe('arousal_status', (data) => {
                    this.updateArousalStatus(data);
                })
            );
        },

        updateArousalStatus(data) {
            this.arousalStateString = data.state;
            this.arousalActive = data.active;
            this.arousalPercent = data.arousalPercent;
            this.currentPressure = data.pressure;
            this.arousalLimit = data.limit;
            this.limitExceededCounter = data.limitExceededCounter;
            this.clenchThreshold = data.clenchThreshold;
            this.lastClenchDuration = data.lastClenchDuration;
            this.currentSessionDuration = data.currentSessionDuration;

            if (!this.pendingUpdate) {
                this.sensitivity = data.sensitivity;
            }

            // console.log('State: ' + this.arousalStateString);
            if (this.arousalStateString === 'LIMIT_EXCEEDED' || this.arousalStateString === 'COOL_OFF_ACTIVE') {
                this.onCooldownActiveEvent();
            } else if (this.arousalStateString === 'COOL_OFF_ENDED') {
                this.onCooldownEndedEvent();
            } else {
                this.updateCooldownRemaining();
            }
        },

        onCooldownActiveEvent() {
            this.cooldownJustEnded = false;
            if (!this.cooldownActive) {
                const cooldownDurationMs = this.config.coolTimeSeconds * 1000;
                // todo: should probably just get this from the server in the future.
                this.cooldownEndTime = Date.now() + cooldownDurationMs;
            }

            this.updateCooldownRemaining();
            this.cooldownActive = true;
        },

        updateCooldownRemaining() {
            const now = Date.now();
            const remaining = this.cooldownEndTime - now;

            if (remaining <= 0) {
                // we might miss the event from the server (only gets sent once, and every 100ms)
                this.cooldownRemainingSeconds = 0;
                this.onCooldownEndedEvent();
            } else {
                this.cooldownRemainingSeconds = Math.ceil(remaining / 1000);
            }
        },

        onCooldownEndedEvent() {
            if (this.cooldownJustEnded) {
                return;
            }

            if (this.cooldownActive) {
                this.cooldownActive = false;
                this.cooldownRemainingSeconds = 0;

                this.cooldownJustEnded = true;
                setTimeout(() => {
                    this.cooldownJustEnded = false;
                }, 3000);
            }
        },

        async fetchArousalConfig() {
            try {
                const response = await fetch('/api/arousal/config');
                if (!response.ok) {
                    throw new Error('Failed to fetch arousal config');
                }

                const data = await response.json();
                this.config = {...data};
                this.defaultConfig = {...data};
            } catch (error) {
                this.$emit('send-notification', 'Error fetching arousal config', 'error');
            }
        },

        async toggleArousalManager() {
            try {
                const nextState = !this.arousalActive;
                const response = await fetch('/api/arousal/state', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({active: nextState})
                });

                if (!response.ok) {
                    throw new Error('Failed to toggle arousal manager');
                }
            } catch (error) {
                this.$emit('send-notification', 'Error toggling arousal manager', 'error');
            }
        },

        async resetArousal() {
            try {
                const response = await fetch('/api/arousal/state', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({reset: true})
                });

                if (!response.ok) {
                    throw new Error('Failed to reset arousal');
                }

                this.$emit('send-notification', 'Recalibrated sensor and restarted session');
            } catch (error) {
                this.$emit('send-notification', 'Error resetting arousal', 'error');
            }
        },

        async setSensitivity(value) {
            try {
                const response = await fetch('/api/arousal/sensitivity', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({sensitivity: parseInt(value)})
                });

                if (!response.ok) {
                    throw new Error('Failed to set sensitivity');
                }
            } catch (error) {
                this.$emit('send-notification', 'Error setting sensitivity', 'error');
            }
        },

        async saveConfig() {
            this.saving = true;
            try {
                const response = await fetch('/api/arousal/config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(this.config)
                });

                if (!response.ok) {
                    throw new Error('Failed to save configuration');
                }

                const result = await response.json();
                if (result.success) {
                    this.$emit('send-notification', 'Arousal configuration saved successfully.');
                    this.defaultConfig = {...this.config};
                } else {
                    alert(result.message || 'Failed to save configuration');
                }
            } catch (error) {
                this.$emit('send-notification', 'Error saving configuration', 'error');
            } finally {
                this.saving = false;
            }
        },

        resetConfig() {
            if (confirm('Are you sure you want to reset all arousal settings to default values?')) {
                this.config = JSON.parse(JSON.stringify(this.defaultConfig));
            }
        },

        toggleConfigSection() {
            this.showConfig = !this.showConfig;
        },

        getProgressBarClass() {
            if (this.arousalPercent < 30) {
                return 'arousal-low';
            } else if (this.arousalPercent < 70) {
                return 'arousal-medium';
            } else {
                return 'arousal-high';
            }
        },
    }
}
</script>

<style scoped>
.status-active {
    background: var(--success-gradient);
    color: white;
    animation: pulse 2s infinite;
}

.status-inactive {
    background: var(--surface-elevated);
    color: var(--text-secondary);
    border: 1px solid var(--border);
}

.progress-container {
    margin: 1.5rem 0;
}

.progress-label {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.5rem;
    font-size: 0.875rem;
    color: var(--text-secondary);
}

.progress-value {
    font-weight: 600;
    color: var(--text-primary);
}

.progress-bar {
    height: 12px;
    background: #e2e8f0;
    border-radius: var(--border-radius-sm);
    overflow: hidden;
    position: relative;
}

.progress-fill {
    height: 100%;
    border-radius: var(--border-radius-sm);
    border-bottom-right-radius: 0;
    border-top-right-radius: 0;
    transition: width 0.3s ease;
    position: relative;
}

.progress-fill::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.3), transparent);
    animation: shimmer 2s infinite;
}

@keyframes shimmer {
    0% {
        transform: translateX(-100%);
    }
    100% {
        transform: translateX(100%);
    }
}

.progress-fill.arousal-low {
    background: var(--success-gradient);
}

.progress-fill.arousal-medium {
    background: var(--warning-gradient);
}

.progress-fill.arousal-high {
    background: var(--danger-gradient);
}

.pressure-fill {
    background: var(--primary-gradient);
}

.clench-threshold-marker {
    position: absolute;
    top: 0;
    height: 100%;
    width: 3px;
    background-color: #fd7e14;
    z-index: 10;
}

.clench-label {
    font-size: 0.75rem;
    color: #fd7e14;
    font-weight: 500;
    margin-top: 0.25rem;
    text-align: center;
}

.controls {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 1rem;
    margin: 2rem 0;
}

.alert {
    padding: 1rem;
    border-radius: var(--border-radius);
    margin: 1rem 0;
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.alert-warning {
    background: #fff3cd;
    color: #856404;
    border: 1px solid #ffeaa7;
}

.alert-success {
    background: #d1ecf1;
    color: #0c5460;
    border: 1px solid #bee5eb;
}

.fade-out {
    animation: fadeOut 3s forwards;
}

@keyframes fadeOut {
    0% {
        opacity: 1;
    }

    70% {
        opacity: 1;
    }

    100% {
        opacity: 0;
    }
}

.config-section {
    margin-top: 2rem;
    padding-top: 2rem;
    border-top: 1px solid var(--border);
}

.config-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1.5rem;
}

.config-item {
    display: flex;
    flex-direction: column;
}

.config-item label {
    font-weight: 500;
    margin-bottom: 0.5rem;
    color: var(--text-primary);
}

.config-actions {
    display: flex;
    justify-content: space-between;
    margin-top: 2rem;
    gap: 1rem;
}

@media (max-width: 768px) {
    .controls {
        grid-template-columns: 1fr;
    }

    .config-grid {
        grid-template-columns: 1fr;
    }

    .config-actions {
        flex-direction: column;
    }
}
</style>
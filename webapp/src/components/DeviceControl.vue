<template>
    <div class="card">
        <div class="card-header">
            <div class="card-title">
                <i class="fas fa-sliders-h"></i>
                Control
            </div>
            <div class="device-status">
                <div v-if="deviceModel" class="device-badge">{{ deviceModel }}</div>
                <div v-if="deviceFirmware" class="firmware-badge">
                    v{{ (Number(deviceFirmware) / 100).toFixed(2) }}
                </div>
                <div v-if="batteryLevel >= 0" class="battery-indicator">
                    <div class="battery-level"
                         :style="{ width: batteryLevel + '%', backgroundColor: batteryColor }"></div>
                    <span class="battery-text">{{ batteryLevel }}%</span>
                </div>
            </div>
        </div>

        <div class="tabs">
            <div class="tab" :class="{ active: activeTab === 'vibration' }" @click="activeTab = 'vibration'">
                <i class="fas fa-vibrate"></i>
                Vibration
            </div>
            <div v-if="hasRotation" class="tab" :class="{ active: activeTab === 'rotation' }"
                 @click="activeTab = 'rotation'">
                <i class="fas fa-sync-alt"></i>
                Rotation
            </div>
            <div v-if="hasAirControl" class="tab" :class="{ active: activeTab === 'air' }" @click="activeTab = 'air'">
                <i class="fas fa-wind"></i>
                Air Control
            </div>
        </div>

        <div class="tab-content">
            <div v-show="activeTab === 'vibration'" class="control-panel">
                <SliderInput id="intensity" v-model="vibrationLevel"
                             :simple="true"
                             :min="0" min-title="Off"
                             :max="20" max-title="Max"
                             :display-transform="value => `Level ${value}`">
                    <template v-slot:label>
                        Intensity
                    </template>
                </SliderInput>

                <div class="controls">
                    <button @click="setVibration(vibrationLevel)" class="btn btn-primary">
                        <i class="fas fa-play"></i>
                        Apply
                    </button>
                    <button @click="setVibration(0)" class="btn btn-danger">
                        <i class="fas fa-stop"></i>
                        Stop
                    </button>
                </div>
            </div>

            <div v-show="activeTab === 'rotation' && hasRotation" class="control-panel">
                <SliderInput id="rotation" v-model="rotationLevel"
                             :simple="true"
                             :min="0" min-title="Off"
                             :max="20" max-title="Max"
                             :display-transform="value => `Level ${value}`">
                    <template v-slot:label>
                        Rotation speed
                    </template>
                </SliderInput>

                <div class="controls">
                    <button @click="setRotation(rotationLevel)" class="btn btn-primary">
                        <i class="fas fa-play"></i>
                        Apply
                    </button>
                    <button @click="changeRotationDirection" class="btn btn-secondary">
                        <i class="fas fa-sync-alt"></i>
                        Change Direction
                    </button>
                    <button @click="setRotation(0)" class="btn btn-danger">
                        <i class="fas fa-stop"></i>
                        Stop
                    </button>
                </div>
            </div>

            <div v-show="activeTab === 'air' && hasAirControl" class="control-panel">
                <SliderInput id="airPump" v-model="airLevel"
                             :simple="true"
                             :min="0" min-title="Off"
                             :max="4" max-title="Max"
                             :display-transform="value => `Level ${value}`">
                    <template v-slot:label>
                        Air pump level
                    </template>
                </SliderInput>

                <div class="controls">
                    <button @click="setAirLevel(airLevel)" class="btn btn-primary">
                        <i class="fas fa-play"></i>
                        Apply
                    </button>
                    <button @click="setAirLevel(0)" class="btn btn-danger">
                        <i class="fas fa-stop"></i>
                        Stop
                    </button>
                </div>

                <div class="air-controls">
                    <h5>Quick Adjustments</h5>
                    <div class="air-buttons">
                        <button @click="adjustAir(true, 1)" class="btn btn-secondary">
                            <i class="fas fa-plus"></i>
                            Inflate +1
                        </button>
                        <button @click="adjustAir(false, 1)" class="btn btn-secondary">
                            <i class="fas fa-minus"></i>
                            Deflate -1
                        </button>
                    </div>
                </div>
            </div>
        </div>

        <div class="device-actions">
            <button @click="powerOff" class="btn btn-warning">
                <i class="fas fa-power-off"></i>
                Power Off
            </button>
            <button @click="disconnect" class="btn btn-danger">
                <i class="fas fa-unlink"></i>
                Disconnect
            </button>
        </div>
    </div>
</template>

<script>
import SliderInput from "@/components/SliderInput.vue";

export default {
    name: 'DeviceControl',
    components: {SliderInput},
    props: {
        batteryLevel: {
            type: Number,
            default: -1
        },
        deviceModel: {
            type: String,
            default: ''
        },
        deviceFirmware: {
            type: String,
            default: ''
        }
    },
    data() {
        return {
            vibrationLevel: 0,
            rotationLevel: 0,
            airLevel: 0,
            activeTab: 'vibration'
        }
    },
    computed: {
        batteryColor() {
            if (this.batteryLevel <= 20) {
                return '#dc3545';
            }
            if (this.batteryLevel <= 50) {
                return '#ffc107';
            }
            return '#28a745';
        },
        hasRotation() {
            const rotationCapableModels = ['NORA'];
            return rotationCapableModels.includes(this.deviceModel);
        },
        hasAirControl() {
            const airControlCapableModels = ['MAX'];
            return airControlCapableModels.includes(this.deviceModel);
        }
    },
    watch: {
        deviceModel: {
            immediate: true,
            // eslint-disable-next-line no-unused-vars
            handler(newModel) {
                if (this.activeTab === 'rotation' && !this.hasRotation) {
                    this.activeTab = 'vibration';
                }
                if (this.activeTab === 'air' && !this.hasAirControl) {
                    this.activeTab = 'vibration';
                }
            }
        }
    },
    methods: {
        disconnect() {
            this.setVibration(0);
            if (this.hasRotation) {
                this.setRotation(0);
            }
            if (this.hasAirControl) {
                this.setAirLevel(0);
            }
            this.$emit('disconnect');
        },

        setVibration(level) {
            if (level === 0) {
                this.vibrationLevel = 0;
            }
            this.$emit('set-vibration', parseInt(level));
        },

        setRotation(level) {
            if (level === 0) {
                this.rotationLevel = 0;
            }
            this.$emit('set-rotation', parseInt(level));
        },

        changeRotationDirection() {
            this.$emit('change-rotation-direction');
        },

        setAirLevel(level) {
            if (level === 0) {
                this.airLevel = 0;
            }
            this.$emit('set-air-level', parseInt(level));
        },

        adjustAir(inflate, amount) {
            this.$emit('adjust-air', inflate, parseInt(amount));
        },

        powerOff() {
            this.vibrationLevel = 0;
            this.rotationLevel = 0;
            this.airLevel = 0;

            this.$emit('power-off');
        }
    }
}
</script>

<style scoped>
.device-status {
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.device-badge, .firmware-badge {
    background: var(--primary-gradient);
    color: white;
    padding: 0.25rem 0.75rem;
    border-radius: 50px;
    font-size: 0.75rem;
    font-weight: 500;
}

.firmware-badge {
    background: var(--surface-elevated);
    color: var(--text-secondary);
    border: 1px solid var(--border);
}

.battery-indicator {
    position: relative;
    width: 40px;
    height: 20px;
    border: 2px solid var(--border);
    border-radius: 3px;
    padding: 1px;
    background-color: var(--surface-elevated);
}

.battery-indicator:after {
    content: '';
    position: absolute;
    top: 50%;
    right: -4px;
    transform: translateY(-50%);
    width: 3px;
    height: 10px;
    background-color: var(--border);
    border-radius: 0 2px 2px 0;
}

.battery-level {
    height: 100%;
    background-color: #28a745;
    border-radius: 1px;
    transition: width 0.3s ease;
}

.battery-text {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    font-size: 9px;
    color: white;
    text-shadow: 0 0 2px rgba(0, 0, 0, 0.7);
    white-space: nowrap;
    font-weight: 600;
}

.control-panel {
    padding: 1rem 0;
}

.controls {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1rem;
    margin: 2rem 0;
}

.controls.three-buttons {
    grid-template-columns: 1fr 1fr 1fr;
}

.air-controls {
    margin-top: 2rem;
    padding-top: 1rem;
    border-top: 1px solid var(--border);
}

.air-controls h5 {
    margin-bottom: 1rem;
    color: var(--text-primary);
    font-size: 1rem;
    font-weight: 600;
}

.air-buttons {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1rem;
}

.device-actions {
    display: flex;
    justify-content: space-between;
    margin-top: 2rem;
    padding-top: 1rem;
    border-top: 1px solid var(--border);
    gap: 1rem;
}

@media (max-width: 768px) {
    .controls {
        grid-template-columns: 1fr;
    }

    .controls.three-buttons {
        grid-template-columns: 1fr;
    }

    .air-buttons {
        grid-template-columns: 1fr;
    }

    .device-actions {
        flex-direction: column;
    }

    .device-status {
        flex-wrap: wrap;
        justify-content: center;
    }
}
</style>
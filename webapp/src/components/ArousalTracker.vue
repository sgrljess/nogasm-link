<template>
    <div class="card">
        <div class="card-header">
            <div class="card-title">
                <i class="fas fa-chart-line"></i>
                Analytics
            </div>
            <div class="analytics-controls">
                <div class="status-indicator" :class="isRecording ? 'status-recording' : 'status-inactive'">
                    <i class="fas fa-circle"></i>
                    {{ isRecording ? 'Recording' : 'Inactive' }}
                </div>
                <button @click="exportCSV" class="btn btn-secondary" :disabled="!hasData">
                    <i class="fas fa-download"></i>
                    Export
                </button>
                <button @click="clearData" class="btn btn-danger">
                    <i class="fas fa-trash"></i>
                    Clear Data
                </button>
            </div>
        </div>

        <div class="metric-grid">
            <div class="metric">
                <div class="metric-value">{{ dataPoints.length }}</div>
                <div class="metric-label">Data Points</div>
            </div>
            <div class="metric">
                <div class="metric-value">{{ states.length }}</div>
                <div class="metric-label">Events Marked</div>
            </div>
        </div>

        <div class="chart-container">
            <div class="chart-header">
                <h5>Real-time Data</h5>
                <div class="chart-controls">
                    <label class="switch-label">
                        <input type="checkbox" class="switch-input" v-model="autoScroll">
                        <span class="switch-slider"></span>
                        <span class="switch-text">Auto-scroll</span>
                    </label>
                </div>
            </div>

            <div class="chart-wrapper">
                <div class="chart-canvas-container">
                    <canvas ref="chartCanvas"></canvas>
                </div>
                <div class="state-markers">
                    <div v-for="(state, index) in visibleStates" :key="`state-${index}`"
                         class="state-marker" :class="`state-${state.stateId}`"
                         :style="{ left: getEventPosition(state) + '%' }"
                         @mouseover="highlightState(state)" @mouseout="clearHighlight">
                    </div>
                </div>
            </div>

            <div class="chart-navigation">
                <div class="zoom-controls">
                    <button @click="zoomOut" class="btn btn-secondary btn-sm">
                        <i class="fas fa-search-minus"></i>
                    </button>
                    <span class="zoom-label">Zoom</span>
                    <button @click="zoomIn" class="btn btn-secondary btn-sm">
                        <i class="fas fa-search-plus"></i>
                    </button>
                </div>

                <div class="chart-options">
                    <div class="option-group">
                        <label class="option-label">
                            <input type="checkbox" v-model="showPressure" @change="updateChart">
                            <span class="option-color pressure-color"></span>
                            Pressure
                        </label>
                        <label class="option-label">
                            <input type="checkbox" v-model="showClenchThreshold" @change="updateChart">
                            <span class="option-color threshold-color"></span>
                            Clench Threshold
                        </label>
                        <label class="option-label">
                            <input type="checkbox" v-model="showArousal" @change="updateChart">
                            <span class="option-color arousal-color"></span>
                            Arousal %
                        </label>
                    </div>
                </div>

                <div class="scroll-controls">
                    <button @click="scrollLeft" class="btn btn-secondary btn-sm">
                        <i class="fas fa-chevron-left"></i>
                    </button>
                    <button @click="scrollRight" class="btn btn-secondary btn-sm">
                        <i class="fas fa-chevron-right"></i>
                    </button>
                </div>
            </div>
        </div>

        <div v-if="highlightedState" class="state-info">
            <div class="state-details">
                <h6>{{ getStateName(highlightedState.stateId) }}</h6>
                <div class="state-metrics">
                    <span>Time: {{ new Date(highlightedState.timestamp).toLocaleTimeString() }}</span>
                    <span>Pressure: {{ highlightedState.sensorData.pressure.toFixed(2) }}</span>
                    <span>Arousal: {{ highlightedState.sensorData.arousalPercent.toFixed(2) }}%</span>
                </div>
            </div>
        </div>

        <div class="states-panel">
            <h5>Event Marking</h5>
            <div class="state-buttons">
                <button v-for="state in arousalStates" :key="state.id"
                        @click="markArousalState(state.id)"
                        class="btn state-btn"
                        :class="[getStateButtonClass(state.id), { 'active': currentState === state.id }]"
                        :disabled="!isRecording">
                    {{ state.label }}
                </button>
            </div>

            <div v-if="recentStateLogs.length > 0" class="recent-events">
                <div class="recent-header">
                    <span>Recent Events</span>
                    <span class="event-count">{{ recentStateLogs.length }} logged</span>
                </div>
                <div class="recent-events-list">
                    <div v-for="(label, index) in recentStateLogs" :key="index"
                         class="recent-event" :class="`state-${label.stateId}`">
                        <span class="event-name">{{ getStateLabelById(label.stateId) }}</span>
                        <span class="event-time">{{ formatTime(label.timestamp) }}</span>
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>

<script>
import websocketService from '../services/WebSocketService';

export default {
    name: 'ArousalTracker',
    data() {
        return {
            isRecording: false,
            currentState: null,
            recentStateLogs: [],
            unsubscribeFunctions: [],

            // Visualization state
            chart: null,
            dataPoints: [],
            states: [],

            showPressure: true,
            showArousal: true,
            showClenchThreshold: false,

            autoScroll: true,
            viewWindow: {
                start: 0,
                size: 1000
            },

            maxDataPoints: 100000,
            highlightedState: null,

            arousalStates: [
                {id: 'baseline', label: 'Baseline', class: 'btn-secondary'},
                {id: 'arousing', label: 'Arousing', class: 'btn-primary'},
                {id: 'plateau', label: 'Plateau', class: 'btn-info'},
                {id: 'pre_edge', label: 'Pre-Edge', class: 'btn-warning'},
                {id: 'edge', label: 'Edge', class: 'btn-warning'},
                {id: 'refractory', label: 'Refractory', class: 'btn-secondary'},
                {id: 'pre_orgasm', label: 'Pre-Orgasm', class: 'btn-danger'},
                {id: 'orgasm', label: 'Orgasm', class: 'btn-danger'}
            ],

            currentData: {
                pressure: 0,
                clenchThreshold: 0,
                arousalLimit: 0,
                arousalPercent: 0
            },

            stateNames: {
                baseline: 'Baseline',
                arousing: 'Arousing',
                plateau: 'Plateau',
                pre_edge: 'Pre-Edge',
                edge: 'Edge',
                refractory: 'Refractory',
                pre_orgasm: 'Pre-Orgasm',
                orgasm: 'Orgasm'
            }
        };
    },
    computed: {
        hasData() {
            return this.dataPoints.length > 0;
        },
        visibleDataPoints() {
            if (this.dataPoints.length <= this.viewWindow.size) {
                return this.dataPoints;
            }

            const end = this.autoScroll
                ? this.dataPoints.length
                : Math.min(this.viewWindow.start + this.viewWindow.size, this.dataPoints.length);
            const start = end - this.viewWindow.size;

            return this.dataPoints.slice(Math.max(0, start), end);
        },
        visibleStates() {
            if (this.states.length === 0 || this.visibleDataPoints.length === 0) {
                return [];
            }

            const minTime = this.visibleDataPoints[0].timestamp;
            const maxTime = this.visibleDataPoints[this.visibleDataPoints.length - 1].timestamp;

            return this.states.filter(state =>
                state.timestamp >= minTime && state.timestamp <= maxTime
            );
        }
    },
    mounted() {
        this.initWebSocket();
        this.$nextTick(() => {
            this.initChart();
        });
    },
    beforeUnmount() {
        this.unsubscribeFunctions.forEach(unsubscribe => unsubscribe());
        if (this.chart) {
            this.chart.destroy();
            this.chart = null;
        }
    },
    methods: {
        initWebSocket() {
            this.unsubscribeFunctions.push(
                websocketService.subscribe('arousal_status', (data) => {
                    this.updateArousalData(data);
                })
            );
        },

        initChart() {
            if (!this.$refs.chartCanvas) {
                console.log("Chart canvas not found!");
                return;
            }

            const canvas = this.$refs.chartCanvas;
            const ctx = canvas.getContext('2d');
            if (!ctx) {
                console.error('Failed to get 2D context from canvas');
                return;
            }

            const chart = new window.Chart(ctx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [
                        {
                            label: 'Pressure',
                            data: [],
                            borderColor: '#0d6efd',
                            backgroundColor: 'rgba(13, 110, 253, 0.1)',
                            tension: 0.1,
                            pointRadius: 0,
                            borderWidth: 1.5,
                            yAxisID: 'y'
                        },
                        {
                            label: 'Clench Threshold',
                            data: [],
                            borderColor: '#ffc107',
                            backgroundColor: 'rgba(255, 193, 7, 0.1)',
                            tension: 0.1,
                            pointRadius: 0,
                            borderWidth: 1.5,
                            yAxisID: 'y'
                        },
                        {
                            label: 'Arousal %',
                            data: [],
                            borderColor: '#dc3545',
                            backgroundColor: 'rgba(220, 53, 69, 0.1)',
                            tension: 0.1,
                            pointRadius: 0,
                            borderWidth: 1.5,
                            yAxisID: 'y1'
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: false,
                    scales: {
                        x: {
                            ticks: {display: true},
                            grid: {display: false}
                        },
                        y: {
                            beginAtZero: true,
                            position: 'left',
                            title: {display: true, text: 'Pressure data'}
                        },
                        y1: {
                            beginAtZero: true,
                            position: 'right',
                            min: 0,
                            max: 100,
                            grid: {drawOnChartArea: true},
                            title: {display: true, text: 'Arousal'},
                            display: true
                        }
                    },
                    plugins: {
                        legend: {display: false},
                        tooltip: {
                            mode: 'index',
                            intersect: false,
                            callbacks: {
                                title: (tooltipItems) => {
                                    const index = tooltipItems[0].dataIndex;
                                    const dataPoint = this.visibleDataPoints[index];
                                    if (!dataPoint) return '';
                                    return new Date(dataPoint.timestamp).toLocaleTimeString();
                                }
                            }
                        }
                    }
                }
            });

            Object.seal(chart);
            this.chart = chart;
        },

        updateArousalData(data) {
            this.isRecording = data.active;
            if (!this.isRecording) {
                return;
            }

            const timestamp = Date.now();
            this.currentData = {
                pressure: data.pressure,
                clenchThreshold: data.clenchThreshold,
                arousalLimit: data.limit,
                arousalPercent: data.arousalPercent,
                timestamp
            };

            const newDataPoint = {
                timestamp,
                pressure: data.pressure,
                clenchThreshold: data.clenchThreshold,
                arousalLimit: data.limit,
                arousalPercent: data.arousalPercent
            };

            this.dataPoints.push(newDataPoint);
            if (this.dataPoints.length > this.maxDataPoints) {
                this.dataPoints.shift();
            }

            this.updateChart();
        },

        updateChart() {
            if (!this.chart || this.dataPoints.length === 0) {
                return;
            }

            const visibleData = this.visibleDataPoints;
            this.chart.data.labels = visibleData.map(d => new Date(d.timestamp).toLocaleTimeString());

            /*
            data-sets
            0 pressure 
            1 clench threshold
            2 arousal %
            */

            // y axis
            this.chart.data.datasets[0].data = this.showPressure ? visibleData.map(d => d.pressure) : [];
            this.chart.data.datasets[1].data = this.showClenchThreshold ? visibleData.map(d => d.clenchThreshold) : [];

            // y1 axis
            this.chart.data.datasets[2].data = this.showArousal ? visibleData.map(d => d.arousalPercent) : [];

            this.chart.data.datasets[0].hidden = !this.showPressure;
            this.chart.data.datasets[1].hidden = !this.showClenchThreshold;
            this.chart.data.datasets[2].hidden = !this.showArousal;

            this.chart.update();
        },

        getEventPosition(event) {
            if (this.visibleDataPoints.length === 0) {
                return 0;
            }

            const firstTime = this.visibleDataPoints[0].timestamp;
            const lastTime = this.visibleDataPoints[this.visibleDataPoints.length - 1].timestamp;
            const totalTimeRange = lastTime - firstTime;

            if (totalTimeRange === 0) {
                return 0;
            }

            return ((event.timestamp - firstTime) / totalTimeRange) * 100;
        },

        markArousalState(stateId) {
            if (!this.isRecording) {
                return;
            }

            const timestamp = Date.now();
            const state = {
                stateId,
                timestamp,
                sensorData: {...this.currentData}
            };

            this.states.push(state);

            this.recentStateLogs.unshift(state);
            if (this.recentStateLogs.length > 10) {
                this.recentStateLogs.pop();
            }

            this.currentState = stateId;
            this.$emit('send-notification', `${this.getStateName(stateId)} state marked`);
            this.updateChart();

            if (['edge', 'orgasm'].includes(stateId)) {
                setTimeout(() => {
                    this.currentState = null;
                }, 5000);
            }
        },

        getStateButtonClass(stateId) {
            const stateObj = this.arousalStates.find(s => s.id === stateId);
            return stateObj ? stateObj.class : 'btn-secondary';
        },

        getStateLabelById(stateId) {
            const state = this.arousalStates.find(s => s.id === stateId);
            return state ? state.label : stateId;
        },

        getStateName(stateId) {
            return this.stateNames[stateId] || stateId;
        },

        formatTime(timestamp) {
            const date = new Date(timestamp);
            return date.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit', second: '2-digit'});
        },

        highlightState(state) {
            this.highlightedState = state;
        },

        clearHighlight() {
            this.highlightedState = null;
        },

        clearData() {
            if (!confirm('Are you sure you want to clear all recorded data? This will also clear marked events.')) {
                return;
            }

            this.dataPoints = [];
            this.states = [];
            this.recentStateLogs = [];
            this.highlightedState = null;

            if (this.chart) {
                this.updateChart();
            }

            this.$emit('send-notification', 'All data cleared');
        },

        exportCSV() {
            if (this.dataPoints.length === 0) {
                return;
            }

            const headers = [
                'Timestamp',
                'Pressure',
                'ClenchThreshold',
                'ArousalLimit',
                'ArousalPercent',
                'State'
            ];

            const csvContent = [headers.join(',')];

            // Combine all data points and events into a chronological sequence
            const allPoints = [...this.dataPoints.map(dp => ({
                ...dp,
                stateId: null,
            }))];

            this.states.forEach(state => {
                // Find the closest data point to this state by timestamp
                const closestIndex = this.findClosestDataPointIndex(state.timestamp, allPoints);
                if (closestIndex >= 0) {
                    allPoints[closestIndex].stateId = state.stateId;
                }
            });

            allPoints.sort((a, b) => a.timestamp - b.timestamp);
            allPoints.forEach(point => {
                const date = new Date(point.timestamp).toISOString();
                const row = [
                    date,
                    point.pressure.toFixed(2),
                    point.clenchThreshold?.toFixed(2) || '',
                    point.arousalLimit.toFixed(2),
                    point.arousalPercent?.toFixed(2) || '',
                    point.stateId || '',
                ].join(',');
                csvContent.push(row);
            });

            const blob = new Blob([csvContent.join('\n')], {type: 'text/csv;charset=utf-8;'});
            const url = URL.createObjectURL(blob);
            const link = document.createElement('a');

            const now = new Date();
            const filename = `arousal_data_${now.toISOString().replace(/[:.]/g, '-')}.csv`;

            link.setAttribute('href', url);
            link.setAttribute('download', filename);
            link.style.visibility = 'hidden';

            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);

            this.$emit('send-notification', 'Data exported to CSV');
        },

        findClosestDataPointIndex(timestamp, points = null) {
            const dataToSearch = points || this.dataPoints;
            if (dataToSearch.length === 0) {
                return -1;
            }

            let closestIndex = 0;
            let closestDiff = Math.abs(dataToSearch[0].timestamp - timestamp);

            for (let i = 1; i < dataToSearch.length; i++) {
                const diff = Math.abs(dataToSearch[i].timestamp - timestamp);
                if (diff < closestDiff) {
                    closestDiff = diff;
                    closestIndex = i;
                }
            }

            return closestIndex;
        },

        zoomIn() {
            this.viewWindow.size = Math.max(20, Math.floor(this.viewWindow.size * 0.7));
            this.updateChart();
        },

        zoomOut() {
            this.viewWindow.size = Math.min(this.dataPoints.length, Math.floor(this.viewWindow.size * 1.5));
            this.updateChart();
        },

        scrollLeft() {
            if (!this.autoScroll) {
                this.viewWindow.start = Math.max(0, this.viewWindow.start - Math.floor(this.viewWindow.size * 0.3));
                this.updateChart();
            }
        },

        scrollRight() {
            if (!this.autoScroll) {
                const maxStart = Math.max(0, this.dataPoints.length - this.viewWindow.size);
                this.viewWindow.start = Math.min(maxStart, this.viewWindow.start + Math.floor(this.viewWindow.size * 0.3));
                this.updateChart();
            }
        }
    },
}
</script>

<style scoped>
.analytics-controls {
    display: flex;
    align-items: center;
    gap: 1rem;
}

.status-recording {
    background: var(--danger-gradient);
    color: white;
    animation: pulse 2s infinite;
}

.status-inactive {
    background: var(--surface-elevated);
    color: var(--text-secondary);
    border: 1px solid var(--border);
}

.chart-container {
    background: var(--surface-elevated);
    border-radius: var(--border-radius);
    padding: 1.5rem;
    margin: 1.5rem 0;
    border: 1px solid var(--border);
}

.chart-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.chart-header h5 {
    margin: 0;
    color: var(--text-primary);
    font-weight: 600;
}

.chart-controls {
    display: flex;
    align-items: center;
    gap: 1rem;
}

.chart-wrapper {
    position: relative;
    width: 100%;
    height: 300px;
    margin: 1rem 0;
}

.chart-canvas-container {
    width: 100%;
    height: 100%;
    position: relative;
}

.state-markers {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    pointer-events: none;
}

.state-marker {
    position: absolute;
    top: 0;
    height: 100%;
    width: 2px;
    cursor: pointer;
    pointer-events: all;
    opacity: 0.7;
    transition: all 0.2s ease;
}

.state-marker:hover {
    width: 4px;
    opacity: 1;
}

.state-baseline {
    background-color: #6c757d;
}

.state-arousing {
    background-color: #0d6efd;
}

.state-plateau {
    background-color: #0dcaf0;
}

.state-pre_edge {
    background-color: #ffc107;
}

.state-edge {
    background-color: #fd7e14;
}

.state-refractory {
    background-color: #6c757d;
}

.state-pre_orgasm {
    background-color: #dc3545;
}

.state-orgasm {
    background-color: #dc3545;
}

.chart-navigation {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin: 1rem 0;
}

.zoom-controls, .scroll-controls {
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.zoom-label {
    font-size: 0.875rem;
    color: var(--text-secondary);
}

.btn-sm {
    padding: 0.375rem 0.75rem;
    font-size: 0.875rem;
}

.option-group {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
}

.option-label {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    font-size: 0.875rem;
    color: var(--text-secondary);
    cursor: pointer;
}

.option-color {
    width: 12px;
    height: 12px;
    border-radius: 2px;
}

.pressure-color {
    background-color: #0d6efd;
}

.threshold-color {
    background-color: #ffc107;
}

.arousal-color {
    background-color: #dc3545;
}

.state-info {
    background: var(--surface-elevated);
    border: 1px solid var(--border);
    border-radius: var(--border-radius);
    padding: 1rem;
    margin: 1rem 0;
}

.state-details h6 {
    margin-bottom: 0.5rem;
    color: var(--text-primary);
    font-weight: 600;
}

.state-metrics {
    display: flex;
    gap: 1rem;
    font-size: 0.875rem;
    color: var(--text-secondary);
}

.states-panel {
    background: var(--surface-elevated);
    border: 1px solid var(--border);
    border-radius: var(--border-radius);
    padding: 1.5rem;
    margin-top: 1.5rem;
}

.states-panel h5 {
    margin-bottom: 1rem;
    color: var(--text-primary);
    font-weight: 600;
}

.state-buttons {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
    gap: 0.5rem;
    margin-bottom: 1.5rem;
}

.state-btn {
    padding: 0.5rem 1rem;
    font-size: 0.875rem;
    border-radius: var(--border-radius);
    transition: all 0.2s ease;
}

.state-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}

.state-btn.active {
    box-shadow: 0 0 0 2px var(--primary-gradient);
}

.recent-events {
    border-top: 1px solid var(--border);
    padding-top: 1rem;
}

.recent-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.75rem;
}

.recent-header span:first-child {
    font-weight: 500;
    color: var(--text-primary);
}

.event-count {
    font-size: 0.875rem;
    color: var(--text-secondary);
}

.recent-events-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    max-height: 120px;
    overflow-y: auto;
}

.recent-event {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.5rem 0.75rem;
    border-radius: var(--border-radius);
    font-size: 0.875rem;
    color: white;
}

.event-time {
    opacity: 0.8;
    font-size: 0.75rem;
}

@media (max-width: 768px) {
    .analytics-controls {
        flex-direction: column;
        align-items: stretch;
        gap: 0.5rem;
    }

    .chart-controls {
        flex-direction: column;
        align-items: stretch;
        gap: 0.5rem;
    }

    .chart-navigation {
        flex-direction: column;
        gap: 1rem;
    }

    .state-buttons {
        grid-template-columns: 1fr 1fr;
    }

    .state-metrics {
        flex-direction: column;
        gap: 0.25rem;
    }
}
</style>
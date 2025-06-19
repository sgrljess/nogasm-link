<!--suppress ALL -->
<template>
    <div class="card mb-4">
        <div class="card-header bg-primary text-white d-flex justify-content-between align-items-center">
            <div class="card-title">
                <i class="fas fa-upload"></i>
                Over-The-Air Updates
            </div>
            <div class="d-flex align-items-center">
                <div v-if="updateStatus.active" class="badge bg-warning">Updating</div>
                <div v-else class="badge bg-success">Ready</div>
            </div>
        </div>

        <div class="card-body">
            <!-- WiFi Warning -->
            <!--            <div v-if="!updateReady" class="alert alert-warning">-->
            <!--                <i class="fas fa-exclamation-triangle me-2"></i>-->
            <!--                WiFi connection required for updates-->
            <!--            </div>-->

            <!-- Update in Progress -->
            <div v-if="updateStatus.active" class="mb-4">
                <h5>Update in Progress</h5>
                <div class="alert alert-info">
                    <strong>{{ getUpdateTypeDisplay() }}</strong> update in progress...
                    <div v-if="updateStatus.error" class="text-danger mt-2">
                        <strong>Error:</strong> {{ updateStatus.error }}
                    </div>
                </div>

                <div class="progress mb-3" style="height: 25px;">
                    <div class="progress-bar progress-bar-striped progress-bar-animated"
                         :class="getProgressBarClass()"
                         role="progressbar"
                         :style="{ width: updateStatus.percentage + '%' }"
                         :aria-valuenow="updateStatus.percentage"
                         aria-valuemin="0"
                         aria-valuemax="100">
                        {{ updateStatus.percentage.toFixed(1) }}%
                    </div>
                </div>

                <div class="text-muted small">
                    {{ formatBytes(updateStatus.current) }} / {{ formatBytes(updateStatus.total) }}
                    <span v-if="updateStatus.md5Hash" class="ms-2">
                        (MD5: {{ updateStatus.md5Hash.substring(0, 8) }}...)
                    </span>
                </div>
            </div>

            <!-- Ready State -->
            <div v-else>
                <!-- Status Info -->
                <div class="row mb-4">
                    <div class="col-md-6">
                        <div class="metric">
                            <div class="metric-label">Hostname</div>
                            <div class="metric-value">{{ updateStatus.hostname }}</div>
                        </div>
                    </div>
                    <div class="col-md-6">
                        <div class="metric">
                            <div class="metric-label">IP Address</div>
                            <div class="metric-value">{{ updateStatus.ip }}</div>
                        </div>
                    </div>
                </div>

                <!-- File Upload Section -->
                <div class="mb-4">
                    <h5>Select Update File</h5>

                    <!-- Drag and Drop Area -->
                    <div class="drop-zone"
                         :class="{ 'drag-over': isDragOver, 'has-file': selectedFile }"
                         @dragover.prevent="onDragOver"
                         @dragleave.prevent="onDragLeave"
                         @drop.prevent="onDrop"
                         @click="triggerFileInput">

                        <input type="file"
                               ref="fileInput"
                               class="d-none"
                               accept=".bin,.spiffs"
                               @change="onFileSelected">

                        <div v-if="!selectedFile" class="drop-zone-content">
                            <i class="fas fa-cloud-upload-alt fa-3x mb-3 text-muted"></i>
                            <p class="mb-2"><strong>Drop your file here</strong></p>
                            <p class="text-muted">or click to select a file</p>
                            <small class="text-muted">Supports .bin (firmware) and .spiffs (filesystem) files</small>
                        </div>

                        <div v-else class="selected-file-info">
                            <div class="d-flex align-items-center justify-content-between mb-2">
                                <div class="d-flex align-items-center">
                                    <i class="fas fa-file-alt fa-2x me-3 text-primary"></i>
                                    <div>
                                        <h6 class="mb-0">{{ selectedFile.name }}</h6>
                                        <small class="text-muted">{{ formatBytes(selectedFile.size) }} •
                                            {{ getFileType() }}</small>
                                    </div>
                                </div>
                                <button type="button"
                                        class="btn btn-sm btn-outline-danger"
                                        @click.stop="clearFile">
                                    <i class="fas fa-times"></i>
                                </button>
                            </div>

                            <!-- MD5 Calculation -->
                            <div v-if="calculatingMd5" class="text-center py-2">
                                <div class="spinner-border spinner-border-sm me-2" role="status"></div>
                                <span class="text-muted">Calculating MD5 hash...</span>
                            </div>
                            <div v-else-if="md5Hash" class="mt-2">
                                <small class="text-muted">MD5: <code>{{ md5Hash }}</code></small>
                            </div>
                        </div>
                    </div>

                    <!-- File Type Warning -->
                    <div v-if="selectedFile && getFileType() === 'Firmware'" class="alert alert-warning mt-3">
                        <i class="fas fa-exclamation-triangle me-2"></i>
                        <strong>Warning:</strong> This will overwrite the running firmware. Ensure you have the correct
                        file.
                    </div>
                </div>

                <!-- Action Buttons -->
                <div class="d-flex justify-content-between align-items-center">
                    <button @click="startSession"
                            class="btn btn-outline-primary"
                            :disabled="starting">
                        <span v-if="starting" class="spinner-border spinner-border-sm me-2" role="status"></span>
                        {{ starting ? 'Preparing...' : 'Prepare OTA Session' }}
                    </button>

                    <div class="d-flex gap-2">
                        <button @click="refreshStatus" class="btn btn-outline-secondary">
                            <i class="fas fa-sync-alt"></i>
                            Refresh
                        </button>

                        <button @click="uploadFile"
                                class="btn btn-primary"
                                :disabled="!selectedFile || uploading || calculatingMd5 || !md5Hash">
                            <span v-if="uploading" class="spinner-border spinner-border-sm me-2" role="status"></span>
                            {{ uploading ? 'Uploading...' : 'Upload File' }}
                        </button>
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>

<script>
import websocketService from '../services/WebSocketService';

export default {
    name: 'NogasmUpdate',
    data() {
        return {
            updateStatus: {
                active: false,
                state: 0, // 0=IDLE, 1=STARTING, 2=IN_PROGRESS, 3=COMPLETED, 4=ERROR
                mode: 0,  // 0=FIRMWARE, 1=FILESYSTEM
                percentage: 0,
                current: 0,
                total: 0,
                error: '',
                md5Hash: '',
                hostname: '',
                ip: '',
                hasError: false
            },
            starting: false,
            uploading: false,
            selectedFile: null,
            md5Hash: '',
            calculatingMd5: false,
            isDragOver: false,
            unsubscribeFunctions: [],
        }
    },
    computed: {
        updateReady() {
            return this.updateStatus.ip && this.updateStatus.hostname;
        }
    },
    mounted() {
        this.initWebSocket();
        this.fetchOTAStatus();
    },
    beforeUnmount() {
        this.unsubscribeFunctions.forEach(unsubscribe => unsubscribe());
    },
    methods: {
        initWebSocket() {
            websocketService.connect().catch(() => {
                this.$emit('send-notification', 'Failed to connect to WebSocket', 'error');
            });

            this.unsubscribeFunctions.push(
                websocketService.subscribe('ota_status', (data) => {
                    this.updateOTAStatus(data);
                })
            );
        },

        async refreshStatus() {
            await this.fetchOTAStatus();
            this.$emit('send-notification', 'OTA status refreshed', 'info');
        },

        getUpdateTypeDisplay() {
            return this.updateStatus.mode === 0 ? 'Firmware' : 'Filesystem';
        },

        getProgressBarClass() {
            if (this.updateStatus.state === 4) { // ERROR
                return 'bg-danger';
            } else if (this.updateStatus.state === 3) { // COMPLETED
                return 'bg-success';
            } else {
                return 'bg-primary';
            }
        },

        formatBytes(bytes) {
            if (!bytes || bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
        },

        // Drag and Drop Handlers
        onDragOver() {
            this.isDragOver = true;
        },

        onDragLeave() {
            this.isDragOver = false;
        },

        onDrop(event) {
            this.isDragOver = false;
            const files = event.dataTransfer.files;
            if (files.length > 0) {
                this.handleFileSelection(files[0]);
            }
        },

        triggerFileInput() {
            this.$refs.fileInput.click();
        },

        onFileSelected(event) {
            const files = event.target.files;
            if (files.length > 0) {
                this.handleFileSelection(files[0]);
            }
        },

        async handleFileSelection(file) {
            // Validate file type
            if (!file.name.endsWith('.bin') && !file.name.endsWith('.spiffs')) {
                this.$emit('send-notification', 'Invalid file type. Please select a .bin or .spiffs file.', 'error');
                return;
            }

            this.selectedFile = file;
            this.md5Hash = '';

            // Calculate MD5 hash
            await this.calculateMD5(file);
        },

        async calculateMD5(file) {
            this.calculatingMd5 = true;
            try {
                file.arrayBuffer().var
                hash = CryptoJS.MD5("Message");

                const arrayBuffer = await file.arrayBuffer();
                const hashBuffer = await crypto.subtle.digest('MD5', arrayBuffer);
                const hashArray = Array.from(new Uint8Array(hashBuffer));
                this.md5Hash = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
            } catch (error) {
                console.error('MD5 calculation failed:', error);
                this.$emit('send-notification', 'Failed to calculate file hash', 'error');
                this.md5Hash = '';
            } finally {
                this.calculatingMd5 = false;
            }
        },

        clearFile() {
            this.selectedFile = null;
            this.md5Hash = '';
            this.$refs.fileInput.value = '';
        },

        getFileType() {
            if (!this.selectedFile) {
                return null;
            }

            if (this.selectedFile.name.endsWith('.bin')) {
                return 'Firmware';
            } else if (this.selectedFile.name.endsWith('.spiffs')) {
                return 'Filesystem';
            } else {
                return 'Unknown';
            }
        },

        async uploadFile() {
            if (!this.selectedFile || !this.md5Hash) {
                this.$emit('send-notification', 'Please select a file and wait for hash calculation', 'error');
                return;
            }

            this.uploading = true;

            try {
                const formData = new FormData();
                formData.append('file', this.selectedFile);

                // Add MD5 hash as query parameter
                const url = `/api/update/upload?md5=${this.md5Hash}`;

                const response = await fetch(url, {
                    method: 'POST',
                    body: formData
                });

                if (!response.ok) {
                    throw new Error('Upload failed');
                }

                const result = await response.json();
                if (result.success) {
                    this.$emit('send-notification', 'Upload successful! Device will restart.', 'success');
                    this.clearFile();
                } else {
                    throw new Error(result.message || 'Upload failed');
                }
            } catch (error) {
                this.$emit('send-notification', 'Upload failed: ' + error.message, 'error');
            } finally {
                this.uploading = false;
            }
        },

        updateOTAStatus(data) {
            this.updateStatus = {...this.updateStatus, ...data};

            if (data.state === 1) { // STARTING
                this.$emit('send-notification', `${this.getUpdateTypeDisplay()} update starting...`, 'info');
            } else if (data.state === 3) { // COMPLETED
                this.$emit('send-notification', `${this.getUpdateTypeDisplay()} update completed successfully!`, 'success');
            } else if (data.state === 4 && data.error) { // ERROR
                this.$emit('send-notification', `OTA Error: ${data.error}`, 'error');
            }
        },

        async fetchOTAStatus() {
            try {
                const response = await fetch('/api/update/status');
                if (!response.ok) {
                    throw new Error('Failed to fetch OTA status');
                }

                const data = await response.json();
                this.updateStatus = {...this.updateStatus, ...data};
            } catch (error) {
                this.$emit('send-notification', 'Error fetching OTA status', 'error');
            }
        },

        async startSession() {
            this.starting = true;
            try {
                const response = await fetch('/api/update/start', {
                    method: 'POST'
                });

                if (!response.ok) {
                    throw new Error('Failed to start OTA session');
                }

                const result = await response.json();
                if (result.success) {
                    this.$emit('send-notification', 'OTA system ready for web uploads.', 'success');
                    this.updateStatus = {...this.updateStatus, ...result};
                } else {
                    throw new Error(result.message || 'Failed to prepare OTA');
                }
            } catch (error) {
                this.$emit('send-notification', 'Error starting OTA session: ' + error.message, 'error');
            } finally {
                this.starting = false;
            }
        }
    }
}
</script>

<style scoped>
.metric {
    text-align: center;
    padding: 1rem;
    background: #f8f9fa;
    border-radius: 8px;
    border: 1px solid #e9ecef;
}

.metric-value {
    font-size: 1.25rem;
    font-weight: 600;
    color: #495057;
}

.metric-label {
    font-size: 0.875rem;
    color: #6c757d;
    margin-bottom: 0.25rem;
}

.drop-zone {
    border: 2px dashed #dee2e6;
    border-radius: 8px;
    padding: 2rem;
    text-align: center;
    cursor: pointer;
    transition: all 0.3s ease;
    background-color: #f8f9fa;
}

.drop-zone:hover {
    border-color: #0d6efd;
    background-color: #f0f8ff;
}

.drop-zone.drag-over {
    border-color: #0d6efd;
    background-color: #e7f3ff;
    transform: scale(1.02);
}

.drop-zone.has-file {
    border-color: #198754;
    background-color: #f8fff9;
    padding: 1.5rem;
}

.drop-zone-content {
    pointer-events: none;
}

.selected-file-info {
    text-align: left;
}

.alert h6 {
    margin-bottom: 0.5rem;
}

.alert ul {
    padding-left: 1.5rem;
}

code {
    background-color: #f8f9fa;
    padding: 0.2rem 0.4rem;
    border-radius: 0.25rem;
    font-family: monospace;
    font-size: 0.875em;
}
</style>
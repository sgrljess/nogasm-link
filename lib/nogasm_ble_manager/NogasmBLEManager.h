#ifndef NOGASM_BLE_MANAGER_H
#define NOGASM_BLE_MANAGER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NogasmConfig.h>
#include <DeviceProtocol.h>
#include <memory>
#include <vector>
#include <functional>

#define BLE_STABILISE_DELAY_MS 10

// BLE connection states
enum BLEConnectionState
{
  BLE_IDLE,
  BLE_SCANNING,
  BLE_CONNECTING,
  BLE_FINDING_SERVICE,
  BLE_CONNECTED,
  BLE_FAILED,
  BLE_RECONNECTING,
};

enum  CompatibleDeviceProtocol
{
  UNKNOWN,
  LOVENSE
};

struct CompatibleDevice
{
  CompatibleDeviceProtocol protocol;
  std::string address;
  std::string addressType;
  std::string name;
  bool connected = false;
  String modelDisplayName;
  String firmwareVersion;
  int batteryLevel = -1;
  int8_t rssi = -1;
};

class NogasmBLEManager
{
 public:
  explicit NogasmBLEManager(NogasmConfig& config);
  ~NogasmBLEManager();

  void begin(const char* deviceName);
  void connectToDevice(const std::string& address);
  void startScan(uint32_t durationMs = 30000);
  void stopScan();

  // Device control methods
  bool setVibrationLevel(uint8_t level) const;
  bool setRotationLevel(uint8_t level) const;
  bool changeRotationDirection() const;
  bool setAirLevel(uint8_t level) const;
  bool adjustAirLevelRelative(bool inflate, uint8_t amount) const;
  bool powerOffDevice() const;
  bool queryBatteryLevel() const;

  // Device information methods
  const std::vector<CompatibleDevice>& getDevices() const;
  const CompatibleDevice* getCurrentDevice() const;

  // Status methods
  BLEConnectionState getState() const;
  static std::string getStateString(BLEConnectionState state) ;
  bool isScanning() const;
  bool isConnectedState() const;

  // Connection management
  void update();
  void disconnectAndCleanupClient();
  void updateStatus(BLEConnectionState newState, const std::string& reason);
  void onStatusChange(const std::function<void(BLEConnectionState)>& callback);

  bool connectToLastDevice();
  bool shouldTryReconnect();

  class ScanCallbacks final : public NimBLEScanCallbacks
  {
   public:
    explicit ScanCallbacks(NogasmBLEManager* manager) : _manager(manager)
    {
    }

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;

   private:
    NogasmBLEManager* _manager;
  };

 private:
  class ClientCallbacks final : public NimBLEClientCallbacks
  {
   public:
    explicit ClientCallbacks(NogasmBLEManager* manager) : _manager(manager)
    {
    }

    void onConnect(NimBLEClient* client) override;
    void onDisconnect(const NimBLEClient* client) const;
    void onConnectFail(NimBLEClient* client, int reason) override;
    bool onConnParamsUpdateRequest(NimBLEClient* client, const ble_gap_upd_params* params) override;

   private:
    NogasmBLEManager* _manager;
  };

  // Private methods
  bool hasTimedOut(unsigned long timeoutMs);
  bool findCompatibleService();
  bool findCharacteristics();
  void notifyStatusChange() const;
  void processConnectionStateMachine();
  void updateDeviceState(bool connected);
  bool createProtocolForDevice(const CompatibleDevice& device);

  // Private members
  NogasmConfig& _config;
  BLEConnectionState _state = BLE_IDLE;
  std::vector<CompatibleDevice> _devices;
  CompatibleDevice _currentDevice;

  // Device protocol - polymorphic interface for different device types
  std::unique_ptr<DeviceProtocol> _deviceProtocol;

  NimBLEClient* _client = nullptr;
  NimBLERemoteService* _service = nullptr;
  NimBLERemoteCharacteristic* _writeCharacteristic = nullptr;
  NimBLERemoteCharacteristic* _notifyCharacteristic = nullptr;
  ScanCallbacks* _scanCallbacks = nullptr;
  ClientCallbacks* _clientCallbacks = nullptr;

  unsigned long _lastRssiCheck = 0;
  unsigned long _lastStateChangeTime = 0;
  unsigned long _scanEndTime = 0;
  unsigned long _connectionTimeoutMs = 15000;

  bool _scanActive = false;
  bool _autoConnectEnabled = true;
  bool _autoReconnectEnabled = true;
  int _reconnectAttempts = 0;
  static constexpr int MAX_RECONNECT_ATTEMPTS = 5;

  std::function<void(BLEConnectionState)> _statusCallback = nullptr;
};

#endif
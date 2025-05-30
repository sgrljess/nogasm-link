// ReSharper disable CppExpressionWithoutSideEffects
// ReSharper disable CppDFAUnreachableCode
// ReSharper disable CppDFAUnreachableFunctionCall

#include "NogasmBLEManager.h"
#include "LovenseProtocol.h"
#include "Util.h"
#include <cstdarg>

static NogasmBLEManager* s_instance = nullptr;

NogasmBLEManager::NogasmBLEManager(NogasmConfig& config) : _config(config)
{
  _scanCallbacks = new ScanCallbacks(this);
  _clientCallbacks = new ClientCallbacks(this);

  // Set configuration parameters
  _connectionTimeoutMs = config.getConnectionTimeout();
  _autoConnectEnabled = config.getAutoConnect();
  _autoReconnectEnabled = config.getAutoReconnect();

  s_instance = this;
}

NogasmBLEManager::~NogasmBLEManager()
{
  disconnectAndCleanupClient();

  delete _scanCallbacks;
  delete _clientCallbacks;

  s_instance = nullptr;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void NogasmBLEManager::begin(const char* deviceName) // NOLINT(*-convert-member-functions-to-static)
{
  NimBLEDevice::init(deviceName);
  Util::logInfo("Nogasm BLE Manager initialized with name: %s", deviceName);
}

void NogasmBLEManager::startScan(const uint32_t durationMs)
{
  // we can only start scans from idle or failed states
  if (_state != BLE_IDLE && _state != BLE_FAILED)
  {
    Util::logInfo("Cannot start scan, BLE is busy. Current state: %s", getStateString(_state).c_str());
    return;
  }

  Util::logDebug("Starting BLE scan for compatible devices...");

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(_scanCallbacks);
  pScan->setActiveScan(true);
  pScan->setMaxResults(0);
  pScan->start(durationMs, false, true);

  // change state to scanning, result handling is async
  updateStatus(BLE_SCANNING, "Start scanning");
}

void NogasmBLEManager::stopScan()
{
  if (_scanActive)
  {
    Util::logDebug("Manually stopping scan");
    NimBLEDevice::getScan()->stop();
    _scanActive = false;
  }
}

std::string NogasmBLEManager::getStateString(const BLEConnectionState state)
{
  switch (state)
  {
    case BLE_IDLE:
      return "IDLE";
    case BLE_SCANNING:
      return "SCANNING";
    case BLE_CONNECTING:
      return "CONNECTING";
    case BLE_FINDING_SERVICE:
      return "FINDING_SERVICE";
    case BLE_CONNECTED:
      return "CONNECTED";
    case BLE_FAILED:
      return "FAILED";
    case BLE_RECONNECTING:
      return "RECONNECTING";
    default:
      return "UNKNOWN";
  }
}

bool NogasmBLEManager::createProtocolForDevice(const CompatibleDevice& device)
{
  if (!_deviceProtocol)
  {
    // create default protocol
    _deviceProtocol = std::unique_ptr<DeviceProtocol>(new LovenseProtocol());
  }

  if (device.protocol == LOVENSE)
  {
    Util::logTrace("Setting up lovense protocol for device: %s", device.name.c_str());
    _deviceProtocol.reset(new LovenseProtocol());
  }
  else
  {
    Util::logDebug("  > Device type not supported (or not implemented)");
    return false;
  }

  _deviceProtocol->setDeviceInfoCallback(
    [this](const DeviceInfo& info)
    {
      Util::logDebug("Got device info: Model=%s, Battery=%d%, Firmware=%s", info.modelType.c_str(), info.batteryLevel, info.firmwareVersion.c_str());
      _currentDevice.modelDisplayName = info.modelType;
      _currentDevice.firmwareVersion = info.firmwareVersion;
    });

  _deviceProtocol->setBatteryLevelCallback(
    [this](const int level)
    {
      Util::logDebug("Got battery level: %d%", level);
      _currentDevice.batteryLevel = level;
    });

  return true;
}

void NogasmBLEManager::connectToDevice(const std::string& address)
{
  // only allow connections when we transition from idle, scanning, reconnecting or failed.
  if (_state != BLE_IDLE && _state != BLE_SCANNING && _state != BLE_FAILED && _state != BLE_RECONNECTING)
  {
    Util::logInfo("Cannot connect, BLE is busy. Current state: %s", getStateString(_state).c_str());
    return;
  }

  if (_scanActive)
  {
    stopScan();
  }

  // find the device in the scan results
  bool deviceFound = false;
  for (const auto& device : _devices)
  {
    if (device.address == address)
    {
      if (!createProtocolForDevice(device))
      {
        updateStatus(BLE_FAILED, "Failed to create device protocol!");
        break;
      }

      _currentDevice = device;
      deviceFound = true;
      break;
    }
  }

  if (!deviceFound)
  {
    Util::logDebug("Device not found in scan results: %s", address.c_str());
    updateStatus(BLE_FAILED, "Device not found in scan results");
    return;
  }

  Util::logDebug("Connecting to %s (%s) - type: %s",  //
    _currentDevice.name.c_str(), _currentDevice.address.c_str(), _currentDevice.addressType.c_str());

  // clean up existing client if any
  disconnectAndCleanupClient();

  // create fresh callback objects for each connection
  delete _clientCallbacks;
  _clientCallbacks = new ClientCallbacks(this);

  // protect against overused clients
  if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS)
  {
    updateStatus(BLE_FAILED, "Max clients reached - no more connections available");
    return;
  }

  // create a new client
  _client = NimBLEDevice::createClient();
  _client->setClientCallbacks(_clientCallbacks);
  _client->setConnectionParams(12, 15, 0, 250);
  _client->setConnectTimeout(_config.getConnectionTimeout());

  // and hand over to state machine
  updateStatus(BLE_CONNECTING, "Connecting to device...");
}

void NogasmBLEManager::updateDeviceState(const bool connected)
{
  // update the current device
  _currentDevice.connected = connected;

  // find and update the same device in the devices vector
  for (auto& device : _devices)
  {
    if (device.address == _currentDevice.address)
    {
      device.connected = connected;
      Util::logTrace("  > Updated device %s in vector to connected=%d", device.name.c_str(), connected);
      break;
    }
  }
}

void NogasmBLEManager::disconnectAndCleanupClient()
{
  Util::logDebug("  > Cleaning up ble client...");

  updateDeviceState(false);

  if (_deviceProtocol)
  {
    _deviceProtocol->clearCharacteristics();
  }

  if (_client != nullptr)
  {
    if (_client->isConnected())
    {
      _client->disconnect();
    }

    // set the callbacks to nullptr first to prevent any pending callbacks
    _client->setClientCallbacks(nullptr);

    NimBLEDevice::deleteClient(_client);
    _client = nullptr;
  }

  _writeCharacteristic = nullptr;
  _notifyCharacteristic = nullptr;
  _service = nullptr;

  delay(100);
}

bool NogasmBLEManager::setVibrationLevel(const uint8_t level) const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->setVibration(level);
}

bool NogasmBLEManager::setRotationLevel(const uint8_t level) const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->setRotation(level);
}

bool NogasmBLEManager::changeRotationDirection() const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->changeRotationDirection();
}

bool NogasmBLEManager::setAirLevel(const uint8_t level) const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->setAirLevel(level);
}

bool NogasmBLEManager::adjustAirLevelRelative(const bool inflate, const uint8_t amount) const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->adjustAirLevelRelative(inflate, amount);
}

bool NogasmBLEManager::powerOffDevice() const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->powerOff();
}

bool NogasmBLEManager::queryBatteryLevel() const
{
  if (!_deviceProtocol || !_deviceProtocol->isReady())
  {
    return false;
  }

  return _deviceProtocol->queryBatteryLevel();
}

const std::vector<CompatibleDevice>& NogasmBLEManager::getDevices() const
{
  return _devices;
}

const CompatibleDevice* NogasmBLEManager::getCurrentDevice() const
{
  if (_currentDevice.connected)
  {
    return &_currentDevice;
  }

  return nullptr;
}

BLEConnectionState NogasmBLEManager::getState() const
{
  return _state;
}

bool NogasmBLEManager::isScanning() const
{
  return _scanActive;
}

bool NogasmBLEManager::isConnectedState() const
{
  return _state == BLE_CONNECTED && _currentDevice.connected;
}

void NogasmBLEManager::onStatusChange(const std::function<void(BLEConnectionState)>& callback)
{
  _statusCallback = callback;
}

bool NogasmBLEManager::connectToLastDevice()
{
  const String lastDeviceAddr = _config.getLastConnectedDevice();
  if (lastDeviceAddr.isEmpty())
  {
    Util::logDebug("  > No last device to reconnect to");
    return false;
  }

  bool deviceFound = false;
  for (const auto& device : _devices)
  {
    if (device.address == lastDeviceAddr.c_str())
    {
      deviceFound = true;
      break;
    }
  }

  if (!deviceFound)
  {
    Util::logDebug("  > Last device not found in scan results");
    return false;
  }

  Util::logDebug("  > Attempting to reconnect to last device: %s", lastDeviceAddr.c_str());
  connectToDevice(lastDeviceAddr.c_str());
  return true;
}

bool NogasmBLEManager::shouldTryReconnect()
{
  if (!_autoReconnectEnabled)
  {
    Util::logDebug("Auto-reconnect not enabled");
    return false;
  }

  Util::logDebug("Trying to reconnect...");
  if (++_reconnectAttempts > MAX_RECONNECT_ATTEMPTS)
  {
    Util::logDebug("  > Max reconnection attempts reached");
    _reconnectAttempts = 0;
    return false;
  }

  Util::logDebug("  > Attempt %d of %d", _reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
  return true;
}

void NogasmBLEManager::update()
{
  processConnectionStateMachine();
  delay(BLE_STABILISE_DELAY_MS);
}

bool NogasmBLEManager::hasTimedOut(const unsigned long timeoutMs)
{
  if (Util::hasTimeExpired(timeoutMs, _lastStateChangeTime))
  {
    updateStatus(BLE_FAILED, "Timed out");
    return true;
  }

  return false;
}

void NogasmBLEManager::processConnectionStateMachine()
{
  switch (_state)
  {
    case BLE_CONNECTING:
    {
      if (hasTimedOut(_connectionTimeoutMs))
      {
        Util::logDebug("Connection timeout after %d ms", _connectionTimeoutMs);
        return;
      }

      if (_client != nullptr && !_client->isConnected())
      {
        Util::logDebug("Attempting to connect...");

        const uint8_t addressType = _currentDevice.addressType == "RANDOM" ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
        const auto address = NimBLEAddress(_currentDevice.address, addressType);
        const bool connectResult = _client->connect(address, true, true, true);

        if (!connectResult)
        {
          updateStatus(BLE_FAILED, "connect() failed");
        }
        else
        {
          updateStatus(BLE_FINDING_SERVICE, "connect() success");
        }
      }
      break;
    }
    case BLE_FINDING_SERVICE:
    {
      if (hasTimedOut(_connectionTimeoutMs))
      {
        Util::logDebug("Finding service timeout after %d ms", _connectionTimeoutMs);
        break;
      }

      if (_client == nullptr)
      {
        Util::logTrace("Client is null!");
        break;
      }

      if (!_client->isConnected())
      {
        Util::logTrace("Waiting for connection...");
        break;
      }

      // ReSharper disable once CppDFAConstantConditions
      if (!_currentDevice.connected)
      {
        Util::logDebug("Waiting for connect callback...");
        break;
      }

      if (!findCompatibleService())
      {
        updateStatus(BLE_FAILED, "Could not find compatible service");
        break;
      }

      if (!findCharacteristics())
      {
        updateStatus(BLE_FAILED, "Could not find required characteristics");
        break;
      }

      updateStatus(BLE_CONNECTED, "Connected successfully!");
      break;
    }
    case BLE_RECONNECTING:
    {
      Util::logDebug("Reconnecting to device @ %s", _currentDevice.address.c_str());
      connectToDevice(_currentDevice.address);
    }
    default:
    {
      if (isConnectedState())
      {
        if (_client == nullptr || !_client->isConnected())
        {
          updateStatus(BLE_FAILED, "Connection lost unexpectedly");
        }
        else
        {
          // we have a stable connection, update signal strength
          if (Util::hasTimeExpired(1000, _lastRssiCheck))
          {
            _currentDevice.rssi = _client->getRssi();
            _lastRssiCheck = millis();

            Util::logTrace("Updating rssi for connected device:%d", _currentDevice.rssi);
          }
        }
      }
      break;
    }
  }
}

bool NogasmBLEManager::findCompatibleService()
{
  if (_client == nullptr || !_client->isConnected())
  {
    Util::logDebug("  > Cannot find services: client not connected");
    return false;
  }

  if (!_deviceProtocol)
  {
    Util::logDebug("  > Cannot find services: protocol not initialized");
    return false;
  }

  delay(BLE_STABILISE_DELAY_MS * 50);
  Util::logDebug("  > Discovering attributes...");

  bool discovered = false;
  for (int attempt = 0; attempt < 3; attempt++)
  {
    Util::logDebug("  > Discovery attempt %d", attempt + 1);
    discovered = _client->discoverAttributes();
    if (discovered)
    {
      Util::logDebug("  > Service discovery successful");
      break;
    }

    Util::logDebug("  > Service discovery failed, retrying...");
    delay(BLE_STABILISE_DELAY_MS * 50);
  }

  if (!discovered)
  {
    Util::logDebug("  > Service discovery failed after retries");
    return false;
  }

  auto& services = _client->getServices(false);
  Util::logDebug("  > Found services %d", services.size());

  for (const auto& it : services)
  {
    std::string serviceUuid = it->getUUID().toString();
    Util::logTrace("  > Examining service: %s", serviceUuid.c_str());
    if (_deviceProtocol->isCompatibleServiceUUID(serviceUuid))
    {
      Util::logDebug("    > Selected as compatible service: %s", serviceUuid.c_str());
      _service = it;
      return true;
    }
  }

  Util::logDebug("  > No compatible service found");
  return false;
}

bool NogasmBLEManager::findCharacteristics()
{
  if (_service == nullptr)
  {
    Util::logDebug("  > Cannot find characteristics: No service selected");
    return false;
  }

  if (!_deviceProtocol)
  {
    Util::logDebug("  > Cannot find characteristics: protocol not initialized");
    return false;
  }

  Util::logDebug("  > Discovering characteristics");
  for (const auto& it : _service->getCharacteristics(false))
  {
    std::string charUuid = it->getUUID().toString();
    Util::logDebug("  > Examining characteristic: %s", charUuid.c_str());

    const bool canWrite = it->canWrite();
    if (_writeCharacteristic == nullptr && canWrite)
    {
      Util::logDebug("    > Selected as control characteristic");
      _writeCharacteristic = it;
      _deviceProtocol->setTxCharacteristic(_writeCharacteristic);
    }

    const bool canNotify = it->canNotify();
    if (_notifyCharacteristic == nullptr && canNotify)
    {
      Util::logDebug("    > Selected as notify characteristic");
      _notifyCharacteristic = it;
      _deviceProtocol->setRxCharacteristic(_notifyCharacteristic, _client);
    }

    if (_writeCharacteristic != nullptr && _notifyCharacteristic != nullptr)
    {
      return true;
    }
  }

  // the write characteristic is absolutely required
  return _writeCharacteristic != nullptr;
}

void NogasmBLEManager::updateStatus(BLEConnectionState newState, const std::string& reason)
{
  if (newState == BLE_SCANNING)
  {
    _devices.clear();
    _scanActive = true;
  }
  else if (newState == BLE_IDLE)
  {
    _scanActive = false;
  }
  else if (newState == BLE_FAILED)
  {
    // connect() will clean up, we just indicate the current connection failure
    updateDeviceState(false);
    if (shouldTryReconnect())
    {
      // don't transition to failed yet, try reconnecting first
      newState = BLE_RECONNECTING;
    }
  }
  else if (newState == BLE_CONNECTED)
  {
    // reset reconnect attempts on successful connection
    _reconnectAttempts = 0;

    // send device queries
    _deviceProtocol->queryDeviceType();
    _deviceProtocol->queryBatteryLevel();

    // vibration test pulse
    const uint8_t defaultLevel = _config.getDefaultVibrationLevel();
    if (defaultLevel > 0)
    {
      setVibrationLevel(defaultLevel);
    }
    else
    {
      setVibrationLevel(1);
      delay(200);
      setVibrationLevel(0);
    }
  }

  Util::logInfo("%s -> %s :: %s", getStateString(_state).c_str(), getStateString(newState).c_str(), reason.c_str());

  _state = newState;
  _lastStateChangeTime = millis();
  notifyStatusChange();
}

void NogasmBLEManager::notifyStatusChange() const
{
  if (_statusCallback)
  {
    _statusCallback(_state);
  }
}

void NogasmBLEManager::ScanCallbacks::onScanEnd(const NimBLEScanResults& scanResults, int reason)
{
  Util::logDebug("  > Scan stopped, found %zu devices total", _manager->_devices.size());
  _manager->updateStatus(BLE_IDLE, "Scan completed");

  if (_manager->_autoConnectEnabled)
  {
    Util::logDebug("  > Auto-connect enabled");

    // try to find the last connected device if auto-reconnect is enabled
    if (_manager->connectToLastDevice())
    {
      return;
    }

    // connect to the first device if only one is found
    if (_manager->_devices.size() == 1)
    {
      Util::logDebug("  > Found only one device, auto-connecting...");
      const auto& autoConnectDevice = _manager->_devices.front();
      _manager->connectToDevice(autoConnectDevice.address);
    }
  }
}

void NogasmBLEManager::ScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice)
{
  if (advertisedDevice == nullptr || !advertisedDevice->haveName())
  {
    return;
  }

  const uint8_t deviceAddressType = advertisedDevice->getAddress().getType();
  const int8_t deviceRSSI = advertisedDevice->getRSSI();
  const std::string deviceName = advertisedDevice->getName();
  const std::string deviceAddress = advertisedDevice->getAddress().toString();
  const std::string addressTypeStr = (deviceAddressType == BLE_ADDR_PUBLIC)  //
                                       ? "PUBLIC"
                                       : (deviceAddressType == BLE_ADDR_RANDOM)  //
                                           ? "RANDOM"
                                           : "UNKNOWN";

  // determine protocol (lovense only for now)
  CompatibleDeviceProtocol deviceProtocol = UNKNOWN;
  if (LovenseProtocol::isCompatibleDevice(deviceName))
  {
    deviceProtocol = LOVENSE;
  }

  Util::logTrace("  > Found device: %s, address: %s, type: %s, rssi: %d",  //
    deviceName.c_str(), deviceAddress.c_str(), addressTypeStr.c_str(), deviceRSSI);

  if (deviceProtocol != UNKNOWN)
  {
    // ensure we have not seen this device already within the advertisement window
    for (const auto& device : _manager->_devices)
    {
      if (device.address == deviceAddress)
      {
        return;
      }
    }

    Util::logDebug("    > Compatible device identified: %s, address: %s, type: %s",  //
      deviceName.c_str(), deviceAddress.c_str(), addressTypeStr.c_str());

    CompatibleDevice newDevice;
    newDevice.address = deviceAddress;
    newDevice.addressType = addressTypeStr;
    newDevice.name = deviceName;
    newDevice.rssi = deviceRSSI;
    newDevice.protocol = deviceProtocol;

    _manager->_devices.push_back(newDevice);
  }
}

void NogasmBLEManager::ClientCallbacks::onConnect(NimBLEClient* client)
{
  Util::logDebug("  > Connected to: %s, rssi: %d", client->getPeerAddress().toString().c_str(), client->getRssi());
  _manager->updateDeviceState(true);
}

void NogasmBLEManager::ClientCallbacks::onConnectFail(NimBLEClient* client, const int reason)
{
  String reasonStr;
  switch (reason)
  {
    case BLE_HS_ETIMEOUT:
      reasonStr = "TIMEOUT";
      break;
    case BLE_HS_EBADDATA:
      reasonStr = "BAD_DATA";
      break;
    case BLE_HS_EDONE:
      reasonStr = "ALREADY_CONNECTED";
      break;
    case BLE_HS_ENOTCONN:
      reasonStr = "NOT_CONNECTED";
      break;
    default:
      reasonStr = String(reason);
      break;
  }

  Util::logInfo("Connection failed, reason %d (%s)", reason, reasonStr.c_str());
  _manager->updateStatus(BLE_FAILED, reasonStr.c_str());
}

void NogasmBLEManager::ClientCallbacks::onDisconnect(const NimBLEClient* client) const
{
  Util::logDebug("Client disconnected from %s", client->getPeerAddress().toString().c_str());
  _manager->updateStatus(BLE_FAILED, "Unexpected disconnect");
}

bool NogasmBLEManager::ClientCallbacks::onConnParamsUpdateRequest(NimBLEClient* client, const ble_gap_upd_params* params)
{
  Util::logDebug("Connection parameter update request received");
  Util::logDebug("  > Min interval: %d", params->itvl_min);
  Util::logDebug("  > Max interval: %d", params->itvl_max);
  Util::logDebug("  > Latency: %d", params->latency);
  Util::logDebug("  > Timeout: %d", params->supervision_timeout);

  // Accept the parameter update
  return true;
}
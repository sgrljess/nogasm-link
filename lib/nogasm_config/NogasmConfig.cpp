#include "NogasmConfig.h"
#include <ArduinoJson.h>
#include <Util.h>

NogasmConfig::NogasmConfig(fs::FS &filesystem, const char *configFile) : _filesystem(filesystem), _configFile(configFile)
{
}

bool NogasmConfig::load()
{
  File configFile = _filesystem.open(_configFile, "r");
  if (!configFile)
  {
    Util::logDebug("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
    Util::logDebug("Config file size is too large");
    configFile.close();
    return false;
  }

  const std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error)
  {
    Util::logInfo("Failed to parse config file");
    return false;
  }

  if (doc["device"].is<JsonObject>())
  {
    _deviceAddress = doc["device"]["address"] | "";
    _deviceName = doc["device"]["name"] | "";
  }

  if (doc["connection"].is<JsonObject>())
  {
    _scanDuration = doc["connection"]["scanDuration"] | 10000;
    _connectionTimeout = doc["connection"]["connectionTimeout"] | 15000;
  }

  if (doc["control"].is<JsonObject>())
  {
    _defaultVibrationLevel = doc["control"]["defaultVibrationLevel"] | 0;
  }

  if (doc["ui"].is<JsonObject>())
  {
    _autoConnect = doc["ui"]["autoConnect"] | true;
    _autoReconnect = doc["ui"]["autoReconnect"] | true;
  }

  if (doc["arousal"].is<JsonObject>())
  {
    _arousalLimit = doc["arousal"]["limit"] | 500;
    if (doc["arousal"]["config"].is<JsonObject>())
    {
      _arousalConfig.arousalDecayRate = doc["arousal"]["config"]["arousalDecayRate"] | 0.99f;
      _arousalConfig.sensitivityAfterEdgeDecayRate = doc["arousal"]["config"]["sensitivityAfterEdgeDecayRate"] | 0.99f;
      _arousalConfig.minSensitivityWhileDecaying = doc["arousal"]["config"]["minSensitivityWhileDecaying"] | 40;
      _arousalConfig.sensitivityThreshold = doc["arousal"]["config"]["sensitivityThreshold"] | 70;
      _arousalConfig.maxArousalLimit = doc["arousal"]["config"]["maxArousalLimit"] | 4000;
      _arousalConfig.maxSpeed = doc["arousal"]["config"]["maxSpeed"] | 255;
      _arousalConfig.frequency = doc["arousal"]["config"]["frequency"] | 60;
      _arousalConfig.rampTimeSeconds = doc["arousal"]["config"]["rampTimeSeconds"] | 50.0f;
      _arousalConfig.coolTimeSeconds = doc["arousal"]["config"]["coolTimeSeconds"] | 15.0f;
      _arousalConfig.targetEdgeCount = doc["arousal"]["config"]["targetEdgeCount"] | 20;

      // Clench settings
      _arousalConfig.clenchPressureSensitivity = doc["arousal"]["config"]["clenchPressureSensitivity"] | 20;
      _arousalConfig.clenchTimeMinThresholdMs = doc["arousal"]["config"]["clenchTimeMinThresholdMs"] | 250;
      _arousalConfig.clenchTimeMaxThresholdMs = doc["arousal"]["config"]["clenchTimeMaxThresholdMs"] | 3500;
    }
  }

  // Load misc settings
  _lastConnectedDevice = doc["lastConnectedDevice"] | "";
  if (!_deviceAddress.isEmpty())
  {
    Util::logDebug("Loaded saved device: %s, Address: %s", _deviceName.c_str(), _deviceAddress.c_str());
    return true;
  }

  return false;
}

bool NogasmConfig::save() const
{
  JsonDocument doc;

  doc["device"]["address"] = _deviceAddress;
  doc["device"]["name"] = _deviceName;

  doc["connection"]["scanDuration"] = _scanDuration;
  doc["connection"]["connectionTimeout"] = _connectionTimeout;

  doc["control"]["defaultVibrationLevel"] = _defaultVibrationLevel;

  doc["ui"]["autoConnect"] = _autoConnect;
  doc["ui"]["autoReconnect"] = _autoReconnect;

  doc["arousal"]["limit"] = _arousalLimit;
  doc["arousal"]["config"]["arousalDecayRate"] = _arousalConfig.arousalDecayRate;
  doc["arousal"]["config"]["sensitivityThreshold"] = _arousalConfig.sensitivityThreshold;
  doc["arousal"]["config"]["sensitivityAfterEdgeDecayRate"] = _arousalConfig.sensitivityAfterEdgeDecayRate;
  doc["arousal"]["config"]["minSensitivityWhileDecaying"] = _arousalConfig.minSensitivityWhileDecaying;
  doc["arousal"]["config"]["maxArousalLimit"] = _arousalConfig.maxArousalLimit;
  doc["arousal"]["config"]["maxSpeed"] = _arousalConfig.maxSpeed;
  doc["arousal"]["config"]["frequency"] = _arousalConfig.frequency;
  doc["arousal"]["config"]["rampTimeSeconds"] = _arousalConfig.rampTimeSeconds;
  doc["arousal"]["config"]["coolTimeSeconds"] = _arousalConfig.coolTimeSeconds;
  doc["arousal"]["config"]["targetEdgeCount"] = _arousalConfig.targetEdgeCount;
  doc["arousal"]["config"]["clenchPressureSensitivity"] = _arousalConfig.clenchPressureSensitivity;
  doc["arousal"]["config"]["clenchTimeMinThresholdMs"] = _arousalConfig.clenchTimeMinThresholdMs;
  doc["arousal"]["config"]["clenchTimeMaxThresholdMs"] = _arousalConfig.clenchTimeMaxThresholdMs;

  // Save misc settings
  doc["lastConnectedDevice"] = _lastConnectedDevice;

  File configFile = _filesystem.open(_configFile, "w");
  if (!configFile)
  {
    Util::logInfo("Failed to open config file for writing");
    return false;
  }

  serializeJson(doc, configFile);
  configFile.close();
  Util::logTrace("Config saved successfully");
  return true;
}

String NogasmConfig::getDeviceAddress() const
{
  return _deviceAddress;
}

void NogasmConfig::setDeviceAddress(const String &address)
{
  _deviceAddress = address;
}

String NogasmConfig::getDeviceName() const
{
  return _deviceName;
}

void NogasmConfig::setDeviceName(const String &name)
{
  _deviceName = name;
}

void NogasmConfig::saveDeviceInfo(const String &address, const String &name)
{
  _deviceAddress = address;
  _deviceName = name;
  _lastConnectedDevice = address;

  // ReSharper disable once CppExpressionWithoutSideEffects
  save();
}

bool NogasmConfig::hasDeviceInfo() const
{
  return !_deviceAddress.isEmpty();
}

// Connection settings getters/setters
uint32_t NogasmConfig::getScanDuration() const
{
  return _scanDuration;
}

void NogasmConfig::setScanDuration(uint32_t duration)
{
  _scanDuration = duration;
}

uint32_t NogasmConfig::getConnectionTimeout() const
{
  return _connectionTimeout;
}

void NogasmConfig::setConnectionTimeout(uint32_t timeout)
{
  _connectionTimeout = timeout;
}

uint8_t NogasmConfig::getDefaultVibrationLevel() const
{
  return _defaultVibrationLevel;
}

void NogasmConfig::setDefaultVibrationLevel(uint8_t level)
{
  if (level > 20)
    level = 20;  // Cap at maximum Lovense level
  _defaultVibrationLevel = level;
}

bool NogasmConfig::getAutoConnect() const
{
  return _autoConnect;
}

void NogasmConfig::setAutoConnect(bool enabled)
{
  _autoConnect = enabled;
}

bool NogasmConfig::getAutoReconnect() const
{
  return _autoReconnect;
}

void NogasmConfig::setAutoReconnect(bool enabled)
{
  _autoReconnect = enabled;
}

// Misc getters/setters
String NogasmConfig::getLastConnectedDevice() const
{
  return _lastConnectedDevice;
}

void NogasmConfig::setLastConnectedDevice(const String &address)
{
  _lastConnectedDevice = address;
}

void NogasmConfig::clear()
{
  _deviceAddress = "";
  _deviceName = "";
  _scanDuration = 10000;
  _connectionTimeout = 15000;
  _defaultVibrationLevel = 0;
  _autoConnect = true;
  _autoReconnect = true;
  _lastConnectedDevice = "";

  // ReSharper disable once CppExpressionWithoutSideEffects
  save();
}

ArousalConfig NogasmConfig::getArousalConfig() const
{
  return _arousalConfig;
}

void NogasmConfig::setArousalConfig(const ArousalConfig &config)
{
  _arousalConfig = config;
}

int NogasmConfig::getArousalLimit() const
{
  return _arousalLimit;
}

void NogasmConfig::setArousalLimit(int limit)
{
  _arousalLimit = limit;
}

void NogasmConfig::clearCategory(ConfigCategory category)
{
  switch (category)
  {
    case DEVICE:
      _deviceAddress = "";
      _deviceName = "";
      break;
    case CONNECTION:
      _scanDuration = 10000;
      _connectionTimeout = 15000;
      break;
    case UI:
      _autoConnect = true;
      _autoReconnect = true;
      _defaultVibrationLevel = 0;
      break;
    case AROUSAL:
      // Reset arousal settings to defaults
      _arousalLimit = 500;
      _arousalConfig.arousalDecayRate = 0.99f;
      _arousalConfig.minSensitivityWhileDecaying = 40;
      _arousalConfig.sensitivityAfterEdgeDecayRate = 0.99f;
      _arousalConfig.sensitivityThreshold = 70;
      _arousalConfig.maxArousalLimit = 4000;
      _arousalConfig.maxSpeed = 255;
      _arousalConfig.frequency = 60;
      _arousalConfig.targetEdgeCount = 20;
      _arousalConfig.rampTimeSeconds = 50.0f;
      _arousalConfig.coolTimeSeconds = 15.0f;
      break;
  }

  // ReSharper disable once CppExpressionWithoutSideEffects
  save();
}
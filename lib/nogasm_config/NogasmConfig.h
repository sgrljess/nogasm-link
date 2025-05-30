#ifndef NOGASM_CONFIG_H
#define NOGASM_CONFIG_H

#include <Arduino.h>
#include <FS.h>
#include "ArousalConfig.h"

// Configuration categories
enum ConfigCategory
{
  DEVICE = 0,      // Device-specific config
  CONNECTION = 1,  // BLE connection settings
  UI = 2,          // User interface preferences
  AROUSAL = 3      // Arousal control settings
};

class NogasmConfig
{
 public:
  NogasmConfig(fs::FS &filesystem, const char *configFile);

  // Load configuration from file
  bool load();

  // Save configuration to file
  bool save() const;

  // Device info
  String getDeviceAddress() const;
  void setDeviceAddress(const String &address);
  String getDeviceName() const;
  void setDeviceName(const String &name);
  void saveDeviceInfo(const String &address, const String &name);
  bool hasDeviceInfo() const;

  // Connection settings
  uint32_t getScanDuration() const;
  void setScanDuration(uint32_t duration);
  uint32_t getConnectionTimeout() const;
  void setConnectionTimeout(uint32_t timeout);

  // Device control settings
  uint8_t getDefaultVibrationLevel() const;
  void setDefaultVibrationLevel(uint8_t level);

  // UI settings
  bool getAutoConnect() const;
  void setAutoConnect(bool enabled);
  bool getAutoReconnect() const;
  void setAutoReconnect(bool enabled);

  // Arousal settings
  ArousalConfig getArousalConfig() const;
  void setArousalConfig(const ArousalConfig &config);
  int getArousalLimit() const;
  void setArousalLimit(int limit);

  // Misc
  String getLastConnectedDevice() const;
  void setLastConnectedDevice(const String &address);

  // Clear all configuration
  void clear();

  // Clear a specific category
  void clearCategory(ConfigCategory category);

 private:
  fs::FS &_filesystem;
  const char *_configFile;

  // Device info
  String _deviceAddress;
  String _deviceName;

  // Connection settings
  uint32_t _scanDuration = 10000;       // 10 seconds
  uint32_t _connectionTimeout = 15000;  // 15 seconds

  // Device control settings
  uint8_t _defaultVibrationLevel = 0;

  // UI settings
  bool _autoConnect = true;    // Auto-connect to found devices
  bool _autoReconnect = true;  // Auto-reconnect on disconnect

  // Arousal settings
  ArousalConfig _arousalConfig;
  int _arousalLimit = 500;

  // Misc
  String _lastConnectedDevice;  // Address of last successfully connected device
};

#endif
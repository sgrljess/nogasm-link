#ifndef LOVENSE_PROTOCOL_H
#define LOVENSE_PROTOCOL_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>
#include <vector>
#include <functional>
#include "DeviceProtocol.h"

enum class LovenseModel
{
  UNKNOWN,
  NORA,   // A or C
  MAX,    // B
  AMBI,   // L
  LUSH,   // S
  HUSH,   // Z
  DOMI,   // W
  EDGE,   // P
  OSCI,   // O
  TENERA  // Q
};

// Specific Lovense device info
struct LovenseDeviceInfo
{
  LovenseModel model = LovenseModel::UNKNOWN;
  String modelLetter;
  String firmwareVersion;
  String macAddress;
  int batteryLevel = -1;
  uint8_t addressType = BLE_ADDR_PUBLIC;
};

class LovenseProtocol : public DeviceProtocol
{
 public:
  LovenseProtocol();

  // DeviceProtocol implementation
  void setTxCharacteristic(NimBLERemoteCharacteristic* txChar) override;
  void setRxCharacteristic(NimBLERemoteCharacteristic* rxChar, NimBLEClient* client) override;
  void clearCharacteristics() override;
  bool isReady() const override;

  // Device identification
  bool isCompatibleServiceUUID(const std::string& uuid) override;
  static bool isCompatibleDevice(const std::string& deviceName);

  // Static helpers for identification
  static bool isLovenseServiceUUID(const std::string& uuid);
  static bool isLovenseDevice(const std::string& deviceName);

  // Control commands
  bool setVibration(uint8_t level) override;
  bool setRotation(uint8_t level) override;
  bool changeRotationDirection() override;
  bool setAirLevel(uint8_t level) override;
  bool adjustAirLevelRelative(bool inflate, uint8_t amount) override;
  bool powerOff() override;

  // Query functions
  bool queryDeviceType() override;
  bool queryBatteryLevel() override;

  // Info functions
  String getModelDisplayName() const override;

  // Callbacks
  void setDeviceInfoCallback(std::function<void(const DeviceInfo&)> callback) override;
  void setBatteryLevelCallback(std::function<void(int)> callback) override;

  // Helpers
  static LovenseModel modelLetterToEnum(const String& letter);
  static String enumToModelDisplay(LovenseModel model);

  // Process received data
  void handleResponse(const std::string& response);

 private:
  // Send a command to the device
  bool sendCommand(const std::string& command) const;

  // Parse responses
  void parseDeviceTypeResponse(const std::string& response);
  void parseBatteryResponse(const std::string& response);

  // Private members
  NimBLERemoteCharacteristic* _txCharacteristic = nullptr;
  NimBLERemoteCharacteristic* _rxCharacteristic = nullptr;
  LovenseDeviceInfo _lovenseDeviceInfo;

  // Callback functions
  std::function<void(const DeviceInfo&)> _deviceInfoCallback;
  std::function<void(int)> _batteryLevelCallback;

  // Static vectors of known Lovense service UUIDs patterns
  static const std::vector<std::string> LOVENSE_SERVICE_UUID_PATTERNS;
};

#endif  // LOVENSE_PROTOCOL_H
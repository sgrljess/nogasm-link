#ifndef DEVICE_PROTOCOL_H
#define DEVICE_PROTOCOL_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>
#include <functional>

// Forward declaration for device info callback
struct DeviceInfo
{
  String modelType;
  String firmwareVersion;
  String macAddress;
  int batteryLevel = -1;
};

// Device protocol interface
class DeviceProtocol
{
 public:
  virtual ~DeviceProtocol() = default;

  // Connection management
  virtual void setTxCharacteristic(NimBLERemoteCharacteristic* txChar) = 0;
  virtual void setRxCharacteristic(NimBLERemoteCharacteristic* rxChar, NimBLEClient* client) = 0;
  virtual void clearCharacteristics() = 0;
  virtual bool isReady() const = 0;

  // Device identification
  virtual bool isCompatibleServiceUUID(const std::string& uuid) = 0;

  // Control commands
  virtual bool setVibration(uint8_t level) = 0;
  virtual bool setRotation(uint8_t level) = 0;
  virtual bool changeRotationDirection() = 0;
  virtual bool setAirLevel(uint8_t level) = 0;
  virtual bool adjustAirLevelRelative(bool inflate, uint8_t amount) = 0;
  virtual bool powerOff() = 0;

  // Query functions
  virtual bool queryDeviceType() = 0;
  virtual bool queryBatteryLevel() = 0;

  // Info functions
  virtual String getModelDisplayName() const = 0;

  // Callbacks
  virtual void setDeviceInfoCallback(std::function<void(const DeviceInfo&)> callback) = 0;
  virtual void setBatteryLevelCallback(std::function<void(int)> callback) = 0;
};

#endif  // DEVICE_PROTOCOL_H
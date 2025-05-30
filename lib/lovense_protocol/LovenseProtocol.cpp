// ReSharper disable CppExpressionWithoutSideEffects
#include "LovenseProtocol.h"

#include "Util.h"

// Initialize static vector of known Lovense service UUID patterns - last 12 chars
const std::vector<std::string> LovenseProtocol::LOVENSE_SERVICE_UUID_PATTERNS = {
  "05f9b34fb",     // Early Lovense devices (0000fff0-0000-1000-8000-00805f9b34fb)
  "e50e24dcca9e",  // Second generation (6e400001-b5a3-f393-e0a9-e50e24dcca9e)
  "a6920e4c5653",  // Third generation - pattern ending
};

LovenseProtocol::LovenseProtocol() = default;

// DeviceProtocol interface implementation
bool LovenseProtocol::isCompatibleServiceUUID(const std::string& uuid)
{
  return isLovenseServiceUUID(uuid);
}

bool LovenseProtocol::isCompatibleDevice(const std::string& deviceName)
{
  return isLovenseDevice(deviceName);
}

// Static methods
bool LovenseProtocol::isLovenseServiceUUID(const std::string& uuid)
{
  for (const auto& pattern : LOVENSE_SERVICE_UUID_PATTERNS)
  {
    if (uuid.length() >= pattern.length())
    {
      std::string uuidEnd = uuid.substr(uuid.length() - pattern.length());
      if (uuidEnd == pattern)
      {
        return true;
      }
    }
  }

  return false;
}

bool LovenseProtocol::isLovenseDevice(const std::string& deviceName)
{
  if (deviceName.find("LVS") != std::string::npos || deviceName.find("Lovense") != std::string::npos || deviceName.find("LLS") != std::string::npos)
  {
    return true;
  }

  return false;
}

void LovenseProtocol::setTxCharacteristic(NimBLERemoteCharacteristic* txChar)
{
  _txCharacteristic = txChar;
}

void LovenseProtocol::setRxCharacteristic(NimBLERemoteCharacteristic* rxChar, NimBLEClient* client)
{
  _rxCharacteristic = rxChar;

  // Set up notifications if the RX characteristic supports it
  if (_rxCharacteristic && _rxCharacteristic->canNotify())
  {
    _rxCharacteristic->subscribe(true,
      [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, const size_t length, bool isNotify)
      {
        const std::string response(reinterpret_cast<char*>(pData), length);
        this->handleResponse(response);
      });
  }
}

void LovenseProtocol::clearCharacteristics()
{
  // Unsubscribe from notifications if active
  if (_rxCharacteristic && _rxCharacteristic->canNotify())
  {
    _rxCharacteristic->subscribe(false);
  }

  _txCharacteristic = nullptr;
  _rxCharacteristic = nullptr;
}

bool LovenseProtocol::isReady() const
{
  return _txCharacteristic != nullptr && _txCharacteristic->canWrite();
}

bool LovenseProtocol::sendCommand(const std::string& command) const
{
  if (!isReady())
  {
    Util::logInfo("Lovense protocol not ready to send commands");
    return false;
  }

  Util::logTrace("LovenseProtocol::command: %s", command.c_str());
  return _txCharacteristic->writeValue(command.c_str());
}

String LovenseProtocol::getModelDisplayName() const
{
  return enumToModelDisplay(_lovenseDeviceInfo.model);
}

// Callback setters
void LovenseProtocol::setDeviceInfoCallback(const std::function<void(const DeviceInfo&)> callback)
{
  _deviceInfoCallback = callback;
}

void LovenseProtocol::setBatteryLevelCallback(const std::function<void(int)> callback)
{
  _batteryLevelCallback = callback;
}

// Command implementations
bool LovenseProtocol::setVibration(uint8_t level)
{
  // Ensure level is between 0-20 (Lovense range)
  if (level > 20)
  {
    level = 20;
  }

  // // Some newer models use different commands based on model
  // if (_currentDevice.deviceType == "Edge")
  // {
  //   // Edge has two motors that can be controlled separately
  //   command = "Vibrate1:" + std::to_string(level) + ";";
  // }

  const std::string command = "Vibrate:" + std::to_string(level) + ";";
  return sendCommand(command);
}

bool LovenseProtocol::setRotation(uint8_t level)
{
  // Only applicable for toys with rotation like Nora
  if (level > 20)
  {
    level = 20;
  }

  const std::string command = "Rotate:" + std::to_string(level) + ";";
  return sendCommand(command);
}

bool LovenseProtocol::changeRotationDirection()
{
  // Only applicable for toys with rotation like Nora
  return sendCommand("RotateChange;");
}

bool LovenseProtocol::setAirLevel(uint8_t level)
{
  // Only applicable for toys with air control like Max
  if (level > 4)
    level = 4;

  const std::string command = "Air:Level:" + std::to_string(level) + ";";
  return sendCommand(command);
}

bool LovenseProtocol::adjustAirLevelRelative(const bool inflate, uint8_t amount)
{
  // Only applicable for toys with air control like Max
  // Ensure amount is in reasonable range (1-4)
  if (amount < 1)
  {
    amount = 1;
  }
  else if (amount > 4)
  {
    amount = 4;
  }

  const std::string direction = inflate ? "In" : "Out";
  const std::string command = "Air:" + direction + ":" + std::to_string(amount) + ";";
  return sendCommand(command);
}

bool LovenseProtocol::powerOff()
{
  return sendCommand("PowerOff;");
}

// Query functions
bool LovenseProtocol::queryDeviceType()
{
  return sendCommand("DeviceType;");
}

bool LovenseProtocol::queryBatteryLevel()
{
  return sendCommand("Battery;");
}

void LovenseProtocol::handleResponse(const std::string& response)
{
  // Check if the response contains a semicolon as per protocol
  if (response.find(';') == std::string::npos)
  {
    Util::logInfo("Invalid response format");
    return;
  }

  if (response.find(':') != std::string::npos)
  {
    const int colonCount = std::count(response.begin(), response.end(), ':');
    if (colonCount == 2 && response[1] == ':')
    {
      // DeviceType response format: "C:11:0082059AD3BD;"
      parseDeviceTypeResponse(response);
    }
    else
    {
      Util::logInfo("Don't know how to handle response: %s", response.c_str());
    }
  }
  else
  {
    // Simple numeric response (e.g., battery level or status)
    // todo: to handle status we would need to know which command we issued first, not going to implement that now...
    //  since the responses look exactly the same
    if (isdigit(response[0]) && response[response.length() - 1] == ';')
    {
      parseBatteryResponse(response);
    }
    else
    {
      Util::logTrace("LovenseProtocol::reply: %s", response.c_str());
    }
  }
}

// Parser implementations
void LovenseProtocol::parseDeviceTypeResponse(const std::string& response)
{
  // Format: "C:11:0082059AD3BD;"
  // Where C is model letter, 11 is firmware version, and the rest is MAC address
  const String resp(response.c_str());
  const int firstColon = resp.indexOf(':');
  const int secondColon = resp.indexOf(':', firstColon + 1);
  const int semicolon = resp.indexOf(';');

  if (firstColon > 0 && secondColon > firstColon && semicolon > secondColon)
  {
    _lovenseDeviceInfo.modelLetter = resp.substring(0, firstColon);
    _lovenseDeviceInfo.firmwareVersion = resp.substring(firstColon + 1, secondColon);
    _lovenseDeviceInfo.macAddress = resp.substring(secondColon + 1, semicolon);
    _lovenseDeviceInfo.model = modelLetterToEnum(_lovenseDeviceInfo.modelLetter);

    // Convert to common DeviceInfo and call callback
    if (_deviceInfoCallback)
    {
      DeviceInfo info;
      info.modelType = enumToModelDisplay(_lovenseDeviceInfo.model);
      info.firmwareVersion = _lovenseDeviceInfo.firmwareVersion;
      info.macAddress = _lovenseDeviceInfo.macAddress;
      info.batteryLevel = _lovenseDeviceInfo.batteryLevel;

      _deviceInfoCallback(info);
    }
  }
}

void LovenseProtocol::parseBatteryResponse(const std::string& response)
{
  // Format: "85;" or "Battery:85;"
  const String resp(response.c_str());
  int batteryLevel = -1;

  if (resp.startsWith("Battery:"))
  {
    const int colon = resp.indexOf(':');
    const int semicolon = resp.indexOf(';');
    if (colon > 0 && semicolon > colon)
    {
      batteryLevel = resp.substring(colon + 1, semicolon).toInt();
    }
  }
  else
  {
    const int semicolon = resp.indexOf(';');
    if (semicolon > 0)
    {
      batteryLevel = resp.substring(0, semicolon).toInt();
    }
  }

  if (batteryLevel >= 0 && batteryLevel <= 100)
  {
    _lovenseDeviceInfo.batteryLevel = batteryLevel;

    if (_batteryLevelCallback)
    {
      _batteryLevelCallback(batteryLevel);
    }
  }
}

// Helper functions
LovenseModel LovenseProtocol::modelLetterToEnum(const String& letter)
{
  if (letter == "A" || letter == "C")
  {
    return LovenseModel::NORA;
  }
  if (letter == "B")
  {
    return LovenseModel::MAX;
  }
  if (letter == "L")
  {
    return LovenseModel::AMBI;
  }
  if (letter == "S")
  {
    return LovenseModel::LUSH;
  }
  if (letter == "Z")
  {
    return LovenseModel::HUSH;
  }
  if (letter == "W")
  {
    return LovenseModel::DOMI;
  }
  if (letter == "P")
  {
    return LovenseModel::EDGE;
  }
  if (letter == "O")
  {
    return LovenseModel::OSCI;
  }
  if (letter == "Q")
  {
    return LovenseModel::TENERA;
  }

  return LovenseModel::UNKNOWN;
}

String LovenseProtocol::enumToModelDisplay(const LovenseModel model)
{
  switch (model)
  {
    case LovenseModel::NORA:
      return "NORA";
    case LovenseModel::MAX:
      return "MAX";
    case LovenseModel::AMBI:
      return "AMBI";
    case LovenseModel::LUSH:
      return "LUSH";
    case LovenseModel::HUSH:
      return "HUSH";
    case LovenseModel::DOMI:
      return "DOMI";
    case LovenseModel::EDGE:
      return "EDGE";
    case LovenseModel::OSCI:
      return "OSCI";
    case LovenseModel::TENERA:
      return "TENERA";
    default:
      return "UNKNOWN";
  }
}
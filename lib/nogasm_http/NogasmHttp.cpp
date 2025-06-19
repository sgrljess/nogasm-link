// ReSharper disable CppMemberFunctionMayBeStatic
#include "NogasmHttp.h"
#include "NogasmUpdate.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncJson.h>

#include <utility>

#define WS_BLE_UPDATE_TIME 1000               // BLE status updates every 1 second
#define WS_AROUSAL_UPDATE_TIME_ACTIVE 60      // Arousal updates every 100ms when active
#define WS_AROUSAL_UPDATE_TIME_INACTIVE 1000  // Arousal updates every 1 second when inactive
#define WS_CLIENT_TIMEOUT_MS 10000            // 10 seconds without pong = dead client
#define WS_PING_INTERVAL_MS 15000             // Send ping every 15 seconds

NogasmHttp::NogasmHttp(fs::FS &filesystem, NogasmBLEManager &bleManager, WiFiManager &wifiManager, NogasmConfig &config, ArousalManager &arousalManager,
  EncoderManager &encoderManager, NogasmUpdate &nogasmUpdate)
    : _server(NOGASM_HTTP_PORT),
      _filesystem(filesystem),
      _bleManager(bleManager),
      _wifiManager(wifiManager),
      _config(config),
      _encoderManager(encoderManager),
      _arousalManager(arousalManager),
      _nogasmUpdate(nogasmUpdate)
{
  _lastBleUpdate = 0;
  _lastArousalUpdate = 0;
  _lastPingCheck = 0;
  _ws = nullptr;
  _lastBleState = BLE_IDLE;
  _lastArousalActive = false;

  _nogasmUpdate.onStart(
    [this](UpdateMode mode)
    {
      this->sendUpdateStatusUpdate();
    });
  _nogasmUpdate.onProgress(
    [this](const UpdateProgress &progress)
    {
      this->sendUpdateStatusUpdate();
    });
  _nogasmUpdate.onEnd(
    [this](bool success)
    {
      this->sendUpdateStatusUpdate();
    });
  _nogasmUpdate.onError(
    [this](const String &error)
    {
      this->sendUpdateStatusUpdate();
    });
}

void NogasmHttp::begin()
{
  setupWebSocket();
  setupAPIEndpoints();
  setupStaticFiles();

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  _server.begin();
  Util::logInfo("HTTP server started @ %d", NOGASM_HTTP_PORT);
}

void NogasmHttp::update()
{
  if (_wsClients.empty())
  {
    return;
  }

  const unsigned long currentTime = millis();
  if (Util::hasTimeExpired(WS_PING_INTERVAL_MS, _lastPingCheck))
  {
    cleanupDisconnectedClients();
    _lastPingCheck = currentTime;
  }

  const BLEConnectionState currentState = _bleManager.getState();
  if (_lastBleState != currentState || Util::hasTimeExpired(WS_BLE_UPDATE_TIME, _lastBleUpdate))
  {
    sendBleStatusUpdate();
    _lastBleState = currentState;
    _lastBleUpdate = currentTime;
  }

  const bool arousalActive = _arousalManager.isActive();
  const uint32_t arousalUpdateInterval = arousalActive ? WS_AROUSAL_UPDATE_TIME_ACTIVE : WS_AROUSAL_UPDATE_TIME_INACTIVE;
  if (_lastArousalActive != arousalActive || Util::hasTimeExpired(arousalUpdateInterval, _lastArousalUpdate))
  {
    sendArousalStatusUpdate();
    _lastArousalActive = arousalActive;
    _lastArousalUpdate = currentTime;
  }

  if (_nogasmUpdate.isUpdateInProgress())
  {
    sendUpdateStatusUpdate();
  }
}

void NogasmHttp::setupStaticFiles()
{
  _server.serveStatic("/", _filesystem, "/").setDefaultFile("index.html");
}

void NogasmHttp::setupWebSocket()
{
  _ws = new AsyncWebSocket("/ws");
  _ws->onEvent(
    [this](AsyncWebSocket *server, AsyncWebSocketClient *client, const AwsEventType type, void *arg, uint8_t *data, const size_t len)
    {
      this->onWebSocketEvent(server, client, type, arg, data, len);
    });

  _server.addHandler(_ws);
  Util::logInfo("WebSocket server initialized on path: /ws");
}

void NogasmHttp::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, const AwsEventType type, void *arg, uint8_t *data, const size_t len)
{
  switch (type)
  {
    case WS_EVT_CONNECT:
      Util::logDebug("WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());

      _wsClients.insert(client);
      break;

    case WS_EVT_DISCONNECT:
      Util::logDebug("WebSocket client #%u disconnected", client->id());

      _wsClients.erase(client);
      break;

    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;

    case WS_EVT_ERROR:
      Util::logDebug("WebSocket error #%u", client->id());
      break;
    default:
      break;
  }
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void NogasmHttp::handleWebSocketMessage(const AsyncWebSocketClient *client, void *arg, uint8_t *data, const size_t len)
{
  const auto info = static_cast<AwsFrameInfo *>(arg);
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;  // Null terminate
    Util::logDebug("WebSocket message from client #%u: %s", client->id(), reinterpret_cast<char *>(data));
  }
}

bool NogasmHttp::canSendToClient(const AsyncWebSocketClient *client)
{
  if (!client || client->status() != WS_CONNECTED)
  {
    return false;
  }

  if (client->queueIsFull())
  {
    Util::logDebug("Client #%u queue full (%u messages), dropping message", client->id(), client->queueLen());
    return false;
  }

  return client->canSend();
}

void NogasmHttp::sendMessageToClient(AsyncWebSocketClient *client, const String &message)
{
  if (canSendToClient(client))
  {
    client->text(message);
  }
}

void NogasmHttp::broadcastMessage(const String &message)
{
  const auto clientsCopy = _wsClients;
  for (auto client : clientsCopy)
  {
    if (_wsClients.count(client) > 0)
    {
      sendMessageToClient(client, message);
    }
  }
}

void NogasmHttp::cleanupDisconnectedClients()
{
  std::vector<AsyncWebSocketClient *> clientsToRemove;
  for (auto client : _wsClients)
  {
    if (client && client->status() != WS_CONNECTED)
    {
      Util::logDebug("Found disconnected client #%u, removing from tracking", client->id());
      clientsToRemove.push_back(client);
    }
  }

  for (auto client : clientsToRemove)
  {
    _wsClients.erase(client);
  }
}

String NogasmHttp::createBleStatusMessage()
{
  JsonDocument doc;
  doc["type"] = "ble_status";
  generateBleStatusJson(doc);

  String output;
  serializeJson(doc, output);
  return output;
}

String NogasmHttp::createArousalStatusMessage()
{
  JsonDocument doc;
  doc["type"] = "arousal_status";
  generateArousalStatusJson(doc);

  String output;
  serializeJson(doc, output);
  return output;
}

String NogasmHttp::createUpdateStatusMessage()
{
  JsonDocument doc;
  doc["type"] = "update_status";
  generateUpdateStatusJson(doc);

  String output;
  serializeJson(doc, output);
  return output;
}

void NogasmHttp::sendBleStatusUpdate()
{
  const String message = createBleStatusMessage();
  broadcastMessage(message);
}

void NogasmHttp::sendArousalStatusUpdate()
{
  const String message = createArousalStatusMessage();
  broadcastMessage(message);
}

void NogasmHttp::sendUpdateStatusUpdate()
{
  const String message = createUpdateStatusMessage();
  broadcastMessage(message);
}

template <typename T>
void NogasmHttp::generateBleStatusJson(T &doc)
{
  const BLEConnectionState bleState = _bleManager.getState();

  doc["scanning"] = _bleManager.isScanning();
  doc["connected"] = _bleManager.isConnectedState();
  doc["state"] = static_cast<int>(bleState);
  doc["stateString"] = _bleManager.getStateString(bleState);

  const CompatibleDevice *device = _bleManager.getCurrentDevice();
  if (device != nullptr)
  {
    doc["device"]["name"] = device->name;
    doc["device"]["address"] = device->address;
    doc["device"]["addressType"] = device->addressType;
    doc["device"]["battery"] = device->batteryLevel;
    doc["device"]["model"] = device->modelDisplayName;
    doc["device"]["firmwareVersion"] = device->firmwareVersion;
    doc["device"]["rssi"] = device->rssi;
  }

  // add dynamic values as part of websocket
  doc["wifi"]["rssi"] = WiFi.RSSI();
}

template <typename T>
void NogasmHttp::generateArousalStatusJson(T &doc)
{
  doc["active"] = _arousalManager.isActive();
  doc["arousalPercent"] = _arousalManager.getArousalPercent();
  doc["pressure"] = _arousalManager.getCurrentPressure();
  doc["limit"] = _arousalManager.getArousalLimit();
  doc["limitExceededCounter"] = _arousalManager.getLimitExceededCounter();
  doc["sensitivity"] = _arousalManager.getSensitivity();
  doc["currentSessionDuration"] = _arousalManager.getCurrentSessionDuration();

  doc["clenchThreshold"] = _arousalManager.getConfig().clenchPressureThreshold;
  doc["lastClenchDuration"] = _arousalManager.getLastClenchDuration();
  doc["state"] = _arousalManager.getCurrentStateString();
}

template <typename T>
void NogasmHttp::generateDevicesJson(T &doc)
{
  const JsonArray devices = doc.template to<JsonArray>();
  for (const auto &device : _bleManager.getDevices())
  {
    const auto deviceObj = devices.add<JsonObject>();
    deviceObj["name"] = device.name;
    deviceObj["address"] = device.address;
    deviceObj["addressType"] = device.addressType;
    deviceObj["connected"] = device.connected;
    deviceObj["rssi"] = device.rssi;
  }
}

template <typename T>
void NogasmHttp::generateArousalConfigJson(T &doc)
{
  const ArousalConfig &config = _arousalManager.getConfig();

  doc["arousalDecayRate"] = config.arousalDecayRate;
  doc["sensitivityAfterEdgeDecayRate"] = config.sensitivityAfterEdgeDecayRate;
  doc["minSensitivityWhileDecaying"] = config.minSensitivityWhileDecaying;
  doc["sensitivityThreshold"] = config.sensitivityThreshold;
  doc["maxPressureLimit"] = _arousalManager.getPressureLimit();
  doc["maxArousalLimit"] = config.maxArousalLimit;
  doc["maxVibrationLevel"] = ArousalManager::speedToLevel(config.maxSpeed);
  doc["frequency"] = config.frequency;
  doc["rampTimeSeconds"] = config.rampTimeSeconds;
  doc["coolTimeSeconds"] = config.coolTimeSeconds;
  doc["targetEdgeCount"] = config.targetEdgeCount;

  doc["clenchPressureThreshold"] = config.clenchPressureThreshold;
  doc["clenchPressureSensitivity"] = config.clenchPressureSensitivity;
  doc["clenchTimeMinThresholdMs"] = config.clenchTimeMinThresholdMs;
  doc["clenchTimeMaxThresholdMs"] = config.clenchTimeMaxThresholdMs;
}

template <typename T>
void NogasmHttp::generateUpdateStatusJson(T &doc)
{
  const UpdateProgress progress = _nogasmUpdate.getProgress();

  doc["active"] = _nogasmUpdate.isUpdateInProgress();
  doc["state"] = static_cast<int>(progress.state);
  doc["mode"] = static_cast<int>(progress.mode);
  doc["percentage"] = progress.percentage;
  doc["current"] = progress.current;
  doc["total"] = progress.total;
  doc["error"] = progress.error;
  doc["hasError"] = progress.hasError;
  doc["md5Hash"] = progress.md5Hash;

  if (WiFi.isConnected())
  {
    doc["hostname"] = WiFi.getHostname();
    doc["ip"] = WiFi.localIP().toString();
  }
}

void NogasmHttp::setupAPIEndpoints()
{
  _server.on("/api/status", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleGetStatus(request);
    });

  _server.on("/api/devices", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleGetDevices(request);
    });

  _server.on("/api/disconnect", HTTP_POST,
    [this](AsyncWebServerRequest *request)
    {
      this->handleDisconnect(request);
    });

  _server.on("/api/scan", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleScan(request);
    });

  _server.on(
    "/api/connect", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleConnect(request, data, len, index, total);
    });

  _server.on(
    "/api/vibrate", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleVibrate(request, data, len, index, total);
    });

  _server.on(
    "/api/rotate", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleRotate(request, data, len, index, total);
    });

  _server.on("/api/rotate-direction", HTTP_POST,
    [this](AsyncWebServerRequest *request)
    {
      this->handleRotateDirection(request);
    });

  _server.on(
    "/api/air-level", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleAirLevel(request, data, len, index, total);
    });

  _server.on(
    "/api/adjust-air", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleAdjustAir(request, data, len, index, total);
    });

  _server.on("/api/power-off", HTTP_POST,
    [this](AsyncWebServerRequest *request)
    {
      this->handlePowerOff(request);
    });

  _server.on("/api/config", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleGetConfig(request);
    });

  _server.on(
    "/api/config", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleUpdateConfig(request, data, len, index, total);
    });

  _server.on("/api/reset-wifi", HTTP_POST,
    [this](AsyncWebServerRequest *request)
    {
      this->handleResetWifi(request);
    });

  _server.on("/api/arousal/status", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleGetArousalStatus(request);
    });

  _server.on(
    "/api/arousal/state", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleSetArousalState(request, data, len, index, total);
    });

  _server.on(
    "/api/arousal/sensitivity", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleSetArousalSensitivity(request, data, len, index, total);
    });

  _server.on("/api/arousal/config", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleGetArousalConfig(request);
    });

  _server.on(
    "/api/arousal/config", HTTP_POST, [](AsyncWebServerRequest *request) { /* Empty handler - we'll use the onBody handler */ }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, const size_t len, const size_t index, const size_t total)
    {
      this->handleUpdateArousalConfig(request, data, len, index, total);
    });

  _server.on("/api/update/status", HTTP_GET,
    [this](AsyncWebServerRequest *request)
    {
      this->handleGetUpdateStatus(request);
    });

  _server.on("/api/update/start", HTTP_POST,
    [this](AsyncWebServerRequest *request)
    {
      this->handleStartUpdate(request);
    });

  _server.on(
    "/api/update/upload", HTTP_POST,
    [this](AsyncWebServerRequest *request)
    {
      if (_nogasmUpdate.isUpdateInProgress())
      {
        const bool success = _nogasmUpdate.finishUpdate();
        if (success)
        {
          request->send(200, "application/json", R"({"success":true,"message":"Update successful, restarting..."})");
        }
        else
        {
          request->send(500, "application/json", R"({"success":false,"message":"Update failed"})");
        }
      }
      else
      {
        request->send(400, "application/json", R"({"success":false,"message":"No upload in progress"})");
      }
    },
    [this](const AsyncWebServerRequest *request, const String &filename, const size_t index, uint8_t *data, const size_t len, const bool final)
    {
      this->handleUpdateUpload(request, filename, index, data, len, final);
    });
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void NogasmHttp::sendJsonResponse(AsyncWebServerRequest *request, const JsonDocument &doc, const int code)
{
  String response;
  serializeJson(doc, response);
  request->send(code, "application/json", response);
}

void NogasmHttp::sendSuccessResponse(AsyncWebServerRequest *request, const bool success, const char *message)
{
  JsonDocument responseDoc;
  responseDoc["success"] = success;

  if (message != nullptr)
  {
    responseDoc["message"] = message;
  }

  sendJsonResponse(request, responseDoc);
}

void NogasmHttp::handleGetStatus(AsyncWebServerRequest *request)
{
  JsonDocument doc;

  // Add WiFi info (not included in WebSocket)
  doc["wifi"]["connected"] = WiFi.isConnected();
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["wifi"]["hostname"] = WiFi.getHostname();
  doc["wifi"]["rssi"] = WiFi.RSSI();

  // Add BLE info using shared method
  auto ble = doc["ble"].to<JsonObject>();
  generateBleStatusJson(ble);

  sendJsonResponse(request, doc);
}

void NogasmHttp::handleGetDevices(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  generateDevicesJson(doc);
  sendJsonResponse(request, doc);
}

void NogasmHttp::handleDisconnect(AsyncWebServerRequest *request)
{
  _bleManager.disconnectAndCleanupClient();
  _bleManager.updateStatus(BLE_IDLE, "User disconnect");
  sendSuccessResponse(request, true, "Disconnected current device");
}

void NogasmHttp::handleScan(AsyncWebServerRequest *request)
{
  const uint32_t scanDuration = _config.getScanDuration();
  _bleManager.startScan(scanDuration);

  sendSuccessResponse(request, true, "Scan started");
}

void NogasmHttp::handleConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);
  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  const String address = doc["address"].as<String>();
  _bleManager.connectToDevice(address.c_str());

  sendSuccessResponse(request, true);
}

void NogasmHttp::handleVibrate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);
  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  const int level = doc["level"].as<int>();
  if (level < 0 || level > 20)
  {
    sendSuccessResponse(request, false, "Level must be between 0 and 20");
    return;
  }

  if (!_bleManager.isConnectedState())
  {
    sendSuccessResponse(request, false, "No device connected");
    return;
  }

  const bool success = _bleManager.setVibrationLevel(level);

  JsonDocument responseDoc;
  responseDoc["success"] = success;
  responseDoc["level"] = level;

  sendJsonResponse(request, responseDoc);
}

void NogasmHttp::handleRotate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);
  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  const int level = doc["level"].as<int>();
  if (level < 0 || level > 20)
  {
    sendSuccessResponse(request, false, "Level must be between 0 and 20");
    return;
  }

  if (!_bleManager.isConnectedState())
  {
    sendSuccessResponse(request, false, "No device connected");
    return;
  }

  const bool success = _bleManager.setRotationLevel(level);
  JsonDocument responseDoc;
  responseDoc["success"] = success;
  responseDoc["level"] = level;

  sendJsonResponse(request, responseDoc);
}

void NogasmHttp::handleRotateDirection(AsyncWebServerRequest *request)
{
  if (!_bleManager.isConnectedState())
  {
    sendSuccessResponse(request, false, "No device connected");
    return;
  }

  const bool success = _bleManager.changeRotationDirection();
  sendSuccessResponse(request, success);
}

void NogasmHttp::handleAirLevel(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  const int level = doc["level"].as<int>();
  if (level < 0 || level > 4)
  {
    sendSuccessResponse(request, false, "Level must be between 0 and 5");
    return;
  }

  if (!_bleManager.isConnectedState())
  {
    sendSuccessResponse(request, false, "No device connected");
    return;
  }

  const bool success = _bleManager.setAirLevel(level);
  JsonDocument responseDoc;
  responseDoc["success"] = success;
  responseDoc["level"] = level;

  sendJsonResponse(request, responseDoc);
}

void NogasmHttp::handleAdjustAir(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  const bool inflate = doc["inflate"].as<bool>();
  const int amount = doc["amount"].as<int>();

  if (amount < 1 || amount > 4)
  {
    sendSuccessResponse(request, false, "Amount must be between 1 and 4");
    return;
  }

  if (!_bleManager.isConnectedState())
  {
    sendSuccessResponse(request, false, "No device connected");
    return;
  }

  const bool success = _bleManager.adjustAirLevelRelative(inflate, amount);
  sendSuccessResponse(request, success);
}

void NogasmHttp::handlePowerOff(AsyncWebServerRequest *request)
{
  if (!_bleManager.isConnectedState())
  {
    sendSuccessResponse(request, false, "No device connected");
    return;
  }

  const bool success = _bleManager.powerOffDevice();
  sendSuccessResponse(request, success);
}

void NogasmHttp::handleGetConfig(AsyncWebServerRequest *request)
{
  JsonDocument doc;

  doc["connection"]["scanDuration"] = _config.getScanDuration();
  doc["connection"]["connectionTimeout"] = _config.getConnectionTimeout();

  doc["device"]["defaultVibrationLevel"] = _config.getDefaultVibrationLevel();

  doc["ui"]["autoConnect"] = _config.getAutoConnect();
  doc["ui"]["autoReconnect"] = _config.getAutoReconnect();

  sendJsonResponse(request, doc);
}

void NogasmHttp::handleUpdateConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  bool configChanged = false;

  if (doc["connection"].is<JsonObject>())
  {
    if (!doc["connection"]["scanDuration"].isNull() && doc["connection"]["scanDuration"].is<uint32_t>())
    {
      const auto scanDuration = doc["connection"]["scanDuration"].as<uint32_t>();
      _config.setScanDuration(scanDuration);
      configChanged = true;
    }

    if (!doc["connection"]["connectionTimeout"].isNull() && doc["connection"]["connectionTimeout"].is<uint32_t>())
    {
      const auto connectionTimeout = doc["connection"]["connectionTimeout"].as<uint32_t>();
      _config.setConnectionTimeout(connectionTimeout);
      configChanged = true;
    }
  }

  if (doc["device"].is<JsonObject>())
  {
    if (!doc["device"]["defaultVibrationLevel"].isNull() && doc["device"]["defaultVibrationLevel"].is<uint8_t>())
    {
      const auto defaultVibrationLevel = doc["device"]["defaultVibrationLevel"].as<uint8_t>();
      _config.setDefaultVibrationLevel(defaultVibrationLevel);
      configChanged = true;
    }
  }

  if (doc["ui"].is<JsonObject>())
  {
    if (!doc["ui"]["autoConnect"].isNull() && doc["ui"]["autoConnect"].is<bool>())
    {
      const bool autoConnect = doc["ui"]["autoConnect"].as<bool>();
      _config.setAutoConnect(autoConnect);
      configChanged = true;
    }

    if (!doc["ui"]["autoReconnect"].isNull() && doc["ui"]["autoReconnect"].is<bool>())
    {
      const bool autoReconnect = doc["ui"]["autoReconnect"].as<bool>();
      _config.setAutoReconnect(autoReconnect);
      configChanged = true;
    }
  }

  if (configChanged)
  {
    const bool saved = _config.save();
    sendSuccessResponse(request, saved, saved ? "Configuration updated" : "Failed to save configuration");
  }
  else
  {
    sendSuccessResponse(request, true, "No changes to configuration");
  }
}

void NogasmHttp::handleResetWifi(AsyncWebServerRequest *request) const
{
  request->send(200, "text/plain", "WiFi settings will be reset. Please reconnect to the access point.");
  delay(1000);
  _wifiManager.resetSettings();
  ESP.restart();
}

// Arousal endpoint handlers
void NogasmHttp::handleGetArousalStatus(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  generateArousalStatusJson(doc);
  sendJsonResponse(request, doc);
}

void NogasmHttp::handleSetArousalState(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  if (!doc["reset"].isNull() && doc["reset"].as<bool>())
  {
    _arousalManager.end();
    _arousalManager.reset();
    _arousalManager.recalibratePressure();
    _arousalManager.begin();

    sendSuccessResponse(request, true, "Arousal reset, sensor recalibrated");
    return;
  }

  if (!doc["active"].isNull())
  {
    const bool active = doc["active"].as<bool>();
    if (active && !_arousalManager.isActive())
    {
      _arousalManager.begin();
      sendSuccessResponse(request, true, "Arousal manager started");
    }
    else if (!active && _arousalManager.isActive())
    {
      _arousalManager.end();
      sendSuccessResponse(request, true, "Arousal manager stopped");
    }
    else
    {
      sendSuccessResponse(request, true, "No state change needed");
    }
    return;
  }

  sendSuccessResponse(request, false, "Invalid request");
}

void NogasmHttp::handleSetArousalSensitivity(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  if (!doc["sensitivity"].isNull())
  {
    const int sensitivity = doc["sensitivity"].as<int>();
    if (sensitivity < 0 || sensitivity > 255)
    {
      sendSuccessResponse(request, false, "Sensitivity must be between 0 and 255");
      return;
    }

    _arousalManager.setSensitivity(sensitivity);
    _encoderManager.setEncoderValue(sensitivity, 0, 255);

    sendSuccessResponse(request, true, "Sensitivity updated");
    return;
  }

  sendSuccessResponse(request, false, "Invalid request");
}

void NogasmHttp::handleGetArousalConfig(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  generateArousalConfigJson(doc);
  sendJsonResponse(request, doc);
}

void NogasmHttp::handleUpdateArousalConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendSuccessResponse(request, false, "Invalid JSON");
    return;
  }

  // Get current config and modify with new values
  ArousalConfig config = _arousalManager.getConfig();

  if (!doc["arousalDecayRate"].isNull())
  {
    config.arousalDecayRate = doc["arousalDecayRate"].as<float>();
  }

  if (!doc["sensitivityThreshold"].isNull())
  {
    config.sensitivityThreshold = doc["sensitivityThreshold"].as<int>();
  }

  if (!doc["sensitivityAfterEdgeDecayRate"].isNull())
  {
    config.sensitivityAfterEdgeDecayRate = doc["sensitivityAfterEdgeDecayRate"].as<float>();
  }

  if (!doc["minSensitivityWhileDecaying"].isNull())
  {
    config.minSensitivityWhileDecaying = doc["minSensitivityWhileDecaying"].as<int>();
  }

  if (!doc["maxArousalLimit"].isNull())
  {
    config.maxArousalLimit = doc["maxArousalLimit"].as<int>();
  }

  if (!doc["maxVibrationLevel"].isNull())
  {
    const int maxVibrationLevel = constrain(doc["maxVibrationLevel"].as<int>(), 0, SPEED_LEVEL_MAX);
    config.maxSpeed = ArousalManager::levelToSpeed(maxVibrationLevel);
    Util::logTrace("NogasmHttp::maxSpeed = %d", config.maxSpeed);
  }

  if (!doc["frequency"].isNull())
  {
    config.frequency = doc["frequency"].as<int>();
  }

  if (!doc["rampTimeSeconds"].isNull())
  {
    config.rampTimeSeconds = doc["rampTimeSeconds"].as<float>();
  }

  if (!doc["coolTimeSeconds"].isNull())
  {
    config.coolTimeSeconds = doc["coolTimeSeconds"].as<float>();
  }

  if (!doc["clenchPressureSensitivity"].isNull())
  {
    config.clenchPressureSensitivity = doc["clenchPressureSensitivity"].as<int>();
  }

  if (!doc["clenchTimeMinThresholdMs"].isNull())
  {
    config.clenchTimeMinThresholdMs = doc["clenchTimeMinThresholdMs"].as<int>();
  }

  if (!doc["clenchTimeMaxThresholdMs"].isNull())
  {
    config.clenchTimeMaxThresholdMs = doc["clenchTimeMaxThresholdMs"].as<int>();
  }

  if (!doc["targetEdgeCount"].isNull())
  {
    config.targetEdgeCount = doc["targetEdgeCount"].as<int>();
  }

  // Update the config in the arousal manager
  _arousalManager.setConfig(config);

  // Save arousal config to persistent storage
  _config.setArousalConfig(config);
  const bool saved = _config.save();

  sendSuccessResponse(request, saved, saved ? "Arousal configuration updated" : "Failed to save configuration");
}

void NogasmHttp::handleGetUpdateStatus(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  generateUpdateStatusJson(doc);
  sendJsonResponse(request, doc);
}

void NogasmHttp::handleStartUpdate(AsyncWebServerRequest *request)
{
  if (_nogasmUpdate.isUpdateInProgress())
  {
    sendSuccessResponse(request, false, "Update already in progress");
    return;
  }

  if (!WiFi.isConnected())
  {
    sendSuccessResponse(request, false, "WiFi not connected");
    return;
  }

  JsonDocument responseDoc;
  responseDoc["success"] = true;
  responseDoc["message"] = "Update ready for uploads";
  responseDoc["hostname"] = WiFi.getHostname();
  responseDoc["ip"] = WiFi.localIP().toString();
  responseDoc["firmwareWarning"] = "Firmware update will restart the device";
  responseDoc["webUploadAvailable"] = true;

  sendJsonResponse(request, responseDoc);
}

void NogasmHttp::handleUpdateUpload(const AsyncWebServerRequest *request, const String &filename, const size_t index, uint8_t *data, const size_t len, const bool final) const
{
  if (!index)
  {
    Util::logInfo("Web Update Upload started: %s", filename.c_str());
    const UpdateMode updateMode = filename.endsWith(".bin") ? UpdateMode::FIRMWARE : UpdateMode::FILESYSTEM;

    // Stop all services
    _arousalManager.end();
    _bleManager.disconnectAndCleanupClient();

    // Get expected size from request
    const size_t expectedSize = request->contentLength();

    // Get MD5 hash if provided
    String md5Hash = "";
    if (request->hasParam("md5"))
    {
      md5Hash = request->getParam("md5")->value();
    }

    // Start the update
    if (!_nogasmUpdate.startUpdate(updateMode, expectedSize, md5Hash))
    {
      return;
    }
  }

  if (_nogasmUpdate.isUpdateInProgress())
  {
    if (!_nogasmUpdate.writeData(data, len))
    {
      return;
    }
  }

  // todo: possibly finalise here
}
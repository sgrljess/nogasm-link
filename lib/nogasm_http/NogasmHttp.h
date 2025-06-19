#ifndef NOGASM_HTTP_H
#define NOGASM_HTTP_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <set>
#include <WiFi.h>
#include "NogasmUpdate.h"
#include "NogasmBLEManager.h"
#include "NogasmConfig.h"
#include "EncoderManager.h"
#include "ArousalManager.h"

#define NOGASM_HTTP_PORT 8080

class NogasmHttp
{
 public:
  NogasmHttp(fs::FS& filesystem, NogasmBLEManager& bleManager, WiFiManager& wifiManager, NogasmConfig& config, ArousalManager& arousalManager, EncoderManager& encoderManager,
    NogasmUpdate& nogasmUpdate);

  void begin();
  void update();

 private:
  // Web server instance owned by NogasmHttp
  AsyncWebServer _server;
  AsyncWebSocket* _ws;

  // WebSocket client tracking
  std::set<AsyncWebSocketClient*> _wsClients;

  // WebSocket related timing
  unsigned long _lastBleUpdate;
  unsigned long _lastArousalUpdate;
  unsigned long _lastPingCheck;

  // State tracking for change detection
  BLEConnectionState _lastBleState;
  bool _lastArousalActive;

  // References to external dependencies
  fs::FS& _filesystem;
  NogasmBLEManager& _bleManager;
  WiFiManager& _wifiManager;
  NogasmConfig& _config;
  EncoderManager& _encoderManager;
  ArousalManager& _arousalManager;
  NogasmUpdate& _nogasmUpdate;

  void setupAPIEndpoints();
  void setupStaticFiles();
  void setupWebSocket();

  // API endpoint handlers - status
  void handleGetStatus(AsyncWebServerRequest* request);
  void handleGetDevices(AsyncWebServerRequest* request);
  void handleDisconnect(AsyncWebServerRequest* request);
  void handleScan(AsyncWebServerRequest* request);

  // API endpoint handlers - device control
  void handleVibrate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleRotate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleRotateDirection(AsyncWebServerRequest* request);
  void handleAirLevel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleAdjustAir(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePowerOff(AsyncWebServerRequest* request);

  // API endpoint handlers - configuration
  void handleGetConfig(AsyncWebServerRequest* request);
  void handleUpdateConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleResetWifi(AsyncWebServerRequest* request) const;
  void handleConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

  // API endpoint handlers - arousal control
  void handleGetArousalStatus(AsyncWebServerRequest* request);
  void handleSetArousalState(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleSetArousalSensitivity(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleGetArousalConfig(AsyncWebServerRequest* request);
  void handleUpdateArousalConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

  // API endpoint handlers - Update
  void handleGetUpdateStatus(AsyncWebServerRequest* request);
  void handleStartUpdate(AsyncWebServerRequest* request);
  void handleUpdateUpload(const AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) const;

  // WebSocket handlers
  void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  void handleWebSocketMessage(const AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len);
  void cleanupDisconnectedClients();

  bool canSendToClient(const AsyncWebSocketClient* client);
  void sendMessageToClient(AsyncWebSocketClient* client, const String& message);
  void broadcastMessage(const String& message);

  String createBleStatusMessage();
  String createArousalStatusMessage();
  String createUpdateStatusMessage();
  void sendBleStatusUpdate();
  void sendArousalStatusUpdate();
  void sendUpdateStatusUpdate();

  // Shared JSON generation methods
  template <typename T>
  void generateBleStatusJson(T& json);
  template <typename T>
  void generateArousalStatusJson(T& json);
  template <typename T>
  void generateDevicesJson(T& json);
  template <typename T>
  void generateArousalConfigJson(T& json);
  template <class T>
  void generateUpdateStatusJson(T& doc);

  // Helper methods
  void sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc, int code = 200);
  void sendSuccessResponse(AsyncWebServerRequest* request, bool success, const char* message = nullptr);
};

#endif
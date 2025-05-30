#ifndef AROUSAL_MANAGER_H
#define AROUSAL_MANAGER_H

#include <Arduino.h>
#include "PressureSensor.h"
#include "NogasmBLEManager.h"
#include "ArousalConfig.h"
#include "Util.h"

class ArousalManager
{
 public:
  ArousalManager(PressureSensor& sensor, NogasmBLEManager& bleManager);

  void begin();
  void end();
  void update();
  void toggle();

  bool isActive() const
  {
    return _started;
  }

  float getCurrentPressure() const
  {
    return _pressureSensor.getLastSmoothedPressure();
  }

  long getLastClenchDuration() const
  {
    return _clenchDurationMs;
  }

  void setConfig(const ArousalConfig& config)
  {
    _config = config;
  }

  const ArousalConfig& getConfig() const
  {
    return _config;
  }

  float getArousalPercent() const;

  void resetArousal()
  {
    _arousal = 0;
  }

  void setArousalLimit(const int limit)
  {
    _arousalLimit = limit;
  }

  int getArousalLimit() const
  {
    return _arousalLimit;
  }

  unsigned int getPressureLimit() const;

  void setSensitivity(const int sensitivity)
  {
    _arousalLimit = map(sensitivity, 0, 255, 1, _config.maxArousalLimit);
    notifyStateChange(ArousalState::AROUSAL_LIMIT_CHANGE);
    Util::logDebug("ArousalManager updated sensitivity: %d", sensitivity);
  }

  int getSensitivity() const
  {
    return map(_arousalLimit, 1, _config.maxArousalLimit, 0, 255);
  }

  void onStateChange(const std::function<void(const ArousalStateEvent&)>& callback)
  {
    _stateChangeCallback = callback;
  }

  ArousalState getCurrentState() const
  {
    return _currentState;
  }

  String getCurrentStateString() const;

  int getLimitExceededCounter() const
  {
    return _limitExceededCounter;
  }

  unsigned long getCurrentSessionDuration() const
  {
    if (!_started)
    {
      return 0;
    }

    return millis() - _sessionStartTime;
  }

 private:
  PressureSensor& _pressureSensor;
  NogasmBLEManager& _bleManager;
  ArousalConfig _config;

  bool _started = false;
  bool _limitExceeded = false;

  unsigned long _limitExceededTime = 0;
  unsigned long _lastUpdateTime = 0;
  unsigned long _sessionStartTime = 0;

  int _limitExceededCounter = 0;
  int _arousalLimit = 4000;
  float _arousal = 0;
  float _lastPressureValue = 0;
  float _peakStart = 0;
  float _vibrationSpeed = 0;
  uint8_t _lastVibrationLevel = 0;

  long _clenchDurationMs = 0;
  unsigned long _clenchStartTime = 0;

  ArousalState _currentState = ArousalState::IDLE;
  std::function<void(const ArousalStateEvent&)> _stateChangeCallback = nullptr;

  long detectClench(unsigned long currentTime, float pressure);
  void updateVibrationLevel(uint8_t level);
  void notifyStateChange(ArousalState newState, long clenchDuration = 0);
};

#endif
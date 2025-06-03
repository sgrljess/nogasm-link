// ReSharper disable CppExpressionWithoutSideEffects
#include "ArousalManager.h"

ArousalManager::ArousalManager(PressureSensor& sensor, NogasmBLEManager& bleManager) : _pressureSensor(sensor), _bleManager(bleManager)
{
  _arousalLimit = _config.maxArousalLimit;
}

void ArousalManager::toggle()
{
  if (_started)
  {
    end();
  }
  else
  {
    begin();
  }
}

void ArousalManager::reset()
{
  _arousal = 0;
  _lastPressureValue = 0;
  _peakStart = 0;
  _lastVibrationLevel = 0;
  _vibrationSpeed = 0;
  _lastUpdateTime = 0;
  _limitExceeded = false;
  _limitExceededTime = 0;
  _limitExceededCounter = 0;
}

void ArousalManager::begin()
{
  reset();
  _started = true;
  _sessionStartTime = millis();
  Util::logDebug("ArousalManager started with pressure limit: %d", _arousalLimit);
  notifyStateChange(ArousalState::IDLE);
}

void ArousalManager::end()
{
  _started = false;
  updateVibrationLevel(0);
  notifyStateChange(ArousalState::IDLE);
  Util::logDebug("Stopped ArousalManager");
}

void ArousalManager::notifyStateChange(const ArousalState newState, const long clenchDuration)
{
  _currentState = newState;
  if (_stateChangeCallback)
  {
    ArousalStateEvent event{};
    event.state = newState;
    event.arousal = _arousal;
    event.arousalLimit = _arousalLimit;
    event.pressure = _pressureSensor.getLastSmoothedPressure();
    event.maxPressureLimit = _pressureSensor.getMaxPressureLimitRaw();
    event.arousalPercent = getArousalPercent();
    event.vibratorSpeed = _vibrationSpeed;
    event.vibrationLevel = _lastVibrationLevel;
    event.clenchDuration = clenchDuration;
    event.clenchThreshold = _config.clenchPressureThreshold;
    event.limitExceededCounter = _limitExceededCounter;

    _stateChangeCallback(event);
  }
}

String ArousalManager::getCurrentStateString() const
{
  switch (_currentState)
  {
    case ArousalState::IDLE:
      return "IDLE";
    case ArousalState::AROUSAL_LIMIT_CHANGE:
      return "AROUSAL_LIMIT_CHANGE";
    case ArousalState::AROUSAL_INCREASE:
      return "AROUSAL_INCREASE";
    case ArousalState::VIBRATION_CHANGE:
      return "VIBRATION_CHANGE";
    case ArousalState::LIMIT_EXCEEDED:
      return "LIMIT_EXCEEDED";
    case ArousalState::LIMIT_EXCEEDED_BUT_IGNORED:
      return "LIMIT_EXCEEDED_BUT_IGNORED";
    case ArousalState::COOL_OFF_ACTIVE:
      return "COOL_OFF_ACTIVE";
    case ArousalState::COOL_OFF_ENDED:
      return "COOL_OFF_ENDED";
    case ArousalState::CLENCH_DETECTED:
      return "CLENCH_DETECTED";
    case ArousalState::THRESHOLD_ADJUSTED:
      return "THRESHOLD_ADJUSTED";
    case ArousalState::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

void ArousalManager::update()
{
  if (!_started)
  {
    return;
  }

  const unsigned long currentTime = millis();
  const unsigned long updatePeriod = 1000 / _config.frequency;
  if (!Util::hasTimeExpired(updatePeriod, _lastUpdateTime))
  {
    return;
  }

  _arousal *= _config.arousalDecayRate;

  const float speedIncrement = (static_cast<float>(_config.maxSpeed) / (static_cast<float>(_config.frequency) * _config.rampTimeSeconds));
  const float pressure = _pressureSensor.readSmoothedPressure();

  if (!_pressureSensor.isReady())
  {
    Util::logTrace("Filling pressure buffer with readings...");
    return;
  }

  Util::logDebug("ArousalManager::arousal: %.2f, arousal_limit:%d, pressure: %.2f", _arousal, _arousalLimit, pressure);

  // pressure is almost at ADC max, need to adjust trim-pot for op-amp
  if (pressure >= _pressureSensor.getMaxPressureLimitRaw())
  {
    notifyStateChange(ArousalState::ERROR);
    return;
  }

  if (pressure < _lastPressureValue)
  {
    if (_lastPressureValue > _peakStart)
    {
      const float sensitivityThreshold = static_cast<float>(_config.sensitivityThreshold) / 10.0f;
      if (_lastPressureValue - _peakStart > sensitivityThreshold)
      {
        _arousal += (_lastPressureValue - _peakStart);
        notifyStateChange(ArousalState::AROUSAL_INCREASE);
        Util::logDebug("ArousalManager::increase -> arousal=%.2f, limit=%d, increased_by=%.2f", _arousal, _arousalLimit, _lastPressureValue - _peakStart);
      }
    }
    _peakStart = pressure;
  }

  _lastPressureValue = pressure;
  _lastUpdateTime = currentTime;

  const long clenchDuration = detectClench(currentTime, pressure);
  if (clenchDuration > 0)
  {
    Util::logTrace("ArousalManager::clench::duration -> %dms", clenchDuration);
    notifyStateChange(ArousalState::CLENCH_DETECTED, clenchDuration);

    if (clenchDuration > _config.clenchTimeMinThresholdMs && clenchDuration < _config.clenchTimeMaxThresholdMs)
    {
      // todo: figure out a better way to scale the clench duration to arousal increase
      const float arousalIncrease = map(clenchDuration, _config.clenchTimeMinThresholdMs, _config.clenchTimeMaxThresholdMs, 5.0f, 35.0f);
      _arousal += arousalIncrease;

      notifyStateChange(ArousalState::AROUSAL_INCREASE);
      Util::logDebug("ArousalManager::clench::arousal::increase -> arousal=%.2f, limit=%d, duration:%dms", _arousal, _arousalLimit, clenchDuration);
    }
  }

  const unsigned long coolOffPeriodMs = _config.coolTimeSeconds * 1000;
  const bool orgasmAllowed = _limitExceededCounter >= _config.targetEdgeCount;
  const bool inCoolOffPeriod = _limitExceeded && (currentTime - _limitExceededTime < coolOffPeriodMs);

  if (_arousal > _arousalLimit)
  {
    if (!_limitExceeded)
    {
      _limitExceeded = true;
      _limitExceededTime = currentTime;
      _limitExceededCounter++;

      if (orgasmAllowed)
      {
        notifyStateChange(ArousalState::LIMIT_EXCEEDED_BUT_IGNORED);
        Util::logDebug("ArousalManager::edge::limit -> allowing orgasm....");
      }
      else
      {
        _vibrationSpeed = 0;

        // decay limit each time we exceeded the limit (1.0 to disable this)
        const float newArousalLimit = _arousalLimit * constrain(_config.sensitivityAfterEdgeDecayRate, 0.0f, 1.0f);
        _arousalLimit = static_cast<int>(newArousalLimit);
        if (_arousalLimit <= 0)
        {
          _arousalLimit = map(constrain(_config.minSensitivityWhileDecaying, 10, 255), 0, 255, 0, _pressureSensor.getMaxPressureLimitRaw());
        }

        notifyStateChange(ArousalState::LIMIT_EXCEEDED);
        Util::logDebug("ArousalManager::vibration::off -> %.2f", _vibrationSpeed);
      }
    }
  }
  else if (inCoolOffPeriod)
  {
    if (!orgasmAllowed)
    {
      if (currentTime % 500 <= updatePeriod)
      {
        notifyStateChange(ArousalState::COOL_OFF_ACTIVE);
        Util::logDebug("ArousalManager::cool-off -> remaining time: %dms", coolOffPeriodMs - (currentTime - _limitExceededTime));
      }
    }
  }
  else if (_limitExceeded && !inCoolOffPeriod)
  {
    _limitExceeded = false;

    if (!orgasmAllowed)
    {
      notifyStateChange(ArousalState::COOL_OFF_ENDED);
      Util::logDebug("ArousalManager::cool-off::ended -> resuming vibration");
    }
  }
  else if (_vibrationSpeed < _config.maxSpeed)
  {
    Util::logTrace("ArousalManager::vibration ramping -> %.2f", _vibrationSpeed);
    _vibrationSpeed += speedIncrement;
  }

  if (_vibrationSpeed > 0)
  {
    const uint8_t vibrationLevel = speedToLevel(constrain(_vibrationSpeed, 0, _config.maxSpeed));
    updateVibrationLevel(vibrationLevel);
  }
  else
  {
    _vibrationSpeed = 0;
    updateVibrationLevel(0);
  }
}

long ArousalManager::detectClench(const unsigned long currentTime, const float pressure)
{
  const auto sensitivityValue = static_cast<float>(_config.clenchPressureSensitivity);
  Util::logTrace("detectClench() -> pressure=%.2f, threshold=%.2f, sensitivity=%.2f", pressure, _config.clenchPressureThreshold, sensitivityValue);

  // adjust threshold back if inflated during detection
  if (pressure > (_config.clenchPressureThreshold + _config.clenchPressureSensitivity))
  {
    _config.clenchPressureThreshold = constrain(pressure - sensitivityValue, 0, _pressureSensor.getMaxPressureLimitRaw());
    notifyStateChange(ArousalState::THRESHOLD_ADJUSTED);

    Util::logDebug("ArousalManager::threshold::adjust -> %.2f", _config.clenchPressureThreshold);
  }

  if (pressure > _config.clenchPressureThreshold)
  {
    _clenchDurationMs = millis() - _clenchStartTime;
    Util::logTrace("ArousalManager::clench::valid -> duration=%dms, pressure=%.2f, threshold=%.2f", _clenchDurationMs, pressure, _config.clenchPressureThreshold);

    // autocorrect threshold if clench is longer than the configured maximum
    if (_clenchDurationMs >= _config.clenchTimeMaxThresholdMs)
    {
      // increase by the sensitivity value to escape clench
      _config.clenchPressureThreshold = pressure + sensitivityValue;
      notifyStateChange(ArousalState::THRESHOLD_ADJUSTED);
      Util::logDebug("ArousalManager::clench::threshold::decrease (max_exceeded) -> %.2f", _config.clenchPressureThreshold);

      return 0;
    }

    return _clenchDurationMs;
  }

  _clenchStartTime = millis();
  _clenchDurationMs -= 150;
  if (_clenchDurationMs <= 0)
  {
    _clenchDurationMs = 0;
    if (pressure + sensitivityValue < _config.clenchPressureThreshold)
    {
      _config.clenchPressureThreshold *= 0.99;
      notifyStateChange(ArousalState::THRESHOLD_ADJUSTED);

      Util::logDebug("ArousalManager::threshold::decrease -> %.2f", _config.clenchPressureThreshold);
    }
  }

  return 0;
}

void ArousalManager::updateVibrationLevel(const uint8_t level)
{
  if (level != _lastVibrationLevel)
  {
    notifyStateChange(ArousalState::VIBRATION_CHANGE);
    Util::logDebug("ArousalManager::vibration::update -> %d", level);

    if (_bleManager.isConnectedState())
    {
      _bleManager.setVibrationLevel(level);
    }
  }

  _lastVibrationLevel = level;
}

float ArousalManager::getArousalPercent() const
{
  return constrain(100.0 * _arousal / (float)_arousalLimit, 0, 100);
}

unsigned int ArousalManager::getPressureLimit() const
{
  return _pressureSensor.getMaxPressureLimitRaw();
}
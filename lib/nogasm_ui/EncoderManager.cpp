#include "EncoderManager.h"
#include <Util.h>

// Define a static pointer to access the instance from the static ISR method
static EncoderManager* encoderInstance = nullptr;

EncoderManager::EncoderManager(ArousalManager& arousalManager) : _arousalManager(arousalManager)
{
  encoderInstance = this;
}

static void IRAM_ATTR readEncoderISR()
{
  if (encoderInstance)
  {
    encoderInstance->_encoder.readEncoder_ISR();
  }
}

void EncoderManager::setEncoderValue(int value, const int inMin, const int inMax)
{
  value = map(value, inMin, inMax, ENCODER_MIN, ENCODER_MAX);
  _encoder.setEncoderValue(value);
}

void EncoderManager::begin()
{
  _encoder.begin();
  _encoder.setup(readEncoderISR);

  // Configure encoder range for sensitivity control (0-255)
  _encoder.setBoundaries(ENCODER_MIN, ENCODER_MAX, false);
  setEncoderValue(_arousalManager.getArousalLimit(), 1, _arousalManager.getConfig().maxArousalLimit);

  Util::logInfo("EncoderManager::init -> value:%d", _encoder.readEncoder());
}

void EncoderManager::update()
{
  // Handle rotation
  if (_encoder.encoderChanged())
  {
    const int value = _encoder.readEncoder();
    Util::logTrace("EncoderManager::change -> %d", value);

    _arousalManager.setSensitivity(value);

    if (_valueChangedCallback)
    {
      _valueChangedCallback(value);
    }
  }

  // Handle button press with debouncing
  const bool currentState = _encoder.isEncoderButtonDown();
  if (currentState && !_buttonPressed && millis() - _buttonPressTime > BUTTON_DEBOUNCE_MS)
  {
    _buttonPressed = true;
    _buttonPressTime = millis();

    Util::logTrace("EncoderManager::button -> toggling ArousalManager");
    _arousalManager.toggle();

    // Call callback if registered
    if (_buttonPressedCallback)
    {
      _buttonPressedCallback();
    }
  }
  else if (!currentState && _buttonPressed)
  {
    _buttonPressed = false;
  }
}
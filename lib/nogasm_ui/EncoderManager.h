#ifndef ENCODER_MANAGER_H
#define ENCODER_MANAGER_H

#include <Arduino.h>
#include <AiEsp32RotaryEncoder.h>
#include "ArousalManager.h"

#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 35
#define ROTARY_ENCODER_STEPS 4

#define ENCODER_MIN 0
#define ENCODER_MAX 255

class EncoderManager
{
 public:
  explicit EncoderManager(ArousalManager& arousalManager);
  void begin();
  void update();

  void setEncoderValue(int value, int inMin, int inMax);

  void onEncoderValueChanged(std::function<void(int)> callback)
  {
    _valueChangedCallback = callback;
  }

  void onButtonPressed(std::function<void()> callback)
  {
    _buttonPressedCallback = callback;
  }

  AiEsp32RotaryEncoder _encoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);

 private:
  ArousalManager& _arousalManager;
  std::function<void(int)> _valueChangedCallback = nullptr;
  std::function<void()> _buttonPressedCallback = nullptr;

  bool _buttonPressed = false;
  unsigned long _buttonPressTime = 0;
  static constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
};

#endif
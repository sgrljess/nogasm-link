#ifndef RGB_MANAGER_H
#define RGB_MANAGER_H

#include <Arduino.h>
#include <map>

enum class AnimationType
{
  NONE,    // No animation, static color
  FLASH,   // Flashing between color and off
  PULSE,   // Smooth sine wave pulsing
  BREATHE  // Breathing effect (modified sine)
};

enum class LEDState
{
  OFF,
  WIFI_SETUP_REQUIRED,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  SCANNING,
  CONNECTING,
  CONNECTED,
  BLE_ERROR,
  PRESSURE_ERROR,
  AROUSAL_ACTIVE,
  AROUSAL_ALLOW_ORGASM
};

struct LEDStateConfig
{
  LEDState state;           // The LED state this config applies to
  uint8_t red;              // Red component (0-255)
  uint8_t green;            // Green component (0-255)
  uint8_t blue;             // Blue component (0-255)
  AnimationType animation;  // Animation type
  unsigned long duration;   // Duration in ms
  bool continuous;          // Whether animation repeats continuously
  String description;       // the description for this state
};

class RGBManager
{
 public:
  RGBManager(uint8_t redPin, uint8_t greenPin, uint8_t bluePin);

  void begin();
  void update();

  void setLEDState(LEDState state);
  LEDState getLEDState() const
  {
    return _currentState;
  }

  void setColor(uint8_t red, uint8_t green, uint8_t blue);
  void setBrightness(uint8_t brightness);  // 0-255

 private:
  uint8_t _redPin;
  uint8_t _greenPin;
  uint8_t _bluePin;

  LEDState _currentState = LEDState::OFF;

  unsigned long _lastUpdateTime = 0;
  unsigned long _animationStartTime = 0;
  unsigned long _animationDuration = 0;

  uint8_t _brightness = 255;

  uint8_t _red = 0;
  uint8_t _green = 0;
  uint8_t _blue = 0;

  uint8_t _targetRed = 0;
  uint8_t _targetGreen = 0;
  uint8_t _targetBlue = 0;

  bool _isAnimating = false;
  AnimationType _currentAnimation = AnimationType::NONE;
  bool _isContinuous = false;

  std::map<LEDState, LEDStateConfig> _stateConfigs;

  void initStateConfigs();
  void applyStateConfig(const LEDStateConfig& config);
  void updateAnimation();
  void updateLED() const;
  void restartCurrentAnimation();
};

#endif  // RGB_MANAGER_H
#ifndef AROUSAL_CONFIG_H
#define AROUSAL_CONFIG_H

struct ArousalConfig
{
  float arousalDecayRate = 0.990;               // How quickly arousal decays (percentage)
  float sensitivityAfterEdgeDecayRate = 0.995;  // How much sensitivity decreases after an edge (percentage)
  int minSensitivityWhileDecaying = 40;         // The minimum sensitivity to decay too (0-255)
  int sensitivityThreshold = 70;                // Threshold for peak detection (adc value)
  int maxArousalLimit = 4000;                   // Default pressure limit (adc value)
  int maxSpeed = 255;                           // Maximum vibration speed (0-255, pwm value)
  int frequency = 60;                           // Update frequency (Hz)
  int targetEdgeCount = 20;                     // Amount of edges before orgasm is allowed
  float rampTimeSeconds = 50.0;                 // Time to ramp up vibration (seconds)
  float coolTimeSeconds = 15.0;                 // Time to cool down (seconds)

  float clenchPressureThreshold = maxArousalLimit;  // Initial clench threshold
  int clenchPressureSensitivity = 20;               // Sensitivity for clench detection
  int clenchTimeMinThresholdMs = 250;               // Minimum time for clench detection (ms)
  int clenchTimeMaxThresholdMs = 3500;              // Maximum time for clench detection (ms)
};

enum class ArousalState
{
  ERROR,                       // The system state is in an error (max pressure limit)
  IDLE,                        // Not active or just started
  AROUSAL_LIMIT_CHANGE,        // Sensitivity level changed
  AROUSAL_INCREASE,            // Arousal level changed
  VIBRATION_CHANGE,            // Vibration intensity changed
  LIMIT_EXCEEDED,              // Arousal exceeded limit - start cool-off
  LIMIT_EXCEEDED_BUT_IGNORED,  // Arousal exceeded limit, but target edge count reached
  COOL_OFF_ACTIVE,             // Currently in cool-off period
  COOL_OFF_ENDED,              // Cool-off period ended
  CLENCH_DETECTED,             // Clench has been detected
  THRESHOLD_ADJUSTED           // A threshold has been automatically adjusted
};

struct ArousalStateEvent
{
  ArousalState state;
  float pressure;
  float arousal;
  float arousalPercent;
  float clenchThreshold;
  float vibratorSpeed;  // 0-255
  int vibrationLevel;   // 1-20
  int arousalLimit;
  int maxPressureLimit;
  int limitExceededCounter;
  long clenchDuration;
};

#endif
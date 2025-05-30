#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include <Arduino.h>
#include <RunningAverage.h>

#define RA_DEFAULT_SAMPLES 15
#define MAX_PRESSURE_LIMIT 4030

class PressureSensor
{
 public:
  explicit PressureSensor(uint8_t pin, uint8_t samples = 15);

  void begin();
  void calibrateZero(uint16_t samples = RA_DEFAULT_SAMPLES);

  int readRawPressure();         // Direct ADC reading (0-4095)
  float readSmoothedPressure();  // Smoothed reading using RunningAverage
  bool isReady() const;

  /**
   * @return the max pressure limit for this sensor, taking into account the zero offset
   */
  unsigned int getMaxPressureLimitRaw() const
  {
    return constrain(MAX_PRESSURE_LIMIT - static_cast<int>(_offset), 0, MAX_PRESSURE_LIMIT);
  }

  // the last value that was read
  int getLastRawPressure() const
  {
    return _lastValue;
  }

  // the last smoothed value that was read
  float getLastSmoothedPressure() const
  {
    return _lastValueSmoothed;
  }

  void setOffset(const float offset)
  {
    _offset = offset;
  }
  void setScale(const float scale)
  {
    _scale = scale;
  }

 private:
  uint8_t _pin;
  int _lastValue = 0;              // the last value that was read
  float _lastValueSmoothed = 0.0;  // the last smoothed value that was read
  float _offset = 0.0;             // Offset for calibration
  float _scale = 1.0;              // Scale for calibration
  RunningAverage _pressureRA;
};

#endif
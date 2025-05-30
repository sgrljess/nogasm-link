#include "PressureSensor.h"
#include <Util.h>

PressureSensor::PressureSensor(const uint8_t pin, const uint8_t samples) : _pin(pin), _pressureRA(samples)
{
}

void PressureSensor::begin()
{
  // ESP32 has 12-bit ADC (0-4095)
  analogReadResolution(12);

  // Initialize running average
  _pressureRA.clear();

  // Set pin as input
  pinMode(_pin, INPUT);

  Util::logDebug("Starting pressure sensor with %d samples", _pressureRA.getSize());
}

int PressureSensor::readRawPressure()
{
  // Read analog value directly (0-4095 range for ESP32 ADC)
  const int rawValue = analogRead(_pin);
  // Apply calibration (offset is subtracted, scale is multiplied)
  const float calibratedValue = (static_cast<float>(rawValue) - _offset) * _scale;
  // Constrain to 12-bit range
  _lastValue = constrain((int)calibratedValue, 0, getMaxPressureLimitRaw());

  return _lastValue;
}

float PressureSensor::readSmoothedPressure()
{
  const int pressure = readRawPressure();
  _pressureRA.addValue(static_cast<float>(pressure));
  _lastValueSmoothed = _pressureRA.getAverage();

  return _lastValueSmoothed;
}
bool PressureSensor::isReady() const
{
  return _pressureRA.bufferIsFull();
}

void PressureSensor::calibrateZero(uint16_t samples)
{
  float sum = 0.0;

  _pressureRA.clear();

  for (uint16_t i = 0; i < samples; i++)
  {
    sum += static_cast<float>(analogRead(_pin));
    delay(10);
  }

  _offset = sum / static_cast<float>(samples);
  Util::logDebug("Pressure sensor zero calibrated to offset: %.2f (raw ADC value)", _offset);
}
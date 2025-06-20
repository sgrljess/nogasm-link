#pragma once

#include <Arduino.h>

// nogasm default log level (0=off, 1=info, 2=debug, 3=trace)
#define NOGASM_LOG_LEVEL 1

namespace Util
{
void logTrace(const char* format, ...);
void logDebug(const char* format, ...);
void logInfo(const char* format, ...);
void logInternal(int level, const char* format, ...);

bool hasTimeExpired(unsigned int expiryTimeMillis, unsigned long lastTimestamp);

inline long mapWithRound(const long x, const long in_min, const long in_max, const long out_min, const long out_max)
{
  return lround(static_cast<double>(x - in_min) * (out_max - out_min) / static_cast<double>(in_max - in_min) + out_min);
}

String formatBytes(size_t bytes);
}  // namespace Util
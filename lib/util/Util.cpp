#include <Util.h>

namespace Util
{
void logInternal(const int level, const char* format, const va_list args)
{
  if (level > NOGASM_LOG_LEVEL)
  {
    return;
  }

  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);

  if (level == 1)
  {
    Serial.print("[INFO]  ");
  }
  else if (level == 2)
  {
    Serial.print("[DEBUG] ");
  }
  else if (level == 3)
  {
    Serial.print("[TRACE] ");
  }

  Serial.println(buffer);
}

void logTrace(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  logInternal(3, format, args);
  va_end(args);
}

void logDebug(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  logInternal(2, format, args);
  va_end(args);
}

void logInfo(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  logInternal(1, format, args);
  va_end(args);
}

bool hasTimeExpired(const unsigned int expiryTimeMillis, const unsigned long lastTimestamp)
{
  long elapsedTime = millis() - expiryTimeMillis;
  if (elapsedTime < 0)
  {
    // in case the system clock just started
    elapsedTime = 0;
  }

  // logTrace("   ->> millis:%d, expiry:%d, elapsed:%d, last:%d, result:%d",  //
  //   millis(), expiryTimeMillis, elapsedTime, lastTimestamp, elapsedTime > lastTimestamp);
  return elapsedTime > lastTimestamp;
}

String formatBytes(const size_t bytes)
{
  if (bytes < 1024)
  {
    return String(bytes) + "B";
  }
  if (bytes < (1024 * 1024))
  {
    return String(bytes / 1024.0) + "KB";
  }
  if (bytes < (1024 * 1024 * 1024))
  {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
  return String("Large");
}
}  // namespace Util
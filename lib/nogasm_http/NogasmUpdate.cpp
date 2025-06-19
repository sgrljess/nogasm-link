#include "NogasmUpdate.h"
#include <Util.h>

NogasmUpdate::NogasmUpdate()
{
  _progress = {};
  _progress.state = UpdateState::IDLE;
  _progress.hasError = false;
}

void NogasmUpdate::begin()
{
  _active = true;
  Util::logInfo("NogasmUpdate initialized");
}

void NogasmUpdate::update() const
{
  if (_rebootScheduled && Util::hasTimeExpired(2000, _rebootRequestMs))
  {
    Util::logInfo("Rebooting after successful update...");
    ESP.restart();
  }
}

void NogasmUpdate::end()
{
  if (_updateInProgress)
  {
    abortUpdate();
  }

  _active = false;
  _rebootScheduled = false;
  _progress.state = UpdateState::IDLE;
  Util::logInfo("NogasmUpdate stopped");
}

bool NogasmUpdate::isActive() const
{
  return _active;
}

bool NogasmUpdate::isUpdateInProgress() const
{
  return _updateInProgress;
}

UpdateProgress NogasmUpdate::getProgress() const
{
  return _progress;
}

void NogasmUpdate::onStart(const std::function<void(UpdateMode)>& callback)
{
  _onStartCallback = callback;
}

void NogasmUpdate::onProgress(const std::function<void(const UpdateProgress&)>& callback)
{
  _onProgressCallback = callback;
}

void NogasmUpdate::onEnd(const std::function<void(bool)>& callback)
{
  _onEndCallback = callback;
}

void NogasmUpdate::onError(const std::function<void(const String&)>& callback)
{
  _onErrorCallback = callback;
}

bool NogasmUpdate::startUpdate(const UpdateMode mode, const size_t expectedSize, const String& md5Hash)
{
  if (_updateInProgress)
  {
    setError("Update already in progress");
    return false;
  }

  if (!_active)
  {
    setError("NogasmUpdate not active");
    return false;
  }

  _updateCommand = (mode == UpdateMode::FIRMWARE) ? U_FLASH : U_SPIFFS;
  _expectedSize = expectedSize;
  _written = 0;

  const String typeStr = (mode == UpdateMode::FIRMWARE) ? "firmware" : "filesystem";
  if (mode == UpdateMode::FIRMWARE)
  {
    Util::logInfo("⚠️  WARNING: Firmware update starting - device will restart after completion");
  }
  else
  {
    Util::logInfo("✅ Safe filesystem update starting");
  }

  Util::logInfo("Update starting: %s (expected size: %zu bytes)", typeStr.c_str(), expectedSize);

  _progress.mode = mode;
  _progress.state = UpdateState::STARTING;
  _progress.startTime = millis();
  _progress.current = 0;
  _progress.total = expectedSize;
  _progress.percentage = 0.0;
  _progress.error = "";
  _progress.hasError = false;
  _progress.md5Hash = md5Hash;

  if (!md5Hash.isEmpty())
  {
    if (!Update.setMD5(md5Hash.c_str()))
    {
      setError("Invalid MD5 hash format");
      return false;
    }
    Util::logInfo("MD5 hash set: %s", md5Hash.c_str());
  }

  if (!Update.begin(expectedSize > 0 ? expectedSize : UPDATE_SIZE_UNKNOWN, _updateCommand))
  {
    setError("Failed to begin update: " + getUpdateErrorString());
    return false;
  }

  _updateInProgress = true;
  _progress.state = UpdateState::IN_PROGRESS;

  updateProgress();

  if (_onStartCallback)
  {
    _onStartCallback(mode);
  }

  return true;
}

bool NogasmUpdate::writeData(uint8_t* data, const size_t len)
{
  if (!_updateInProgress)
  {
    setError("No update in progress");
    return false;
  }

  const size_t written = Update.write(data, len);
  if (written != len)
  {
    setError("Write failed: " + getUpdateErrorString());
    abortUpdate();
    return false;
  }

  _written += written;
  _progress.current = _written;
  _progress.percentage = (_expectedSize > 0) ? (float(_written) / float(_expectedSize)) * 100.0 : 0.0;

  updateProgress();

  static size_t lastLoggedBytes = 0;
  static float lastLoggedPercentage = 0;

  if ((_written - lastLoggedBytes >= 65536) || (_progress.percentage - lastLoggedPercentage >= 10.0))
  {
    Util::logInfo("Update Progress: %.1f%% (%zu/%zu bytes)", _progress.percentage, _written, _expectedSize);
    lastLoggedBytes = _written;
    lastLoggedPercentage = _progress.percentage;
  }

  return true;
}

bool NogasmUpdate::finishUpdate()
{
  if (!_updateInProgress)
  {
    setError("No update in progress");
    return false;
  }

  const bool success = Update.end(true);
  _updateInProgress = false;
  _progress.endTime = millis();

  if (success)
  {
    _progress.state = UpdateState::COMPLETED;
    _progress.percentage = 100.0;
    _progress.current = _progress.total;

    const unsigned long duration = _progress.endTime - _progress.startTime;
    Util::logInfo("Update completed successfully in %lu ms", duration);

    updateProgress();

    if (_onEndCallback)
    {
      _onEndCallback(true);
    }

    scheduleReboot();
    return true;
  }

  setError("Failed to finalize update: " + getUpdateErrorString());
  if (_onEndCallback)
  {
    _onEndCallback(false);
  }

  return false;
}

void NogasmUpdate::abortUpdate()
{
  if (_updateInProgress)
  {
    Update.abort();
    _updateInProgress = false;
    _progress.state = UpdateState::ERROR;
    _progress.endTime = millis();

    if (_progress.error.isEmpty())
    {
      _progress.error = "Update aborted";
    }

    updateProgress();

    Util::logInfo("Update aborted");

    if (_onEndCallback)
    {
      _onEndCallback(false);
    }
  }
}

void NogasmUpdate::setError(const String& error)
{
  _progress.state = UpdateState::ERROR;
  _progress.error = error;
  _progress.hasError = true;
  _progress.endTime = millis();

  if (_updateInProgress)
  {
    Update.abort();
    _updateInProgress = false;
  }

  updateProgress();

  Util::logInfo("Update Error: %s", error.c_str());
  if (_onErrorCallback)
  {
    _onErrorCallback(error);
  }
}

String NogasmUpdate::getUpdateErrorString() const
{
  if (Update.hasError())
  {
    StreamString str;
    Update.printError(str);
    return str.c_str();
  }
  return "Unknown error";
}

void NogasmUpdate::scheduleReboot()
{
  _rebootRequestMs = millis();
  _rebootScheduled = true;
  Util::logInfo("Reboot scheduled in 2 seconds...");
}

void NogasmUpdate::updateProgress() const
{
  notifyCallbacks();
}

void NogasmUpdate::notifyCallbacks() const
{
  if (_onProgressCallback)
  {
    _onProgressCallback(_progress);
  }
}
#ifndef NOGASM_UPDATE_H
#define NOGASM_UPDATE_H

#include <Arduino.h>
#include <Update.h>
#include <functional>
#include "StreamString.h"

enum class UpdateMode
{
  FIRMWARE,
  FILESYSTEM
};

enum class UpdateState
{
  IDLE,
  STARTING,
  IN_PROGRESS,
  COMPLETED,
  ERROR
};

struct UpdateProgress
{
  UpdateMode mode;
  UpdateState state;
  size_t total;
  size_t current;
  float percentage;
  String error;
  String md5Hash;
  unsigned long startTime;
  unsigned long endTime;
  bool hasError;
};

class NogasmUpdate
{
 public:
  NogasmUpdate();

  void begin();
  void update() const;
  void end();

  bool startUpdate(UpdateMode mode, size_t expectedSize = 0, const String& md5Hash = "");
  bool writeData(uint8_t* data, size_t len);
  bool finishUpdate();
  void abortUpdate();

  bool isActive() const;
  bool isUpdateInProgress() const;
  UpdateProgress getProgress() const;

  void onStart(const std::function<void(UpdateMode)>& callback);
  void onProgress(const std::function<void(const UpdateProgress&)>& callback);
  void onEnd(const std::function<void(bool)>& callback);
  void onError(const std::function<void(const String&)>& callback);

 private:
  bool _active = false;
  bool _updateInProgress = false;
  bool _rebootScheduled = false;
  UpdateProgress _progress;

  int _updateCommand = U_FLASH;
  size_t _expectedSize = 0;
  size_t _written = 0;
  unsigned long _rebootRequestMs = 0;

  std::function<void(UpdateMode)> _onStartCallback = nullptr;
  std::function<void(const UpdateProgress&)> _onProgressCallback = nullptr;
  std::function<void(bool)> _onEndCallback = nullptr;
  std::function<void(const String&)> _onErrorCallback = nullptr;

  void updateProgress() const;
  void notifyCallbacks() const;
  void setError(const String& error);
  String getUpdateErrorString() const;
  void scheduleReboot();
};

#endif  // NOGASM_UPDATE_H
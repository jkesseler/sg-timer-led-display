#pragma once

#include "BaseTimerDevice.h"
#include "Logger.h"

/**
 * @brief Shared base for timers that speak the F8/F9 frame protocol.
 *
 * The Special Pie M1A2 (name-pattern and UUID variants) and the ASN Tracker all
 * use the same wire format:
 *
 *   [F8 F9] [MESSAGE_TYPE] [DATA...] [F9 F8]
 *
 * with shot times reported as seconds + centiseconds. This base implements the
 * complete parse/normalize step in processTimerData(); concrete devices only
 * provide discovery (matchesDevice), connection setup (attemptConnection), and
 * their service/characteristic UUIDs.
 *
 * Times placed in NormalizedShotData are converted to milliseconds, per the
 * project-wide invariant (centiseconds * 10 = milliseconds).
 */
class FrameProtocolTimerDevice : public BaseTimerDevice {
protected:
  // Frame protocol message types (identical across all frame-based devices).
  enum class FrameMessageType : uint8_t {
    SESSION_STOP  = 0x18,   // 24
    SESSION_START = 0x34,   // 52
    SHOT_DETECTED = 0x36    // 54
  };

  // Component tag used for this device's log lines.
  const char* logTag;

  // BLE notify characteristic (non-owning - owned by the BLE service/client).
  BLERemoteCharacteristic* pNotifyCharacteristic;

  // Shot tracking for split-time calculation.
  uint32_t previousTimeSeconds;
  uint32_t previousTimeCentiseconds;
  bool hasPreviousShot;
  uint8_t currentSessionId;
  bool sessionActiveFlag;

  // First shot number this device puts on the wire (0 or 1). Shot numbers are
  // normalized to 1-based by subtracting this base and adding 1, so a device
  // that starts counting at 1 is passed through unchanged. Supplied by the
  // concrete device via its SHOT_INDEX_BASE constant.
  uint8_t shotIndexBase;

  FrameProtocolTimerDevice(const char* model, const char* tag, uint8_t indexBase)
    : BaseTimerDevice(model),
      logTag(tag),
      pNotifyCharacteristic(nullptr),
      previousTimeSeconds(0),
      previousTimeCentiseconds(0),
      hasPreviousShot(false),
      currentSessionId(0),
      sessionActiveFlag(false),
      shotIndexBase(indexBase) {}

  // Parse one notification frame and fire the matching normalized callback.
  void processTimerData(uint8_t* pData, size_t length) {
    if (!pData || length == 0) {
      LOG_WARN(logTag, "Invalid data received (null or empty)");
      return;
    }

    logNotificationBytes(logTag, pData, length);

    // Validate frame markers: [F8 F9] ... [F9 F8]
    if (length < 6 || pData[0] != 0xF8 || pData[1] != 0xF9 ||
        pData[length - 2] != 0xF9 || pData[length - 1] != 0xF8) {
      LOG_WARN(logTag, "Invalid frame markers");
      return;
    }

    switch (static_cast<FrameMessageType>(pData[2])) {
      case FrameMessageType::SESSION_START:
        handleSessionStart(pData[3]);
        break;

      case FrameMessageType::SESSION_STOP:
        handleSessionStop(pData[3]);
        break;

      case FrameMessageType::SHOT_DETECTED:
        // Format: F8 F9 36 00 [SEC] [CS] [SHOT#] [chk?] F9 F8
        if (length >= 10) {
          handleShotDetected(pData[4], pData[5], pData[6]);
        }
        break;

      default:
        LOG_WARN(logTag, "Unknown message type: 0x%02X", pData[2]);
        break;
    }
  }

private:
  void handleSessionStart(uint8_t sessionId) {
    currentSessionId = sessionId;
    LOG_TIMER("SESSION_START - ID: 0x%02X", currentSessionId);

    currentSession.sessionId = currentSessionId;
    currentSession.isActive = true;
    currentSession.totalShots = 0;
    currentSession.startTimestamp = millis();
    currentSession.startDelaySeconds = 0.0f;  // frame protocol has no start delay

    sessionActiveFlag = true;
    hasPreviousShot = false;
    previousTimeSeconds = 0;
    previousTimeCentiseconds = 0;

    if (sessionStartedCallback) sessionStartedCallback(currentSession);
    // No separate countdown phase - signal ready immediately.
    if (countdownCompleteCallback) countdownCompleteCallback(currentSession);
  }

  void handleSessionStop(uint8_t sessionId) {
    LOG_TIMER("SESSION_STOP - ID: 0x%02X", sessionId);

    currentSession.isActive = false;
    sessionActiveFlag = false;
    hasPreviousShot = false;

    if (sessionStoppedCallback) sessionStoppedCallback(currentSession);
  }

  void handleShotDetected(uint8_t seconds, uint8_t centiseconds, uint8_t shotNumber) {
    uint32_t absoluteTimeMs = (uint32_t)seconds * 1000 + (uint32_t)centiseconds * 10;

    uint32_t splitTimeMs = 0;
    bool isFirstShot = !hasPreviousShot;
    if (hasPreviousShot) {
      uint32_t previousTimeMs = previousTimeSeconds * 1000 + previousTimeCentiseconds * 10;
      splitTimeMs = absoluteTimeMs - previousTimeMs;
    }

    previousTimeSeconds = seconds;
    previousTimeCentiseconds = centiseconds;
    hasPreviousShot = true;

    // Normalize to 1-based regardless of where the device starts counting.
    uint16_t normalizedShotNumber = (uint16_t)(shotNumber - shotIndexBase + 1);

    LOG_DEBUG(logTag, "SHOT_DETECTED #%u (wire %u): %u.%02u (split %u ms)",
              normalizedShotNumber, shotNumber, seconds, centiseconds, splitTimeMs);

    currentSession.totalShots = normalizedShotNumber;

    if (shotDetectedCallback) {
      NormalizedShotData shotData;
      shotData.sessionId = currentSessionId;
      shotData.shotNumber = normalizedShotNumber;
      shotData.absoluteTimeMs = absoluteTimeMs;
      shotData.splitTimeMs = splitTimeMs;
      shotData.timestampMs = millis();
      strncpy(shotData.deviceModel, deviceModel, sizeof(shotData.deviceModel) - 1);
      shotData.deviceModel[sizeof(shotData.deviceModel) - 1] = '\0';
      shotData.isFirstShot = isFirstShot;
      shotDetectedCallback(shotData);
    }
  }
};

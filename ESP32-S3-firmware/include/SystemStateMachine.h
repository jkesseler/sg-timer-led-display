#pragma once

#include <Arduino.h>
#include "Logger.h"
#include "ITimerDevice.h"

// Define LOG_STATE macro if not already defined
#ifndef LOG_STATE
#define LOG_STATE(tag, format, ...) LOG_INFO(tag, format, ##__VA_ARGS__)
#endif

// System State Enumeration
enum class SystemState {
  // System States (Phase 1)
  STARTUP,
  MANUAL_RESET,

  // Connection States (Phase 1)
  SEARCHING_FOR_DEVICES,
  CONNECTING,
  CONNECTED,
  CONNECTION_ERROR,
  RECONNECTING,
  COMMUNICATION_ERROR,

  // Session States (Phase 1)
  IDLE,
  SESSION_STARTING,
  SESSION_ACTIVE,
  SHOT_DETECTED,
  SESSION_SUSPENDED,
  SESSION_ENDING,
  SESSION_ENDED,

  // Error States (Phase 1)
  DEVICE_ERROR,
  SYSTEM_ERROR,
  RECOVERY,

  // Maintenance States (Future Phases)
  CONFIGURATION,
  FIRMWARE_UPDATE,
  SLEEP
};

// State Context Structure
struct StateContext {
  SystemState currentState;
  SystemState previousState;
  unsigned long stateEnterTime;
  unsigned long stateTimeout;
  String stateData;  // State-specific data
  uint8_t retryCount;
  bool canTransition;

  StateContext() :
    currentState(SystemState::STARTUP),
    previousState(SystemState::STARTUP),
    stateEnterTime(0),
    stateTimeout(0),
    stateData(""),
    retryCount(0),
    canTransition(true) {}
};

// State Timeouts (in milliseconds)
namespace StateTimeouts {
  constexpr uint32_t STARTUP_TIMEOUT = 5000;           // 5 seconds
  constexpr uint32_t CONNECTING_TIMEOUT = 35000;       // 35 seconds (longer than BLE timeout)
  constexpr uint32_t CONNECTION_ERROR_TIMEOUT = 3000;  // 3 seconds
  constexpr uint32_t RECONNECTING_TIMEOUT = 5000;      // 5 seconds
  constexpr uint32_t COMMUNICATION_ERROR_TIMEOUT = 2000; // 2 seconds
  constexpr uint32_t SESSION_ENDED_TIMEOUT = 10000;    // 10 seconds
  constexpr uint32_t SHOT_DETECTED_TIMEOUT = 3000;     // 3 seconds
  constexpr uint32_t RECOVERY_TIMEOUT = 5000;          // 5 seconds
  constexpr uint32_t MANUAL_RESET_TIMEOUT = 100;       // Immediate
}

// Forward declarations
class TimerApplication;
class ITimerDevice;
class DisplayManager;

class SystemStateMachine {
private:
  StateContext context;
  TimerApplication* app;  // Reference to parent application

  // State timeout checking
  bool isStateTimedOut() const;
  void setTimeout(uint32_t timeoutMs);
  void clearTimeout();
  void handleStateTimeout();

  // State transition validation
  bool isValidTransition(SystemState from, SystemState to) const;
  void logStateTransition(SystemState from, SystemState to, const char* reason);

  // State entry/exit handlers
  void performStateEntry(SystemState state);
  void performStateExit(SystemState state);

  // State handlers (Phase 1 implementation)
  void handleStartup();
  void handleManualReset();
  void handleSearchingForDevices();
  void handleConnecting();
  void handleConnected();
  void handleConnectionError();
  void handleReconnecting();
  void handleCommunicationError();
  void handleIdle();
  void handleSessionStarting();
  void handleSessionActive();
  void handleShotDetected();
  void handleSessionSuspended();
  void handleSessionEnding();
  void handleSessionEnded();
  void handleDeviceError();
  void handleSystemError();
  void handleRecovery();

  // Utility methods
  const char* stateToString(SystemState state) const;
  void updateDisplayForState();
  void resetRetryCount();
  void incrementRetryCount();
  bool hasExceededMaxRetries() const;

  // Constants
  static constexpr uint8_t MAX_RETRY_COUNT = 15;

public:
  SystemStateMachine(TimerApplication* application);
  ~SystemStateMachine();

  // Core state machine interface
  void initialize();
  void update();

  // State control
  void forceTransition(SystemState newState, const char* reason = "Forced");
  bool requestTransition(SystemState newState, const char* reason = "Requested");

  // State queries
  SystemState getCurrentState() const { return context.currentState; }
  SystemState getPreviousState() const { return context.previousState; }
  bool isInErrorState() const;
  bool isInConnectionState() const;
  bool isInSessionState() const;
  bool canAcceptEvents() const;

  // External event triggers
  void onButtonPressed();
  void onConnectionStateChanged(DeviceConnectionState deviceState);
  void onSessionEvent(const char* eventType);
  void onError(const char* errorType);

  // System health
  unsigned long getTimeInCurrentState() const;
  String getStateInfo() const;
};
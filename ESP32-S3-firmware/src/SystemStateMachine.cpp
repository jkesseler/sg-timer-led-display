#include "SystemStateMachine.h"
#include "TimerApplication.h"
#include "ITimerDevice.h"
#include "DisplayManager.h"

SystemStateMachine::SystemStateMachine(TimerApplication* application)
  : app(application) {
  // Initialize context
  context.currentState = SystemState::STARTUP;
  context.previousState = SystemState::STARTUP;
  context.stateEnterTime = millis();
  context.stateTimeout = 0;
  context.retryCount = 0;
  context.canTransition = true;
}

SystemStateMachine::~SystemStateMachine() {
  // Cleanup if needed
}

void SystemStateMachine::initialize() {
  LOG_STATE("STATE", "State Machine initialized");
  forceTransition(SystemState::STARTUP, "Initialization");
}

void SystemStateMachine::update() {
  // Check for timeouts
  if (isStateTimedOut()) {
    handleStateTimeout();
  }

  // Execute current state handler
  switch (context.currentState) {
    case SystemState::STARTUP:
      handleStartup();
      break;
    case SystemState::MANUAL_RESET:
      handleManualReset();
      break;
    case SystemState::SEARCHING_FOR_DEVICES:
      handleSearchingForDevices();
      break;
    case SystemState::CONNECTING:
      handleConnecting();
      break;
    case SystemState::CONNECTED:
      handleConnected();
      break;
    case SystemState::CONNECTION_ERROR:
      handleConnectionError();
      break;
    case SystemState::RECONNECTING:
      handleReconnecting();
      break;
    case SystemState::COMMUNICATION_ERROR:
      handleCommunicationError();
      break;
    case SystemState::IDLE:
      handleIdle();
      break;
    case SystemState::SESSION_STARTING:
      handleSessionStarting();
      break;
    case SystemState::SESSION_ACTIVE:
      handleSessionActive();
      break;
    case SystemState::SHOT_DETECTED:
      handleShotDetected();
      break;
    case SystemState::SESSION_SUSPENDED:
      handleSessionSuspended();
      break;
    case SystemState::SESSION_ENDING:
      handleSessionEnding();
      break;
    case SystemState::SESSION_ENDED:
      handleSessionEnded();
      break;
    case SystemState::DEVICE_ERROR:
      handleDeviceError();
      break;
    case SystemState::SYSTEM_ERROR:
      handleSystemError();
      break;
    case SystemState::RECOVERY:
      handleRecovery();
      break;
    default:
      LOG_ERROR("STATE", "Unknown state: %d", (int)context.currentState);
      forceTransition(SystemState::SYSTEM_ERROR, "Unknown state");
      break;
  }
}

// State Handlers

void SystemStateMachine::handleStartup() {
  // Check if application is fully initialized
  if (app && app->isInitialized()) {
    requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Startup complete");
  } else if (isStateTimedOut()) {
    forceTransition(SystemState::SYSTEM_ERROR, "Startup timeout");
  }
}

void SystemStateMachine::handleManualReset() {
  // Immediate transition to searching - reset is handled in transition
  requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Manual reset complete");
}

void SystemStateMachine::handleSearchingForDevices() {
  // This state is handled by BLE scanning logic
  // Transition occurs via onConnectionStateChanged callback
}

void SystemStateMachine::handleConnecting() {
  // This state is handled by BLE connection logic
  // Transition occurs via onConnectionStateChanged callback
  if (isStateTimedOut()) {
    requestTransition(SystemState::CONNECTION_ERROR, "Connection timeout");
  }
}

void SystemStateMachine::handleConnected() {
  // Successfully connected, transition to idle
  requestTransition(SystemState::IDLE, "Connection established");
}

void SystemStateMachine::handleConnectionError() {
  if (isStateTimedOut()) {
    if (hasExceededMaxRetries()) {
      requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Max retries exceeded, restart search");
      resetRetryCount();
    } else {
      requestTransition(SystemState::RECONNECTING, "Retry connection");
      incrementRetryCount();
    }
  }
}

void SystemStateMachine::handleReconnecting() {
  // This state is handled by BLE reconnection logic
  if (isStateTimedOut()) {
    requestTransition(SystemState::CONNECTION_ERROR, "Reconnection timeout");
  }
}

void SystemStateMachine::handleCommunicationError() {
  if (isStateTimedOut()) {
    requestTransition(SystemState::RECONNECTING, "Communication error recovery");
  }
}

void SystemStateMachine::handleIdle() {
  // Waiting for session events
  // Transitions occur via onSessionEvent callback
}

void SystemStateMachine::handleSessionStarting() {
  // Session is starting, this will transition when session becomes active
  // Transition occurs via onSessionEvent callback
}

void SystemStateMachine::handleSessionActive() {
  // Active session, waiting for shots or session end
  // Transitions occur via shot detection or session end events
}

void SystemStateMachine::handleShotDetected() {
  if (isStateTimedOut()) {
    requestTransition(SystemState::SESSION_ACTIVE, "Shot display complete");
  }
}

void SystemStateMachine::handleSessionSuspended() {
  // Session is suspended, waiting for resume or end
  // Transitions occur via onSessionEvent callback
}

void SystemStateMachine::handleSessionEnding() {
  // Session is ending, transition to ended state
  requestTransition(SystemState::SESSION_ENDED, "Session end processing");
}

void SystemStateMachine::handleSessionEnded() {
  if (isStateTimedOut()) {
    requestTransition(SystemState::IDLE, "Session summary complete");
  }
}

void SystemStateMachine::handleDeviceError() {
  if (isStateTimedOut()) {
    requestTransition(SystemState::RECOVERY, "Device error recovery");
  }
}

void SystemStateMachine::handleSystemError() {
  // Critical error - only manual reset can recover
  LOG_ERROR("STATE", "System in error state - manual reset required");
}

void SystemStateMachine::handleRecovery() {
  if (isStateTimedOut()) {
    if (hasExceededMaxRetries()) {
      forceTransition(SystemState::SYSTEM_ERROR, "Recovery failed");
    } else {
      requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Recovery attempt");
      incrementRetryCount();
    }
  }
}

// State Transition Management

bool SystemStateMachine::requestTransition(SystemState newState, const char* reason) {
  if (!context.canTransition) {
    LOG_WARN("STATE", "Transition blocked: %s -> %s (%s)",
             stateToString(context.currentState), stateToString(newState), reason);
    return false;
  }

  if (!isValidTransition(context.currentState, newState)) {
    LOG_ERROR("STATE", "Invalid transition: %s -> %s (%s)",
              stateToString(context.currentState), stateToString(newState), reason);
    return false;
  }

  forceTransition(newState, reason);
  return true;
}

void SystemStateMachine::forceTransition(SystemState newState, const char* reason) {
  SystemState oldState = context.currentState;

  // Log transition
  logStateTransition(oldState, newState, reason);

  // Perform state-specific exit actions
  performStateExit(oldState);

  // Update context
  context.previousState = context.currentState;
  context.currentState = newState;
  context.stateEnterTime = millis();
  clearTimeout();

  // Perform state-specific entry actions
  performStateEntry(newState);

  // Update display
  updateDisplayForState();
}

void SystemStateMachine::performStateEntry(SystemState state) {
  switch (state) {
    case SystemState::STARTUP:
      setTimeout(StateTimeouts::STARTUP_TIMEOUT);
      break;
    case SystemState::MANUAL_RESET:
      setTimeout(StateTimeouts::MANUAL_RESET_TIMEOUT);
      resetRetryCount();
      break;
    case SystemState::CONNECTING:
      setTimeout(StateTimeouts::CONNECTING_TIMEOUT);
      break;
    case SystemState::CONNECTION_ERROR:
      setTimeout(StateTimeouts::CONNECTION_ERROR_TIMEOUT);
      break;
    case SystemState::RECONNECTING:
      setTimeout(StateTimeouts::RECONNECTING_TIMEOUT);
      break;
    case SystemState::COMMUNICATION_ERROR:
      setTimeout(StateTimeouts::COMMUNICATION_ERROR_TIMEOUT);
      break;
    case SystemState::SESSION_ENDED:
      setTimeout(StateTimeouts::SESSION_ENDED_TIMEOUT);
      break;
    case SystemState::SHOT_DETECTED:
      setTimeout(StateTimeouts::SHOT_DETECTED_TIMEOUT);
      break;
    case SystemState::RECOVERY:
      setTimeout(StateTimeouts::RECOVERY_TIMEOUT);
      break;
    default:
      // No specific entry actions or timeouts
      break;
  }
}

void SystemStateMachine::performStateExit(SystemState state) {
  // Perform any cleanup needed when exiting specific states
  switch (state) {
    case SystemState::MANUAL_RESET:
      // Reset application state
      if (app) {
        app->resetToInitialState();
      }
      break;
    default:
      // No specific exit actions
      break;
  }
}

// Event Handlers

void SystemStateMachine::onButtonPressed() {
  LOG_STATE("STATE", "Button pressed - manual reset triggered");
  forceTransition(SystemState::MANUAL_RESET, "Button pressed");
}

void SystemStateMachine::onConnectionStateChanged(DeviceConnectionState deviceState) {
  switch (deviceState) {
    case DeviceConnectionState::SCANNING:
      if (context.currentState != SystemState::SEARCHING_FOR_DEVICES) {
        requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Device scanning started");
      }
      break;
    case DeviceConnectionState::CONNECTING:
      requestTransition(SystemState::CONNECTING, "Device connection started");
      break;
    case DeviceConnectionState::CONNECTED:
      requestTransition(SystemState::CONNECTED, "Device connected");
      break;
    case DeviceConnectionState::DISCONNECTED:
      if (isInSessionState()) {
        requestTransition(SystemState::COMMUNICATION_ERROR, "Connection lost during session");
      } else {
        requestTransition(SystemState::CONNECTION_ERROR, "Connection lost");
      }
      break;
    case DeviceConnectionState::ERROR:
      requestTransition(SystemState::CONNECTION_ERROR, "Device connection error");
      break;
  }
}

void SystemStateMachine::onSessionEvent(const char* eventType) {
  if (strcmp(eventType, "started") == 0) {
    requestTransition(SystemState::SESSION_STARTING, "Session started");
  } else if (strcmp(eventType, "active") == 0) {
    requestTransition(SystemState::SESSION_ACTIVE, "Session active");
  } else if (strcmp(eventType, "shot") == 0) {
    requestTransition(SystemState::SHOT_DETECTED, "Shot detected");
  } else if (strcmp(eventType, "suspended") == 0) {
    requestTransition(SystemState::SESSION_SUSPENDED, "Session suspended");
  } else if (strcmp(eventType, "resumed") == 0) {
    requestTransition(SystemState::SESSION_ACTIVE, "Session resumed");
  } else if (strcmp(eventType, "ending") == 0) {
    requestTransition(SystemState::SESSION_ENDING, "Session ending");
  } else if (strcmp(eventType, "ended") == 0) {
    requestTransition(SystemState::SESSION_ENDED, "Session ended");
  }
}

void SystemStateMachine::onError(const char* errorType) {
  if (strcmp(errorType, "device") == 0) {
    requestTransition(SystemState::DEVICE_ERROR, "Device error");
  } else if (strcmp(errorType, "communication") == 0) {
    requestTransition(SystemState::COMMUNICATION_ERROR, "Communication error");
  } else if (strcmp(errorType, "system") == 0) {
    forceTransition(SystemState::SYSTEM_ERROR, "System error");
  }
}

// Utility Methods

void SystemStateMachine::handleStateTimeout() {
  LOG_WARN("STATE", "State timeout in %s after %lu ms",
           stateToString(context.currentState),
           millis() - context.stateEnterTime);

  // Handle specific timeout behaviors
  switch (context.currentState) {
    case SystemState::STARTUP:
      forceTransition(SystemState::SYSTEM_ERROR, "Startup timeout");
      break;
    case SystemState::CONNECTING:
      requestTransition(SystemState::CONNECTION_ERROR, "Connection timeout");
      break;
    case SystemState::CONNECTION_ERROR:
      if (hasExceededMaxRetries()) {
        requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Max retries exceeded");
        resetRetryCount();
      } else {
        requestTransition(SystemState::RECONNECTING, "Retry connection");
        incrementRetryCount();
      }
      break;
    case SystemState::RECONNECTING:
      requestTransition(SystemState::CONNECTION_ERROR, "Reconnection timeout");
      break;
    case SystemState::COMMUNICATION_ERROR:
      requestTransition(SystemState::RECONNECTING, "Communication error recovery");
      break;
    case SystemState::SESSION_ENDED:
      requestTransition(SystemState::IDLE, "Session summary complete");
      break;
    case SystemState::SHOT_DETECTED:
      requestTransition(SystemState::SESSION_ACTIVE, "Shot display complete");
      break;
    case SystemState::RECOVERY:
      if (hasExceededMaxRetries()) {
        forceTransition(SystemState::SYSTEM_ERROR, "Recovery failed");
      } else {
        requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Recovery attempt");
        incrementRetryCount();
      }
      break;
    case SystemState::MANUAL_RESET:
      requestTransition(SystemState::SEARCHING_FOR_DEVICES, "Manual reset complete");
      break;
    default:
      LOG_WARN("STATE", "Unhandled timeout in state %s", stateToString(context.currentState));
      break;
  }
}

bool SystemStateMachine::isStateTimedOut() const {
  if (context.stateTimeout == 0) {
    return false; // No timeout set
  }
  return (millis() - context.stateEnterTime) >= context.stateTimeout;
}

void SystemStateMachine::setTimeout(uint32_t timeoutMs) {
  context.stateTimeout = timeoutMs;
}

void SystemStateMachine::clearTimeout() {
  context.stateTimeout = 0;
}

bool SystemStateMachine::isValidTransition(SystemState from, SystemState to) const {
  // Universal transitions (valid from any state)
  if (to == SystemState::MANUAL_RESET || to == SystemState::SYSTEM_ERROR) {
    return true;
  }

  // State-specific transitions
  switch (from) {
    case SystemState::STARTUP:
      return (to == SystemState::SEARCHING_FOR_DEVICES);

    case SystemState::MANUAL_RESET:
      return (to == SystemState::SEARCHING_FOR_DEVICES);

    case SystemState::SEARCHING_FOR_DEVICES:
      return (to == SystemState::CONNECTING || to == SystemState::CONNECTION_ERROR);

    case SystemState::CONNECTING:
      return (to == SystemState::CONNECTED || to == SystemState::CONNECTION_ERROR);

    case SystemState::CONNECTED:
      return (to == SystemState::IDLE || to == SystemState::COMMUNICATION_ERROR);

    case SystemState::CONNECTION_ERROR:
      return (to == SystemState::RECONNECTING || to == SystemState::SEARCHING_FOR_DEVICES);

    case SystemState::RECONNECTING:
      return (to == SystemState::CONNECTED || to == SystemState::CONNECTION_ERROR);

    case SystemState::COMMUNICATION_ERROR:
      return (to == SystemState::RECONNECTING || to == SystemState::SEARCHING_FOR_DEVICES);

    case SystemState::IDLE:
      return (to == SystemState::SESSION_STARTING || to == SystemState::COMMUNICATION_ERROR);

    case SystemState::SESSION_STARTING:
      return (to == SystemState::SESSION_ACTIVE || to == SystemState::IDLE);

    case SystemState::SESSION_ACTIVE:
      return (to == SystemState::SHOT_DETECTED || to == SystemState::SESSION_SUSPENDED ||
              to == SystemState::SESSION_ENDING || to == SystemState::COMMUNICATION_ERROR);

    case SystemState::SHOT_DETECTED:
      return (to == SystemState::SESSION_ACTIVE || to == SystemState::SESSION_ENDING);

    case SystemState::SESSION_SUSPENDED:
      return (to == SystemState::SESSION_ACTIVE || to == SystemState::SESSION_ENDING);

    case SystemState::SESSION_ENDING:
      return (to == SystemState::SESSION_ENDED);

    case SystemState::SESSION_ENDED:
      return (to == SystemState::IDLE || to == SystemState::SESSION_STARTING);

    case SystemState::DEVICE_ERROR:
      return (to == SystemState::RECOVERY || to == SystemState::SYSTEM_ERROR);

    case SystemState::RECOVERY:
      return (to == SystemState::SEARCHING_FOR_DEVICES || to == SystemState::SYSTEM_ERROR);

    case SystemState::SYSTEM_ERROR:
      return false; // Only manual reset can exit system error

    default:
      return false;
  }
}

void SystemStateMachine::logStateTransition(SystemState from, SystemState to, const char* reason) {
  LOG_STATE("STATE", "State: %s -> %s (%s)", stateToString(from), stateToString(to), reason);
}

const char* SystemStateMachine::stateToString(SystemState state) const {
  switch (state) {
    case SystemState::STARTUP: return "STARTUP";
    case SystemState::MANUAL_RESET: return "MANUAL_RESET";
    case SystemState::SEARCHING_FOR_DEVICES: return "SEARCHING_FOR_DEVICES";
    case SystemState::CONNECTING: return "CONNECTING";
    case SystemState::CONNECTED: return "CONNECTED";
    case SystemState::CONNECTION_ERROR: return "CONNECTION_ERROR";
    case SystemState::RECONNECTING: return "RECONNECTING";
    case SystemState::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
    case SystemState::IDLE: return "IDLE";
    case SystemState::SESSION_STARTING: return "SESSION_STARTING";
    case SystemState::SESSION_ACTIVE: return "SESSION_ACTIVE";
    case SystemState::SHOT_DETECTED: return "SHOT_DETECTED";
    case SystemState::SESSION_SUSPENDED: return "SESSION_SUSPENDED";
    case SystemState::SESSION_ENDING: return "SESSION_ENDING";
    case SystemState::SESSION_ENDED: return "SESSION_ENDED";
    case SystemState::DEVICE_ERROR: return "DEVICE_ERROR";
    case SystemState::SYSTEM_ERROR: return "SYSTEM_ERROR";
    case SystemState::RECOVERY: return "RECOVERY";
    case SystemState::CONFIGURATION: return "CONFIGURATION";
    case SystemState::FIRMWARE_UPDATE: return "FIRMWARE_UPDATE";
    case SystemState::SLEEP: return "SLEEP";
    default: return "UNKNOWN";
  }
}

void SystemStateMachine::updateDisplayForState() {
  if (!app || !app->getDisplayManager()) {
    return;
  }

  DisplayManager* display = app->getDisplayManager();

  // Map system states to display actions
  switch (context.currentState) {
    case SystemState::STARTUP:
      display->showStartup();
      break;
    case SystemState::SEARCHING_FOR_DEVICES:
      display->showConnectionState(DeviceConnectionState::SCANNING);
      break;
    case SystemState::CONNECTING:
      display->showConnectionState(DeviceConnectionState::CONNECTING);
      break;
    case SystemState::CONNECTED:
    case SystemState::IDLE:
      display->showConnectionState(DeviceConnectionState::CONNECTED);
      break;
    case SystemState::CONNECTION_ERROR:
    case SystemState::COMMUNICATION_ERROR:
      display->showConnectionState(DeviceConnectionState::ERROR);
      break;
    case SystemState::SYSTEM_ERROR:
      // TODO: Add showSystemError() method to DisplayManager
      LOG_ERROR("DISPLAY", "System Error state - display method needed");
      break;
    default:
      // Other states will be handled by session-specific display updates
      break;
  }
}

void SystemStateMachine::resetRetryCount() {
  context.retryCount = 0;
}

void SystemStateMachine::incrementRetryCount() {
  context.retryCount++;
}

bool SystemStateMachine::hasExceededMaxRetries() const {
  return context.retryCount >= MAX_RETRY_COUNT;
}

// State Query Methods

bool SystemStateMachine::isInErrorState() const {
  return (context.currentState == SystemState::CONNECTION_ERROR ||
          context.currentState == SystemState::COMMUNICATION_ERROR ||
          context.currentState == SystemState::DEVICE_ERROR ||
          context.currentState == SystemState::SYSTEM_ERROR);
}

bool SystemStateMachine::isInConnectionState() const {
  return (context.currentState == SystemState::SEARCHING_FOR_DEVICES ||
          context.currentState == SystemState::CONNECTING ||
          context.currentState == SystemState::CONNECTED ||
          context.currentState == SystemState::RECONNECTING);
}

bool SystemStateMachine::isInSessionState() const {
  return (context.currentState == SystemState::SESSION_STARTING ||
          context.currentState == SystemState::SESSION_ACTIVE ||
          context.currentState == SystemState::SHOT_DETECTED ||
          context.currentState == SystemState::SESSION_SUSPENDED ||
          context.currentState == SystemState::SESSION_ENDING ||
          context.currentState == SystemState::SESSION_ENDED);
}

bool SystemStateMachine::canAcceptEvents() const {
  return context.canTransition &&
         context.currentState != SystemState::SYSTEM_ERROR;
}

unsigned long SystemStateMachine::getTimeInCurrentState() const {
  return millis() - context.stateEnterTime;
}

String SystemStateMachine::getStateInfo() const {
  return String(stateToString(context.currentState)) +
         " (" + String(getTimeInCurrentState()) + "ms)";
}
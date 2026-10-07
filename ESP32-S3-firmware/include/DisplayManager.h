#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ITimerDevice.h"
#include "common.h"

// Display states
enum class DisplayState {
  STARTUP,
  DISCONNECTED,
  SCANNING,
  CONNECTING,
  CONNECTED,
  COUNTDOWN,           // Showing countdown before session starts
  WAITING_FOR_SHOTS,
  SHOWING_SHOT,
  SESSION_ENDED
};

// Display colors (RGB565 format helpers)
struct DisplayColors {
  static const uint16_t RED;
  static const uint16_t GREEN;
  static const uint16_t BLUE;
  static const uint16_t YELLOW;
  static const uint16_t WHITE;
  static const uint16_t LIGHT_BLUE;
  static const uint16_t GRAY;
};

class DisplayManager {
private:
  MatrixPanel_I2S_DMA* display;
  DisplayState currentState;
  unsigned long lastUpdateTime;

  // Display data
  NormalizedShotData lastShotData;
  SessionData currentSessionData;
  DeviceConnectionState connectionState;
  // Owned copy of the device name. The source pointer belongs to the timer
  // device object, which is destroyed on disconnect, so we must not retain it.
  const char* deviceName;       // points at deviceNameStorage, or nullptr
  char deviceNameStorage[64];

  // Next / on-deck shooters, alternated with the session-end screen
  static constexpr size_t SHOOTER_NAME_SIZE = 48;
  char upNextName[SHOOTER_NAME_SIZE];
  char onDeckName[SHOOTER_NAME_SIZE];
  bool hasUpNext;
  bool showingUpNext;
  unsigned long lastUpNextToggle;
  static const uint16_t UP_NEXT_TOGGLE_MS = 10000;

  // Countdown tracking
  unsigned long countdownStartTime;
  float countdownDurationSeconds;

  // Dirty flag pattern - signals when display needs update
  bool displayDirty;
  bool needsClear;  // Signals that display should be cleared before next render

  // Cached display values for renderShotData to detect changes
  uint16_t cachedShotNumber;
  uint32_t cachedAbsoluteTimeMs;
  uint32_t cachedSplitTimeMs;

  // Marquee scrolling state (for device name in CONNECTED state)
  int16_t scrollOffset;
  unsigned long lastScrollUpdate;
  int16_t textPixelWidth;

  // Marquee scrolling state for startup message
  int16_t startupScrollOffset;
  unsigned long startupLastScrollUpdate;
  int16_t startupTextPixelWidth;

  static const uint16_t SCROLL_SPEED_MS = 25;  // Update scroll every 25ms
  static const uint16_t SCROLL_PAUSE_MS = 1000; // Pause at start/end

  // Signal that display needs to be redrawn
  void markDirty(bool clearFirst = true);

  // Internal display methods
  void renderStartupMessage();
  void renderConnectionStatus();
  void renderCountdown();
  void renderWaitingForShots();
  void renderShotData();
  void renderSessionEnd();
  void renderUpNext();
  void clearDisplay();
  void clearConnectionDetailLine();

  // Helper methods
  static void formatTime(uint32_t timeMs, char* buffer, size_t bufferSize);
  static void formatSplitTime(uint32_t timeMs, char* buffer, size_t bufferSize);
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const;

public:
  DisplayManager();
  ~DisplayManager();

  bool initialize();
  void update();

  // State updates
  void showStartup();
  void showConnectionState(DeviceConnectionState state, const char* deviceName = nullptr);
  void showCountdown(const SessionData& sessionData);
  void showWaitingForShots(const SessionData& sessionData);
  void showShotData(const NormalizedShotData& shotData);
  void showSessionEnd(const SessionData& sessionData, uint16_t lastShotNumber);
  // Ignored unless sessionId is the session currently shown as ended.
  // onDeck may be empty.
  void showUpNext(uint32_t sessionId, const char* next, const char* onDeck);

  // Getters
  DisplayState getCurrentState() const { return currentState; }
  bool isInitialized() const { return display != nullptr; }
};
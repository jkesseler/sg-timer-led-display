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
  const char* deviceName;

  // Marquee scrolling state
  int16_t scrollOffset;
  unsigned long lastScrollUpdate;
  int16_t textPixelWidth;
  static const uint16_t SCROLL_SPEED_MS = 25;  // Update scroll every 50ms
  static const uint16_t SCROLL_PAUSE_MS = 1000; // Pause at start/end

  // Internal display methods
  void renderStartupMessage();
  void renderConnectionStatus();
  void renderWaitingForShots();
  void renderShotData();
  void renderSessionEnd();
  void clearDisplay();

  // Helper methods
  void formatTime(uint32_t timeMs, char* buffer, size_t bufferSize);
  void formatSplitTime(uint32_t timeMs, char* buffer, size_t bufferSize);
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

public:
  DisplayManager();
  ~DisplayManager();

  bool initialize();
  void update();

  // State updates
  void showStartup();
  void showConnectionState(DeviceConnectionState state, const char* deviceName = nullptr);
  void showWaitingForShots(const SessionData& sessionData);
  void showShotData(const NormalizedShotData& shotData);
  void showSessionEnd(const SessionData& sessionData, uint16_t lastShotNumber);

  // Getters
  DisplayState getCurrentState() const { return currentState; }
  bool isInitialized() const { return display != nullptr; }
};
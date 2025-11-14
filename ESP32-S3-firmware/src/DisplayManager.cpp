#include "DisplayManager.h"
#include "Logger.h"
#include <stdio.h>

// Define color constants
const uint16_t DisplayColors::RED = 0xF800;        // 255, 0, 0
const uint16_t DisplayColors::GREEN = 0x07E0;      // 0, 255, 0
const uint16_t DisplayColors::BLUE = 0x001F;       // 0, 0, 255
const uint16_t DisplayColors::YELLOW = 0xFFE0;     // 255, 255, 0
const uint16_t DisplayColors::WHITE = 0xFFFF;      // 255, 255, 255
const uint16_t DisplayColors::LIGHT_BLUE = 0x647F; // 100, 100, 255
const uint16_t DisplayColors::GRAY = 0x8410;       // 128, 128, 128

DisplayManager::DisplayManager()
  : display(nullptr),
    currentState(DisplayState::STARTUP),
    lastUpdateTime(0),
    connectionState(DeviceConnectionState::DISCONNECTED),
    deviceName(nullptr),
    scrollOffset(0),
    lastScrollUpdate(0),
    textPixelWidth(0) {
  // Initialize data structures
  memset(&lastShotData, 0, sizeof(lastShotData));
  memset(&currentSessionData, 0, sizeof(currentSessionData));
}

DisplayManager::~DisplayManager() {
  if (display) {
    delete display;
    display = nullptr;
  }
}

bool DisplayManager::initialize() {
  LOG_DISPLAY("Initializing HUB75 LED panels...");

  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,      // Width of one panel
    PANEL_HEIGHT,     // Height of panel
    PANEL_CHAIN       // Number of panels chained
  );

  // Configure for ESP32-S3
  mxconfig.gpio.e = 18;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Uncomment if needed for specific panels
  // mxconfig.clkphase = false;
  mxconfig.latch_blanking = 2;
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

  display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!display) {
    LOG_ERROR("DISPLAY", "Failed to create display instance");
    return false;
  }

  display->begin();
  clearDisplay();

  showStartup();
  LOG_DISPLAY("HUB75 panels initialized successfully");
  return true;
}

void DisplayManager::update() {
  // Update display based on current state
  unsigned long currentTime = millis();

  switch (currentState) {
    case DisplayState::STARTUP:
      // Auto-transition after startup delay
      if (currentTime - lastUpdateTime > STARTUP_MESSAGE_DELAY) {
        showConnectionState(connectionState, deviceName);
      }
      break;

    case DisplayState::DISCONNECTED:
    case DisplayState::SCANNING:
    case DisplayState::CONNECTING:
    case DisplayState::CONNECTED:
      renderConnectionStatus();
      break;

    case DisplayState::WAITING_FOR_SHOTS:
      renderWaitingForShots();
      break;

    case DisplayState::SHOWING_SHOT:
      renderShotData();
      break;

    case DisplayState::SESSION_ENDED:
      renderSessionEnd();
      break;
  }
}

void DisplayManager::showStartup() {
  currentState = DisplayState::STARTUP;
  lastUpdateTime = millis();
  renderStartupMessage();
}

void DisplayManager::showConnectionState(DeviceConnectionState state, const char* name) {
  connectionState = state;
  deviceName = name;

  // Reset scroll when state changes
  scrollOffset = 0;
  lastScrollUpdate = millis();
  textPixelWidth = 0;

  switch (state) {
    case DeviceConnectionState::DISCONNECTED:
      currentState = DisplayState::DISCONNECTED;
      break;
    case DeviceConnectionState::SCANNING:
      currentState = DisplayState::SCANNING;
      break;
    case DeviceConnectionState::CONNECTING:
      currentState = DisplayState::CONNECTING;
      break;
    case DeviceConnectionState::CONNECTED:
      currentState = DisplayState::CONNECTED;
      break;
    case DeviceConnectionState::ERROR:
      currentState = DisplayState::DISCONNECTED; // Treat error as disconnected
      break;
  }

  lastUpdateTime = millis();
  renderConnectionStatus();
}

void DisplayManager::showWaitingForShots(const SessionData& sessionData) {
  currentState = DisplayState::WAITING_FOR_SHOTS;
  currentSessionData = sessionData;
  lastUpdateTime = millis();
  renderWaitingForShots();
}

void DisplayManager::showShotData(const NormalizedShotData& shotData) {
  currentState = DisplayState::SHOWING_SHOT;
  lastShotData = shotData;
  lastUpdateTime = millis();
  renderShotData();
}

void DisplayManager::showSessionEnd(const SessionData& sessionData, uint16_t lastShotNumber) {
  currentState = DisplayState::SESSION_ENDED;
  currentSessionData = sessionData;
  lastShotData.shotNumber = lastShotNumber; // Store for display
  lastUpdateTime = millis();
  renderSessionEnd();
}

void DisplayManager::clearDisplay() {
  if (display) {
    display->clearScreen();
    // display->fillScreen(0); // Black background
  }
}

uint16_t DisplayManager::color565(uint8_t r, uint8_t g, uint8_t b) {
  if (display) {
    return display->color565(r, g, b);
  }
  return 0;
}

void DisplayManager::renderStartupMessage() {
  if (!display) return;

  clearDisplay();
  display->setTextSize(2);
  display->setTextColor(DisplayColors::GREEN);
  display->setCursor(2, 0);
  display->print("TIMER");
  display->setCursor(2, 16);
  display->print("READY");
}

void DisplayManager::renderConnectionStatus() {
  if (!display) return;

  clearDisplay();

  // First line - status message
  display->setTextSize(2);
  display->setCursor(2, 2);
  display->setTextWrap(false);

  const char* statusText = nullptr;
  uint16_t statusColor = DisplayColors::WHITE;

  switch (connectionState) {
    case DeviceConnectionState::DISCONNECTED:
      statusColor = DisplayColors::RED;
      statusText = "DISCONNECTED";
      break;
    case DeviceConnectionState::SCANNING:
      statusColor = DisplayColors::YELLOW;
      statusText = "SCANNING...";
      break;
    case DeviceConnectionState::CONNECTING:
      statusColor = DisplayColors::BLUE;
      statusText = "CONNECTING...";
      break;
    case DeviceConnectionState::CONNECTED:
      statusColor = DisplayColors::GREEN;
      statusText = "CONNECTED";
      break;
    case DeviceConnectionState::ERROR:
      statusColor = DisplayColors::RED;
      statusText = "ERROR";
      break;
  }

  display->setTextColor(statusColor);
  display->print(statusText);

  // Second line - device name with marquee scrolling
  if (deviceName && connectionState == DeviceConnectionState::CONNECTED) {
    display->setTextSize(2);
    display->setTextColor(DisplayColors::WHITE);

    // Calculate text width if not already done
    if (textPixelWidth == 0) {
      textPixelWidth = strlen(deviceName) * 12;
    }

    const int16_t displayWidth = PANEL_WIDTH * PANEL_CHAIN; // 128 pixels
    const int16_t lineY = 16;

    // If text fits on screen, just display it normally
    if (textPixelWidth <= displayWidth - 8) {
      display->setCursor(2, lineY);
      display->print(deviceName);
    } else {
      // Text is too long - implement marquee scrolling
      unsigned long currentTime = millis();

      // Update scroll position based on time
      if (currentTime - lastScrollUpdate >= SCROLL_SPEED_MS) {
        scrollOffset++;

        // Reset when text has scrolled completely off screen
        if (scrollOffset > textPixelWidth + 60) { // +60 for gap before repeat
          scrollOffset = -displayWidth; // Start from right edge
        }

        lastScrollUpdate = currentTime;
      }

      // Draw text at scrolled position
      int16_t xPos = displayWidth - scrollOffset;
      display->setCursor(xPos, lineY);
      display->print(deviceName);

      // If we're near the end, draw another copy for seamless loop
      if (scrollOffset > textPixelWidth) {
        int16_t xPos2 = xPos + textPixelWidth + 60; // +60 pixel gap
        display->setCursor(xPos2, lineY);
        display->print(deviceName);
      }
    }
  }
}

void DisplayManager::renderWaitingForShots() {
  if (!display) return;

  clearDisplay();
  display->setTextSize(1);
  display->setTextColor(DisplayColors::LIGHT_BLUE);
  display->setCursor(10, 2);
  display->print("READY");

  display->setTextSize(3);
  display->setTextColor(DisplayColors::WHITE);
  display->setCursor(10, 18);
  display->print("00:00");
}

void DisplayManager::renderShotData() {
  if (!display) return;

  clearDisplay();

  // Display time (bottom section)
  char timeBuffer[16];
  formatTime(lastShotData.absoluteTimeMs, timeBuffer, sizeof(timeBuffer));

  // Display shot number (top section)
  display->setTextSize(1);
  display->setTextColor(DisplayColors::YELLOW);
  display->setCursor(2, 2);
  display->printf("#:%d ", lastShotData.shotNumber);

  display->setTextSize(3);
  display->setTextColor(DisplayColors::GREEN);
  display->print(timeBuffer);
}

void DisplayManager::renderSessionEnd() {
  if (!display) return;

  clearDisplay();

  // Display "SESSION END"
  display->setTextSize(1);
  display->setTextColor(DisplayColors::RED);
  display->setCursor(2, 2);
  display->print("SESSION END");

  // // Display total shots
  // display->setTextSize(2);
  // display->setTextColor(DisplayColors::YELLOW);
  // display->setCursor(5, 14);
  // display->printf("Shots: %d", currentSessionData.totalShots);

  // Display last shot number if available
  if (lastShotData.shotNumber > 0) {
    display->setTextSize(3);
    display->setTextColor(DisplayColors::GREEN);
    display->setCursor(5, 16);
    display->printf("Last: #%d", lastShotData.shotNumber);
  }
}

void DisplayManager::formatTime(uint32_t timeMs, char* buffer, size_t bufferSize) {
  uint32_t totalSeconds = timeMs / 1000;
  uint32_t minutes = totalSeconds / 60;
  uint32_t seconds = totalSeconds % 60;
  uint32_t milliseconds = timeMs % 1000;

  if (minutes > 0) {
    snprintf(buffer, bufferSize, "%02lu:%02lu.%01lu",
             minutes, seconds, milliseconds / 100);
  } else {
    snprintf(buffer, bufferSize, "%02lu.%03lu", seconds, milliseconds);
  }
}
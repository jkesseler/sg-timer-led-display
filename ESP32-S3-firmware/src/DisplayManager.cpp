#include "DisplayManager.h"
#include "Logger.h"
#include <stdio.h>
#include <U8g2_for_Adafruit_GFX.h>

U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

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
    displayDirty(true),
    needsClear(true),
    cachedShotNumber(0xFFFF),
    cachedAbsoluteTimeMs(0xFFFFFFFF),
    cachedSplitTimeMs(0xFFFFFFFF),
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

  // Configure for ESP32-S3 to reduce scanlines
  mxconfig.gpio.e = 18;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Increase color depth for smoother PWM and reduce visible scanlines
  mxconfig.latch_blanking = 4;           // Higher blanking reduces ghosting
  mxconfig.clkphase = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;  // Higher clock speed for better refresh

  // Optimize for better refresh rate and color depth
  mxconfig.min_refresh_rate = 120;       // Higher refresh rate reduces flicker/scanlines

  display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!display) {
    LOG_ERROR("DISPLAY", "Failed to create display instance");
    return false;
  }

  display->begin();

  display->setBrightness8(200); // Slightly lower brightness helps reduce scanlines
  display->clearScreen();
  display->setTextWrap(false);
  u8g2_for_adafruit_gfx.begin(*display);

  showStartup();
  LOG_DISPLAY("HUB75 panels initialized successfully");
  return true;
}

void DisplayManager::markDirty(bool clearFirst) {
  displayDirty = true;
  needsClear = clearFirst;
}

void DisplayManager::update() {
  // Update display based on current state
  unsigned long currentTime = millis();

  switch (currentState) {
    case DisplayState::STARTUP:
      // Render startup message if dirty
      if (displayDirty) {
        if (needsClear) {
          clearDisplay();
        }
        renderStartupMessage();
        displayDirty = false;
        needsClear = false;
      }

      // Auto-transition after startup delay
      if (currentTime - lastUpdateTime > STARTUP_MESSAGE_DELAY) {
        showConnectionState(connectionState, deviceName);
      }
      break;

    case DisplayState::DISCONNECTED:
    case DisplayState::SCANNING:
    case DisplayState::CONNECTING:
    case DisplayState::CONNECTED:
      // Update marquee scroll position if needed (only in CONNECTED state)
      if (currentState == DisplayState::CONNECTED && deviceName && textPixelWidth > (PANEL_WIDTH * PANEL_CHAIN) - 8) {
        // Text is too long and needs scrolling
        if (currentTime - lastScrollUpdate >= SCROLL_SPEED_MS) {
          scrollOffset++;

          const int16_t displayWidth = PANEL_WIDTH * PANEL_CHAIN;
          // Reset when text has scrolled completely off screen
          if (scrollOffset > textPixelWidth + 60) { // +60 for gap before repeat
            scrollOffset = -displayWidth; // Start from right edge
          }

          lastScrollUpdate = currentTime;
          markDirty(false); // Mark dirty without clearing to update animation
        }
      }

      // Only render if display is dirty
      if (displayDirty) {
        if (needsClear) {
          clearDisplay();
        }
        renderConnectionStatus();
        displayDirty = false;
        needsClear = false;
      }
      break;

    case DisplayState::WAITING_FOR_SHOTS:
      if (displayDirty) {
        if (needsClear) {
          clearDisplay();
        }
        renderWaitingForShots();
        displayDirty = false;
        needsClear = false;
      }
      break;

    case DisplayState::SHOWING_SHOT:
      if (displayDirty) {
        if (needsClear) {
          clearDisplay();
        }
        renderShotData();
        displayDirty = false;
        needsClear = false;
      }
      break;

    case DisplayState::SESSION_ENDED:
      if (displayDirty) {
        if (needsClear) {
          clearDisplay();
        }
        renderSessionEnd();
        displayDirty = false;
        needsClear = false;
      }
      break;
  }
}

void DisplayManager::showStartup() {
  currentState = DisplayState::STARTUP;
  lastUpdateTime = millis();
  markDirty(true);  // Signal display update needed with clear
}

void DisplayManager::showConnectionState(DeviceConnectionState state, const char* name) {
  connectionState = state;
  deviceName = name;

  // Reset scroll when state changes
  scrollOffset = 0;
  lastScrollUpdate = millis();
  
  // Calculate text width immediately when device name is set
  // This ensures scrolling logic in update() works from the start
  if (deviceName && state == DeviceConnectionState::CONNECTED) {
    textPixelWidth = strlen(deviceName) * 12;
  } else {
    textPixelWidth = 0;
  }

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
  markDirty(true);  // Signal display update needed with clear
}

void DisplayManager::showWaitingForShots(const SessionData& sessionData) {
  currentState = DisplayState::WAITING_FOR_SHOTS;
  currentSessionData = sessionData;
  lastUpdateTime = millis();
  markDirty(true);  // Signal display update needed with clear
}

void DisplayManager::showShotData(const NormalizedShotData& shotData) {
  // Check if data actually changed
  bool dataChanged = (cachedShotNumber != shotData.shotNumber) ||
                     (cachedAbsoluteTimeMs != shotData.absoluteTimeMs) ||
                     (cachedSplitTimeMs != shotData.splitTimeMs);

  // Only update if state is different or data changed
  if (currentState != DisplayState::SHOWING_SHOT || dataChanged) {
    currentState = DisplayState::SHOWING_SHOT;
    lastShotData = shotData;
    lastUpdateTime = millis();

    // Update cached values
    cachedShotNumber = shotData.shotNumber;
    cachedAbsoluteTimeMs = shotData.absoluteTimeMs;
    cachedSplitTimeMs = shotData.splitTimeMs;

    markDirty(true);  // Signal display update needed with clear
  }
}

void DisplayManager::showSessionEnd(const SessionData& sessionData, uint16_t lastShotNumber) {
  currentState = DisplayState::SESSION_ENDED;
  currentSessionData = sessionData;
  lastShotData.shotNumber = lastShotNumber; // Store for display
  lastUpdateTime = millis();
  markDirty(true);  // Signal display update needed with clear
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

  u8g2_for_adafruit_gfx.setFontMode(1);
  u8g2_for_adafruit_gfx.setFontDirection(0);
  u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::GREEN);
  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvR10_tf);
  u8g2_for_adafruit_gfx.setCursor(0, 12);
  u8g2_for_adafruit_gfx.print(F("Timer Ready"));

}

void DisplayManager::renderConnectionStatus() {
  if (!display) return;

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

  u8g2_for_adafruit_gfx.setFontMode(1);
  u8g2_for_adafruit_gfx.setFontDirection(0);
  u8g2_for_adafruit_gfx.setForegroundColor(statusColor);
  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvR10_tf);
  u8g2_for_adafruit_gfx.setCursor(0, 12);
  u8g2_for_adafruit_gfx.print(statusText);

  // Second line - device name with marquee scrolling
  if (deviceName && connectionState == DeviceConnectionState::CONNECTED) {
    u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::WHITE);

    const int16_t displayWidth = PANEL_WIDTH * PANEL_CHAIN; // 128 pixels
    const int16_t lineY = 28;

    // If text fits on screen, just display it normally
    if (textPixelWidth <= displayWidth - 8) {
      u8g2_for_adafruit_gfx.setCursor(2, lineY);
      u8g2_for_adafruit_gfx.print(deviceName);
    } else {
      // Text is too long - implement marquee scrolling
      // (Scroll position is updated in update() method)

      // Draw text at scrolled position
      int16_t xPos = displayWidth - scrollOffset;
      u8g2_for_adafruit_gfx.setCursor(xPos, lineY);
      u8g2_for_adafruit_gfx.print(deviceName);

      // If we're near the end, draw another copy for seamless loop
      if (scrollOffset > textPixelWidth) {
        int16_t xPos2 = xPos + textPixelWidth + 60; // +60 pixel gap
        u8g2_for_adafruit_gfx.setCursor(xPos2, lineY);
        u8g2_for_adafruit_gfx.print(deviceName);
      }
    }
  }
}

void DisplayManager::renderWaitingForShots() {
  if (!display) return;

  u8g2_for_adafruit_gfx.setFontMode(1);
  u8g2_for_adafruit_gfx.setFontDirection(0);
  u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::WHITE);

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvR10_tf);
  u8g2_for_adafruit_gfx.setCursor(0, 12);
  u8g2_for_adafruit_gfx.print(F("Shots: 0"));
  u8g2_for_adafruit_gfx.setCursor(0, 28);
  u8g2_for_adafruit_gfx.print(F("Split: 0:00"));

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvB18_tf);
  u8g2_for_adafruit_gfx.setCursor(65, 25);
  u8g2_for_adafruit_gfx.print(F("00:00"));
}

void DisplayManager::renderShotData() {
  if (!display) return;

  char timeBuffer[16];
  char splitBuffer[16];
  char shotBuffer[32];
  formatTime(lastShotData.absoluteTimeMs, timeBuffer, sizeof(timeBuffer));
  formatSplitTime(lastShotData.splitTimeMs, splitBuffer, sizeof(splitBuffer));

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvR10_tf);
  u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::YELLOW);
  u8g2_for_adafruit_gfx.setCursor(0, 12);
  snprintf(shotBuffer, sizeof(shotBuffer), "Shots: %d", lastShotData.shotNumber + 1);
  u8g2_for_adafruit_gfx.print(shotBuffer);

  u8g2_for_adafruit_gfx.setCursor(0, 28);
  u8g2_for_adafruit_gfx.print(F("Split: "));
  u8g2_for_adafruit_gfx.print(splitBuffer);

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvB18_tf);
  u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::GREEN);
  u8g2_for_adafruit_gfx.setCursor(65, 25);
  u8g2_for_adafruit_gfx.print(timeBuffer);
}

void DisplayManager::renderSessionEnd() {
  if (!display) return;

  char timeBuffer[16];
  formatTime(lastShotData.absoluteTimeMs, timeBuffer, sizeof(timeBuffer));

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvB18_tf);
  u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::RED);
  u8g2_for_adafruit_gfx.setCursor(1, 25);
  u8g2_for_adafruit_gfx.print(F("END:"));

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvB18_tf);
  u8g2_for_adafruit_gfx.setForegroundColor(DisplayColors::RED);
  u8g2_for_adafruit_gfx.setCursor(65, 25);
  u8g2_for_adafruit_gfx.print(timeBuffer);
}

void DisplayManager::formatTime(uint32_t timeMs, char* buffer, size_t bufferSize) {
  // Calculate total seconds and centiseconds (hundredths of a second)
  uint32_t totalSeconds = timeMs / 1000;
  uint32_t centiseconds = (timeMs % 1000) / 10;  // Get hundredths of a second (0-99)

  // Format as "ss:cc" where ss = seconds, cc = centiseconds (always 2 digits each)
  snprintf(buffer, bufferSize, "%02lu:%02lu", totalSeconds, centiseconds);
}

void DisplayManager::formatSplitTime(uint32_t timeMs, char* buffer, size_t bufferSize) {
  // Calculate total seconds and centiseconds (hundredths of a second)
  uint32_t totalSeconds = timeMs / 1000;
  uint32_t centiseconds = (timeMs % 1000) / 10;  // Get hundredths of a second (0-99)

  // Format as "s:cc" when < 10 seconds, "ss:cc" when >= 10 seconds
  if (totalSeconds < 10) {
    snprintf(buffer, bufferSize, "%lu:%02lu", totalSeconds, centiseconds);
  } else {
    snprintf(buffer, bufferSize, "%02lu:%02lu", totalSeconds, centiseconds);
  }
}
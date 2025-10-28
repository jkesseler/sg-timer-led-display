#include <Arduino.h>
#include <BLEDevice.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ITimerDevice.h"
#include "SGTimerDevice.h"
#include "common.h"

// Timer device interface
ITimerDevice* timerDevice = nullptr;

// Display state tracking
bool sessionActive = false;
uint16_t lastShotNum = 0;
uint32_t lastShotTime = 0;  // in milliseconds
bool showSessionEnd = false;
SessionData currentSessionData;
NormalizedShotData lastShotData;

// Brightness control variables
uint8_t currentBrightness = DEFAULT_BRIGHTNESS;  // Default brightness (0-255)
unsigned long lastBrightnessUpdate = 0;

// HUB75 LED Panel Configuration
MatrixPanel_I2S_DMA *display = nullptr;

// Function declarations
void initDisplay();
void updateDisplay();
void updateBrightness();
uint8_t readPotentiometerBrightness();
void displayShotData(const NormalizedShotData& shotData);
void displaySessionEnd(const SessionData& sessionData);
void displayWaiting();
void displayConnectionStatus(DeviceConnectionState state);

// Timer device event callbacks
void onShotDetected(const NormalizedShotData& shotData);
void onSessionStarted(const SessionData& sessionData);
void onSessionStopped(const SessionData& sessionData);
void onSessionSuspended(const SessionData& sessionData);
void onSessionResumed(const SessionData& sessionData);
void onConnectionStateChanged(DeviceConnectionState state);

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(STARTUP_DELAY);

  Serial.println("=== SG Shot Timer BLE Bridge ===");
  Serial.println("ESP32-S3 DevKit-C Starting...");

  // Initialize potentiometer pin for brightness control
  pinMode(POTENTIOMETER_PIN, INPUT);
  analogReadResolution(ADC_RESOLUTION);  // Set ADC resolution to 12 bits
  Serial.printf("[BRIGHTNESS] Potentiometer initialized on pin %d\n", POTENTIOMETER_PIN);

  // Initialize LED Display
  initDisplay();

  // Initialize BLE
  BLEDevice::init(BLE_DEVICE_NAME);
  Serial.println("[BLE] ESP32-S3 BLE Client initialized");

  // Create timer device (SG Timer implementation)
  timerDevice = new SGTimerDevice();

  // Set up callbacks
  timerDevice->onShotDetected(onShotDetected);
  timerDevice->onSessionStarted(onSessionStarted);
  timerDevice->onSessionStopped(onSessionStopped);
  timerDevice->onSessionSuspended(onSessionSuspended);
  timerDevice->onSessionResumed(onSessionResumed);
  timerDevice->onConnectionStateChanged(onConnectionStateChanged);

  // Initialize the timer device
  if (timerDevice->initialize()) {
    Serial.println("[TIMER] Timer device initialized successfully");
    timerDevice->startScanning();
  } else {
    Serial.println("[TIMER] Failed to initialize timer device");
  }
}

void loop() {
  // Update timer device (handles connection logic)
  if (timerDevice) {
    timerDevice->update();
  }

  // Update brightness from potentiometer
  updateBrightness();

  // Update display
  updateDisplay();

  delay(MAIN_LOOP_DELAY);
}

// Timer device event callbacks
void onShotDetected(const NormalizedShotData& shotData) {
  // Update display state
  lastShotNum = shotData.shotNumber;
  lastShotTime = shotData.absoluteTimeMs;
  lastShotData = shotData;

  // Update display if session is active
  if (sessionActive) {
    displayShotData(shotData);
  }

  Serial.printf("[DISPLAY] Shot detected: #%d, Time: %.3fs, Split: %.3fs\n",
                shotData.shotNumber,
                shotData.absoluteTimeMs / 1000.0,
                shotData.splitTimeMs / 1000.0);
}

void onSessionStarted(const SessionData& sessionData) {
  Serial.printf("[DISPLAY] Session started: ID %u\n", sessionData.sessionId);

  currentSessionData = sessionData;
  sessionActive = true;
  showSessionEnd = false;
  lastShotNum = 0;
  lastShotTime = 0;

  displayWaiting();  // Show waiting state
}

void onSessionStopped(const SessionData& sessionData) {
  Serial.printf("[DISPLAY] Session stopped: ID %u, Total shots: %d\n",
                sessionData.sessionId, sessionData.totalShots);

  currentSessionData = sessionData;
  sessionActive = false;
  showSessionEnd = true;

  displaySessionEnd(sessionData);
}

void onSessionSuspended(const SessionData& sessionData) {
  Serial.printf("[DISPLAY] Session suspended: ID %u, Total shots: %d\n",
                sessionData.sessionId, sessionData.totalShots);

  currentSessionData = sessionData;
  // Keep sessionActive true for suspended sessions
}

void onSessionResumed(const SessionData& sessionData) {
  Serial.printf("[DISPLAY] Session resumed: ID %u, Total shots: %d\n",
                sessionData.sessionId, sessionData.totalShots);

  currentSessionData = sessionData;
  sessionActive = true;
  showSessionEnd = false;
}

void onConnectionStateChanged(DeviceConnectionState state) {
  Serial.printf("[DISPLAY] Connection state changed: %d\n", (int)state);

  // Update display based on connection state
  if (state == DeviceConnectionState::DISCONNECTED && sessionActive) {
    sessionActive = false;
    showSessionEnd = false;
  }

  displayConnectionStatus(state);
}

// Initialize HUB75 LED Display
void initDisplay() {
  Serial.println("[DISPLAY] Initializing HUB75 LED panels...");

  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,      // Width of one panel
    PANEL_HEIGHT,     // Height of panel
    PANEL_CHAIN       // Number of panels chained
  );

  // Configure for ESP32-S3
  mxconfig.gpio.e = 18;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Uncomment if vertical output isn't aligned in the two panels of a single 1/16 scan panel
  // OR x-coord 0 isn't visible on screen
  // mxconfig.clkphase = false;

  // Uncomment if ghosting/artefacts are seen on the display
  // mxconfig.latch_blanking = 4;
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;





  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
  display->setBrightness8(DEFAULT_BRIGHTNESS); // 0-255
  display->clearScreen();

  // Show startup message
  display->setTextSize(1);
  display->setTextColor(display->color565(0, 255, 0)); // Green
  display->setCursor(2, 2);
  display->print("TIMER");
  display->setCursor(2, 12);
  display->print("READY");

  Serial.println("[DISPLAY] HUB75 panels initialized");
  delay(STARTUP_MESSAGE_DELAY);
  display->clearScreen();
}

// Update display with current state
void updateDisplay() {
  if (showSessionEnd) {
    displaySessionEnd(currentSessionData);
  } else if (sessionActive) {
    if (lastShotNum > 0) {
      displayShotData(lastShotData);
    } else {
      displayWaiting();
    }
  } else {
    // Show connection status when not in session
    DeviceConnectionState state = timerDevice ? timerDevice->getConnectionState() : DeviceConnectionState::DISCONNECTED;
    displayConnectionStatus(state);
  }
}

// Display shot number and time during active session
void displayShotData(const NormalizedShotData& shotData) {
  if (display == nullptr) return;

  display->clearScreen();

  uint32_t shotTimeMs = shotData.absoluteTimeMs;

  // Calculate time components
  uint32_t totalSeconds = shotTimeMs / 1000;
  uint32_t minutes = totalSeconds / 60;
  uint32_t seconds = totalSeconds % 60;
  uint32_t milliseconds = shotTimeMs % 1000;

  // Display shot number (top section)
  display->setTextSize(2);
  display->setTextColor(display->color565(255, 255, 0)); // Yellow
  display->setCursor(2, 2);
  display->printf("SHOT:%d", shotData.shotNumber);

  // Display time (bottom section)
  display->setTextSize(2);
  display->setTextColor(display->color565(0, 255, 0)); // Green
  display->setCursor(2, 18);

  if (minutes > 0) {
    display->printf("%02d:%02d.%01d", minutes, seconds, milliseconds / 100);
  } else {
    display->printf("%02d.%03d", seconds, milliseconds);
  }

  Serial.printf("[DISPLAY] Shot: %d, Time: %02d:%02d.%03d\n",
                shotData.shotNumber, minutes, seconds, milliseconds);
}

// Display waiting state (00:00) when session started but no shots detected
void displayWaiting() {
  if (display == nullptr) return;

  display->clearScreen();

  display->setTextSize(2);
  display->setTextColor(display->color565(100, 100, 255)); // Light blue
  display->setCursor(10, 2);
  display->print("READY");

  display->setTextSize(3);
  display->setTextColor(display->color565(200, 200, 200)); // White
  display->setCursor(10, 18);
  display->print("00:00");

  Serial.println("[DISPLAY] Waiting for shots...");
}

// Display session end summary
void displaySessionEnd(const SessionData& sessionData) {
  if (display == nullptr) return;

  display->clearScreen();

  // Display "SESSION END"
  display->setTextSize(1);
  display->setTextColor(display->color565(255, 0, 0)); // Red
  display->setCursor(15, 2);
  display->print("SESSION END");

  // Display total shots
  display->setTextSize(2);
  display->setTextColor(display->color565(255, 255, 0)); // Yellow
  display->setCursor(5, 14);
  display->printf("SHOTS: %d", sessionData.totalShots);

  // Display last shot number
  if (lastShotNum > 0) {
    display->setTextSize(1);
    display->setTextColor(display->color565(0, 255, 0)); // Green
    display->setCursor(5, 28);
    display->printf("Last: #%d", lastShotNum);
  }

  Serial.printf("[DISPLAY] Session End - Total Shots: %d\n", sessionData.totalShots);
}

// Display connection status
void displayConnectionStatus(DeviceConnectionState state) {
  if (display == nullptr) return;

  display->clearScreen();

  display->setTextSize(1);
  display->setCursor(2, 2);

  switch (state) {
    case DeviceConnectionState::DISCONNECTED:
      display->setTextColor(display->color565(255, 0, 0)); // Red
      display->print("DISCONNECTED");
      break;
    case DeviceConnectionState::SCANNING:
      display->setTextColor(display->color565(255, 255, 0)); // Yellow
      display->print("SCANNING...");
      break;
    case DeviceConnectionState::CONNECTING:
      display->setTextColor(display->color565(0, 0, 255)); // Blue
      display->print("CONNECTING...");
      break;
    case DeviceConnectionState::CONNECTED:
      display->setTextColor(display->color565(0, 255, 0)); // Green
      display->print("CONNECTED");
      if (timerDevice) {
        display->setCursor(2, 12);
        display->printf("Device: %s", timerDevice->getDeviceName());
      }
      break;
    case DeviceConnectionState::ERROR:
      display->setTextColor(display->color565(255, 0, 0)); // Red
      display->print("CONNECTION ERROR");
      break;
  }
}

// Update display brightness based on potentiometer reading
void updateBrightness() {
  unsigned long currentTime = millis();

  // Only update brightness at specified intervals to avoid flickering
  if (currentTime - lastBrightnessUpdate >= BRIGHTNESS_UPDATE_INTERVAL) {
    uint8_t newBrightness = readPotentiometerBrightness();

    // Only update if brightness changed significantly (avoid small fluctuations)
    if (abs(newBrightness - currentBrightness) > BRIGHTNESS_CHANGE_THRESHOLD) {
      currentBrightness = newBrightness;
      if (display) {
        display->setBrightness8(currentBrightness);
      }
      Serial.printf("[BRIGHTNESS] Updated to %d (%.1f%%)\n",
                    currentBrightness, (currentBrightness / 255.0) * 100);
    }

    lastBrightnessUpdate = currentTime;
  }
}

// Read potentiometer and convert to brightness value (0-255)
uint8_t readPotentiometerBrightness() {
  int potValue = analogRead(POTENTIOMETER_PIN);

  // Map potentiometer reading (0 to POTENTIOMETER_MAX_VALUE) to brightness (MIN_BRIGHTNESS to MAX_BRIGHTNESS)
  // Minimum brightness to ensure display is always visible
  uint8_t brightness = map(potValue, 0, POTENTIOMETER_MAX_VALUE, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

  return brightness;
}


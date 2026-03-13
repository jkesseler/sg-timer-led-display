/**
 * @file startup-message.cpp
 * @brief Isolated test for startup message marquee scrolling
 *
 * This test isolates the startup message rendering to debug
 * the marquee scrolling behavior.
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h>

// Display configuration
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define PANEL_CHAIN 2

// Display colors (RGB565)
#define COLOR_GREEN 0x07E0

// Scrolling configuration
#define SCROLL_SPEED_MS 25  // Update scroll every 25ms

// Global objects
MatrixPanel_I2S_DMA* display = nullptr;
U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

// Scrolling state
int16_t startupScrollOffset = 0;
unsigned long startupLastScrollUpdate = 0;
int16_t startupTextPixelWidth = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Startup Message Marquee Test ===");

  // Initialize display
  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,      // Width of one panel
    PANEL_HEIGHT,     // Height of panel
    PANEL_CHAIN       // Number of panels chained
  );

  // Configure for ESP32-S3 to reduce scanlines
  mxconfig.gpio.e = 18;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.latch_blanking = 4;
  mxconfig.clkphase = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  mxconfig.min_refresh_rate = 120;

  display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!display) {
    Serial.println("ERROR: Failed to create display instance");
    while (1) delay(1000);
  }

  display->begin();
  display->setBrightness8(200);
  display->clearScreen();
  display->setTextWrap(false);
  u8g2_for_adafruit_gfx.begin(*display);

  Serial.println("Display initialized successfully");
  Serial.printf("Display width: %d pixels\n", PANEL_WIDTH * PANEL_CHAIN);

  // Calculate text width
  const char* startupText = "Pew Pew Timer. By J.K. technical solutions";
  startupTextPixelWidth = strlen(startupText) * 15;  // Average 15px per character

  Serial.printf("Text: \"%s\"\n", startupText);
  Serial.printf("Text length: %d characters\n", strlen(startupText));
  Serial.printf("Calculated text width: %d pixels\n", startupTextPixelWidth);

  // Initialize scroll position
  startupScrollOffset = 0;
  startupLastScrollUpdate = millis();

  Serial.println("Starting marquee scroll...");
}

void loop() {
  unsigned long currentTime = millis();
  const int16_t displayWidth = PANEL_WIDTH * PANEL_CHAIN;
  bool needsUpdate = false;

  // Update scroll position
  if (currentTime - startupLastScrollUpdate >= SCROLL_SPEED_MS) {
    startupScrollOffset++;

    // Reset when text has scrolled completely off screen
    if (startupScrollOffset > startupTextPixelWidth + 60) {
      startupScrollOffset = 0;
      Serial.printf("Scroll reset to 0 (text width: %d)\n", startupTextPixelWidth);
    }

    startupLastScrollUpdate = currentTime;
    needsUpdate = true;

    // Debug output every 50 pixels
    if (startupScrollOffset % 50 == 0) {
      Serial.printf("Scroll offset: %d pixels\n", startupScrollOffset);
    }
  }

  // Render if needed
  if (needsUpdate) {
    display->clearScreen();

    u8g2_for_adafruit_gfx.setFontMode(1);
    u8g2_for_adafruit_gfx.setFontDirection(0);
    u8g2_for_adafruit_gfx.setForegroundColor(COLOR_GREEN);
    u8g2_for_adafruit_gfx.setFont(u8g2_font_luRS18_tr);

    const char* startupText = "Pew Pew Timer. By J.K. technical solutions";
    const int16_t lineY = 28;

    // Calculate text position (scrolls from left to right)
    int16_t xPos = 0 - startupScrollOffset;

    // Draw first copy of text
    u8g2_for_adafruit_gfx.setCursor(xPos, lineY);
    u8g2_for_adafruit_gfx.print(startupText);

    // Draw second copy for seamless loop
    int16_t xPos2 = xPos + startupTextPixelWidth + 60;
    u8g2_for_adafruit_gfx.setCursor(xPos2, lineY);
    u8g2_for_adafruit_gfx.print(startupText);

    // Debug: show scroll position at top
    u8g2_for_adafruit_gfx.setFont(u8g2_font_helvR10_tf);
    u8g2_for_adafruit_gfx.setCursor(0, 10);
    char debugBuffer[32];
    snprintf(debugBuffer, sizeof(debugBuffer), "Offset:%d", startupScrollOffset);
    u8g2_for_adafruit_gfx.print(debugBuffer);
  }

  delay(10);  // Small delay to prevent CPU overload
}

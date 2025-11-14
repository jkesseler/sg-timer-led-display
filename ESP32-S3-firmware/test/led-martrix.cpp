/**
 * Minimal HUB75 LED Matrix Test
 *
 * Tests two chained 64x32 HUB75 panels with simple scrolling text
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Panel Configuration
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define PANEL_CHAIN 2

// Total display dimensions
#define DISPLAY_WIDTH (PANEL_WIDTH * PANEL_CHAIN)
#define DISPLAY_HEIGHT PANEL_HEIGHT

// Colors (RGB565 format)
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_WHITE   0xFFFF

// Global display object
MatrixPanel_I2S_DMA *display = nullptr;

// Scrolling text variables
int16_t scrollX = DISPLAY_WIDTH;
const char* testText = "HUB75 TEST";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================");
  Serial.println("HUB75 LED Matrix Test");
  Serial.println("=================================");

  // Configure HUB75 display
  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,      // Width of one panel
    PANEL_HEIGHT,     // Height of panel
    PANEL_CHAIN       // Number of panels chained
  );

  // Configure for ESP32-S3
  // mxconfig.gpio.e = 18;
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Create display instance
  display = new MatrixPanel_I2S_DMA(mxconfig);

  if (!display) {
    Serial.println("ERROR: Failed to create display instance!");
    return;
  }

  // Initialize display
  if (!display->begin()) {
    Serial.println("ERROR: Display initialization failed!");
    return;
  }

  display->setBrightness8(128); // Medium brightness
  display->clearScreen();

  Serial.println("Display initialized successfully!");
  Serial.printf("Display size: %d x %d pixels\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void loop() {
  // Clear display
  display->fillScreen(0); // Black background

  // Set text properties
  display->setTextSize(1);
  display->setTextColor(COLOR_GREEN);
  display->setTextWrap(false);

  // Draw scrolling text
  display->setCursor(scrollX, 12);
  display->print(testText);

  // Update scroll position (scroll from right to left)
  scrollX--;

  // Calculate approximate text width (6 pixels per char)
  int16_t textWidth = strlen(testText) * 6;

  // Reset when text scrolls off the left side
  if (scrollX < -textWidth) {
    scrollX = DISPLAY_WIDTH;
  }

  // Add some static elements for visual reference
  // Corner markers
  display->drawPixel(0, 0, COLOR_RED);
  display->drawPixel(DISPLAY_WIDTH - 1, 0, COLOR_BLUE);
  display->drawPixel(0, DISPLAY_HEIGHT - 1, COLOR_YELLOW);
  display->drawPixel(DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, COLOR_WHITE);

  // Center line
  // display->drawLine(0, DISPLAY_HEIGHT / 2, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT / 2, 0x18E3); // Dark cyan

  delay(30); // ~33 FPS
}

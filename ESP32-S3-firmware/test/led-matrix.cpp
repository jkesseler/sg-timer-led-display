/**
 * Minimal HUB75 LED Matrix Test
 *
 * Tests two chained 64x32 HUB75 panels with simple scrolling text
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h>

U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

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
#define COLOR_LIGHT_BLUE 0x647F
#define COLOR_GRAY    0x8410

// Global display object
MatrixPanel_I2S_DMA *display = nullptr;

// Scrolling text variables
int16_t scrollX = DISPLAY_WIDTH;
const char* testText = "HUB75 TEST";

void setup() {
  Serial.begin(115200);

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
  // mxconfig.latch_blanking = 2;
  // mxconfig.clkphase = false;
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;

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

  u8g2_for_adafruit_gfx.begin(*display);                 // connect u8g2 procedures to Adafruit GFX (dereference pointer)


  display->setBrightness8(200); // Medium brightness
  display->clearScreen();
  display->setTextWrap(false);

  Serial.println("Display initialized successfully!");
  Serial.printf("Display size: %d x %d pixels\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void loop() {
  // Clear display and draw text
  display->clearScreen();

  u8g2_for_adafruit_gfx.setFontMode(1);                // use u8g2 transparent mode (this is default)
  u8g2_for_adafruit_gfx.setFontDirection(0);             // left to right (this is default)
  u8g2_for_adafruit_gfx.setForegroundColor(COLOR_WHITE);     // apply Adafruit GFX color

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvR10_tf); // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  u8g2_for_adafruit_gfx.setCursor(0, 12);              // start writing at this position
  u8g2_for_adafruit_gfx.print(F("Shots: 12"));
  u8g2_for_adafruit_gfx.setCursor(0, 28);              // start writing at this position
  u8g2_for_adafruit_gfx.print(F("Split: 0:78"));       // UTF-8 string with german umlaut chars

  u8g2_for_adafruit_gfx.setFont(u8g2_font_helvB18_tf);
  u8g2_for_adafruit_gfx.setCursor(65, 25);
  u8g2_for_adafruit_gfx.print(F("12:92"));

  delay(2000);
}
#include "BrightnessController.h"
#include "Logger.h"
#include <Arduino.h>

BrightnessController::BrightnessController()
  : currentBrightness(DEFAULT_BRIGHTNESS),
    lastUpdateTime(0) {
}

bool BrightnessController::initialize() {
  pinMode(POTENTIOMETER_PIN, INPUT);
  analogReadResolution(ADC_RESOLUTION);

  LOG_BRIGHTNESS("Potentiometer initialized on pin %d", POTENTIOMETER_PIN);
  return true;
}

void BrightnessController::update() {
  unsigned long currentTime = millis();

  // Only update brightness at specified intervals to avoid flickering
  if (currentTime - lastUpdateTime >= BRIGHTNESS_UPDATE_INTERVAL) {
    uint8_t newBrightness = readPotentiometerValue();

    // Only update if brightness changed significantly (avoid small fluctuations)
    if (abs(newBrightness - currentBrightness) > BRIGHTNESS_CHANGE_THRESHOLD) {
      setCurrentBrightness(newBrightness);
    }

    lastUpdateTime = currentTime;
  }
}

void BrightnessController::setBrightnessCallback(std::function<void(uint8_t)> callback) {
  brightnessCallback = callback;
}

void BrightnessController::setCurrentBrightness(uint8_t brightness) {
  if (brightness != currentBrightness) {
    currentBrightness = brightness;

    if (brightnessCallback) {
      brightnessCallback(brightness);
    }

    LOG_BRIGHTNESS("Brightness updated to %d (%.1f%%)",
                   brightness, (brightness / 255.0) * 100);
  }
}

uint8_t BrightnessController::readPotentiometerValue() {
  int potValue = analogRead(POTENTIOMETER_PIN);
  return mapToBrightness(potValue);
}

uint8_t BrightnessController::mapToBrightness(int potValue) {
  // Map potentiometer reading to brightness range
  // Ensure minimum brightness so display is always visible
  uint8_t brightness = map(potValue, 0, POTENTIOMETER_MAX_VALUE,
                          MIN_BRIGHTNESS, MAX_BRIGHTNESS);
  return brightness;
}
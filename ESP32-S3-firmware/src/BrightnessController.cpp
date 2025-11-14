#include "BrightnessController.h"
#include "Logger.h"
#include <Arduino.h>

BrightnessController::BrightnessController()
  : currentBrightness(DEFAULT_BRIGHTNESS),
    lastUpdateTime(0) {
}

bool BrightnessController::initialize() {
  pinMode(POTENTIOMETER_PIN, INPUT_PULLDOWN); // Add internal pull-down resistor
  analogReadResolution(ADC_RESOLUTION);
  analogSetAttenuation(ADC_11db);

  LOG_BRIGHTNESS("Potentiometer initialized on pin %d", POTENTIOMETER_PIN);

  // Set initial brightness to DEFAULT_BRIGHTNESS
  setCurrentBrightness(DEFAULT_BRIGHTNESS);

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

  // Detect if potentiometer is not connected (pulled to ground by INPUT_PULLDOWN)
  // If reading is very low (< 5% of max), assume no potentiometer and use default
  const int NO_POTENTIOMETER_THRESHOLD = POTENTIOMETER_MAX_VALUE * 0.05;

  if (potValue < NO_POTENTIOMETER_THRESHOLD) {
    LOG_DEBUG("BRIGHTNESS", "No potentiometer detected (ADC: %d), using DEFAULT_BRIGHTNESS", potValue);
    return DEFAULT_BRIGHTNESS;
  }

  return mapToBrightness(potValue);
}

uint8_t BrightnessController::mapToBrightness(int potValue) {
  // Map potentiometer reading to brightness range
  // Ensure minimum brightness so display is always visible
  uint8_t brightness = map(potValue, 0, POTENTIOMETER_MAX_VALUE, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
  return brightness;
}
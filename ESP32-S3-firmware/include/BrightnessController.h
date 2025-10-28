#pragma once

#include "common.h"
#include <functional>

class BrightnessController {
private:
  uint8_t currentBrightness;
  unsigned long lastUpdateTime;
  std::function<void(uint8_t)> brightnessCallback;

  uint8_t readPotentiometerValue();
  uint8_t mapToBrightness(int potValue);

public:
  BrightnessController();

  bool initialize();
  void update();
  void setBrightnessCallback(std::function<void(uint8_t)> callback);

  uint8_t getCurrentBrightness() const { return currentBrightness; }
  void setCurrentBrightness(uint8_t brightness);
};
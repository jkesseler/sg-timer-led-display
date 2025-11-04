#pragma once

#include <Arduino.h>
#include "common.h"

class ButtonHandler {
private:
  volatile bool buttonPressed;
  unsigned long lastPressTime;

  static ButtonHandler* instance;
  static void IRAM_ATTR buttonISR();

public:
  ButtonHandler();
  ~ButtonHandler();

  bool initialize();
  bool checkButtonPress(); // Returns true if button was pressed (resets flag)

  // For ISR access
  void setButtonPressed() { buttonPressed = true; }
};
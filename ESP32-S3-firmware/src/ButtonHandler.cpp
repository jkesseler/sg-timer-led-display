#include "ButtonHandler.h"
#include "Logger.h"

// Static instance for ISR
ButtonHandler* ButtonHandler::instance = nullptr;

ButtonHandler::ButtonHandler()
  : buttonPressed(false), lastPressTime(0) {
  instance = this;
}

ButtonHandler::~ButtonHandler() {
  instance = nullptr;
}

bool ButtonHandler::initialize() {
  LOG_SYSTEM("Initializing reset button on pin %d", RESET_BUTTON_PIN);

  // Configure button pin as input with pullup
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Attach interrupt for falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(RESET_BUTTON_PIN), buttonISR, FALLING);

  LOG_SYSTEM("Reset button initialized successfully");
  return true;
}

bool ButtonHandler::checkButtonPress() {
  if (buttonPressed) {
    unsigned long currentTime = millis();

    // Check debounce
    if (currentTime - lastPressTime > BUTTON_DEBOUNCE_MS) {
      buttonPressed = false;
      lastPressTime = currentTime;
      LOG_SYSTEM("Button press detected and processed");
      return true;
    } else {
      // Too soon, ignore (debounce)
      buttonPressed = false;
    }
  }

  return false;
}

void IRAM_ATTR ButtonHandler::buttonISR() {
  if (instance) {
    instance->setButtonPressed();
  }
}
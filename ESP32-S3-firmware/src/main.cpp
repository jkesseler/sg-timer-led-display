#include <Arduino.h>
#include "TimerApplication.h"
#include "Logger.h"
#include "common.h"

// Global application instance
TimerApplication* app = nullptr;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  // Set logging level
  Logger::setLevel(LogLevel::INFO);

  // Create and initialize application
  app = new TimerApplication();
  if (!app->initialize()) {
    LOG_ERROR("MAIN", "Failed to initialize application");
    while (true) {
      delay(1000); // Halt on initialization failure
    }
  }
}

void loop() {
  if (app) {
    app->run();
  }
}
#include <Arduino.h>
#include "TimerApplication.h"
#include "Logger.h"
#include "common.h"
#include <memory>

// Global application instance - using unique_ptr for proper lifecycle management
// Note: std::make_unique requires C++14, using reset() pattern for C++11 compatibility
static std::unique_ptr<TimerApplication> app;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  // Set logging level
  Logger::setLevel(LogLevel::INFO);

  // Create and initialize application
  app.reset(new TimerApplication());
  if (!app->initialize()) {
    LOG_ERROR("MAIN", "Failed to initialize application");
    // Halt on initialization failure - indicate error state
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  if (app) {
    app->run();
  }
}
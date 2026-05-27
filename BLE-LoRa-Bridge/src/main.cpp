#include <Arduino.h>
#include "BridgeApplication.h"

BridgeApplication app;

void setup() {
  app.initialize();
}

void loop() {
  app.run();
}

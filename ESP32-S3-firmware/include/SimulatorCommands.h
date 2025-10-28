#pragma once

#include "SGTimerSimulator.h"
#include "Logger.h"

// Simple command interface for controlling the simulator
class SimulatorCommands {
private:
  SGTimerSimulator* simulator;
  String inputBuffer;

  void processCommand(const String& command);
  void showHelp();
  void showStatus();

public:
  SimulatorCommands(SGTimerSimulator* sim);
  void update(); // Call this in main loop to process serial input
  void handleSerialInput();
};
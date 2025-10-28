#include "SimulatorCommands.h"

SimulatorCommands::SimulatorCommands(SGTimerSimulator* sim)
  : simulator(sim), inputBuffer("") {
}

void SimulatorCommands::update() {
  handleSerialInput();
}

void SimulatorCommands::handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else if (c >= 32 && c <= 126) { // Printable characters
      inputBuffer += c;
    }
  }
}

void SimulatorCommands::processCommand(const String& command) {
  String cmd = command;
  cmd.trim();
  cmd.toLowerCase();

  if (!simulator) {
    LOG_ERROR("SIM-CMD", "No simulator instance available");
    return;
  }

  LOG_INFO("SIM-CMD", "Processing command: %s", cmd.c_str());

  if (cmd == "help" || cmd == "h") {
    showHelp();
  }
  else if (cmd == "status" || cmd == "s") {
    showStatus();
  }
  else if (cmd == "connect" || cmd == "c") {
    if (simulator->getConnectionState() == DeviceConnectionState::DISCONNECTED) {
      simulator->startScanning();
      LOG_INFO("SIM-CMD", "Starting connection simulation");
    } else {
      LOG_WARN("SIM-CMD", "Already connected or connecting");
    }
  }
  else if (cmd == "disconnect" || cmd == "d") {
    simulator->disconnect();
    LOG_INFO("SIM-CMD", "Disconnecting simulator");
  }
  else if (cmd == "start" || cmd == "st") {
    if (simulator->startSession()) {
      LOG_INFO("SIM-CMD", "Starting session");
    } else {
      LOG_WARN("SIM-CMD", "Failed to start session");
    }
  }
  else if (cmd == "stop" || cmd == "sp") {
    if (simulator->stopSession()) {
      LOG_INFO("SIM-CMD", "Stopping session");
    } else {
      LOG_WARN("SIM-CMD", "Failed to stop session");
    }
  }
  else if (cmd == "shot" || cmd == "sh") {
    simulator->simulateManualShot();
    LOG_INFO("SIM-CMD", "Manual shot triggered");
  }
  else if (cmd == "reset" || cmd == "r") {
    simulator->reset();
    LOG_INFO("SIM-CMD", "Simulator reset");
  }
  else if (cmd == "auto") {
    simulator->setSimulationMode(SimulationMode::AUTO_SHOTS);
    LOG_INFO("SIM-CMD", "Switched to AUTO_SHOTS mode");
  }
  else if (cmd == "manual" || cmd == "m") {
    simulator->setSimulationMode(SimulationMode::MANUAL);
    LOG_INFO("SIM-CMD", "Switched to MANUAL mode");
  }
  else if (cmd == "realistic") {
    simulator->setSimulationMode(SimulationMode::REALISTIC);
    LOG_INFO("SIM-CMD", "Switched to REALISTIC mode");
  }
  else if (cmd == "demo") {
    // Quick demo sequence
    simulator->setSimulationMode(SimulationMode::AUTO_SHOTS);
    if (simulator->getConnectionState() == DeviceConnectionState::DISCONNECTED) {
      simulator->startScanning();
    }
    LOG_INFO("SIM-CMD", "Starting demo sequence");
  }
  else {
    LOG_WARN("SIM-CMD", "Unknown command: %s (type 'help' for commands)", cmd.c_str());
  }
}

void SimulatorCommands::showHelp() {
  Serial.println("\n=== SG Timer Simulator Commands ===");
  Serial.println("Connection:");
  Serial.println("  connect, c    - Start connection simulation");
  Serial.println("  disconnect, d - Disconnect simulator");
  Serial.println("");
  Serial.println("Session Control:");
  Serial.println("  start, st     - Start shooting session");
  Serial.println("  stop, sp      - Stop shooting session");
  Serial.println("  shot, sh      - Trigger manual shot (manual mode)");
  Serial.println("");
  Serial.println("Simulation Modes:");
  Serial.println("  manual, m     - Manual control mode");
  Serial.println("  auto          - Auto shots mode");
  Serial.println("  realistic     - Realistic shooting pattern");
  Serial.println("");
  Serial.println("Utility:");
  Serial.println("  status, s     - Show current status");
  Serial.println("  reset, r      - Reset simulator");
  Serial.println("  demo          - Start quick demo");
  Serial.println("  help, h       - Show this help");
  Serial.println("");
  Serial.println("Type commands and press Enter");
  Serial.println("=====================================\n");
}

void SimulatorCommands::showStatus() {
  const char* stateNames[] = {"DISCONNECTED", "SCANNING", "CONNECTING", "CONNECTED", "ERROR"};
  const char* modeNames[] = {"MANUAL", "AUTO_CONNECT", "AUTO_SHOTS", "REALISTIC"};

  Serial.println("\n=== Simulator Status ===");
  Serial.printf("Device: %s\n", simulator->getDeviceName());
  Serial.printf("Model: %s\n", simulator->getDeviceModel());
  Serial.printf("Connection: %s\n", stateNames[(int)simulator->getConnectionState()]);
  Serial.printf("Mode: %s\n", modeNames[(int)simulator->getSimulationMode()]);
  Serial.printf("Session Active: %s\n", simulator->isSessionActive() ? "YES" : "NO");
  Serial.printf("Shot Count: %d\n", simulator->getCurrentShotCount());
  Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Uptime: %lu ms\n", millis());
  Serial.println("========================\n");
}
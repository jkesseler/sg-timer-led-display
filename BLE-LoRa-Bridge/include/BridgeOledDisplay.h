#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include "common.h"
#include "Logger.h"

/**
 * @brief Status information displayed on the OLED
 */
struct BridgeStatus {
  BridgeRole role = BridgeRole::TRANSMITTER;
  ReceiverOutputMode outputMode = ReceiverOutputMode::MQTT_OUTPUT;

  // Transmitter fields
  const char* timerModel = nullptr;  // e.g. "SG Timer Sport"
  bool bleConnected = false;
  bool bleScanning = false;
  uint32_t shotsTx = 0;

  // Receiver fields
  int lastRssi = 0;
  uint32_t shotsRx = 0;
  uint32_t crcErrors = 0;
  bool mqttConnected = false;
  uint16_t bleClients = 0;
  bool hasLastShot = false;
  uint16_t lastShotNumber = 0;
  uint32_t lastShotTimeMs = 0;

  // Common
  bool wifiConnected = false;
  uint32_t uptimeMs = 0;
};

/**
 * @brief Manages the SSD1306 128×64 OLED on the LoRa32 T3 v1.6.1
 *
 * Shows device role, connection status, shot counts, and diagnostics.
 * Full redraw only on state change to minimize flickering.
 */
class OledDisplay {
public:
  bool initialize();
  void update(const BridgeStatus& status);
  void showStartupMessage(const char* message);
  void showConfigPortal(const char* ssid);

private:
  SSD1306Wire* display = nullptr;
  bool initialized = false;

  // Last rendered state — skip redraw if unchanged
  BridgeStatus lastStatus;
  bool firstDraw = true;

  void drawTransmitterView(const BridgeStatus& status);
  void drawReceiverView(const BridgeStatus& status);
  void drawCommonFooter(const BridgeStatus& status);
  bool statusChanged(const BridgeStatus& a, const BridgeStatus& b) const;
};

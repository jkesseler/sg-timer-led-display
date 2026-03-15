#include "BridgeOledDisplay.h"

bool OledDisplay::initialize() {
  display = new SSD1306Wire(OLED_I2C_ADDR, OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display->init()) {
    LOG_ERROR("OLED", "SSD1306 init failed");
    return false;
  }

  display->flipScreenVertically();
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->clear();
  display->display();

  initialized = true;
  LOG_INFO("OLED", "Display initialized (%dx%d)", OLED_WIDTH, OLED_HEIGHT);
  return true;
}

void OledDisplay::showStartupMessage(const char* message) {
  if (!initialized) return;
  display->clear();
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(OLED_WIDTH / 2, 10, message);
  display->setFont(ArialMT_Plain_10);
  display->drawString(OLED_WIDTH / 2, 35, "LoRa Bridge");
  display->drawString(OLED_WIDTH / 2, 50, "v1.0.0");
  display->display();
}

void OledDisplay::showConfigPortal(const char* ssid) {
  if (!initialized) return;
  display->clear();
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(OLED_WIDTH / 2, 0, "WiFi Config Portal");
  display->drawString(OLED_WIDTH / 2, 15, "Connect to:");
  display->setFont(ArialMT_Plain_16);
  display->drawString(OLED_WIDTH / 2, 30, ssid);
  display->setFont(ArialMT_Plain_10);
  display->drawString(OLED_WIDTH / 2, 52, "192.168.4.1");
  display->display();
}

void OledDisplay::update(const BridgeStatus& status) {
  if (!initialized) return;

  // Skip full redraw if nothing changed
  if (!firstDraw && !statusChanged(status, lastStatus)) return;

  display->clear();
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  if (status.role == BridgeRole::TRANSMITTER) {
    drawTransmitterView(status);
  } else {
    drawReceiverView(status);
  }

  drawCommonFooter(status);
  display->display();

  lastStatus = status;
  firstDraw = false;
}

void OledDisplay::drawTransmitterView(const BridgeStatus& status) {
  // Row 0: Role label
  display->setFont(ArialMT_Plain_16);
  display->drawString(0, 0, "TX  BLE->LoRa");

  display->setFont(ArialMT_Plain_10);

  // Row 1: BLE status
  if (status.bleConnected && status.timerModel) {
    display->drawString(0, 18, String("BLE: ") + status.timerModel);
  } else if (status.bleScanning) {
    display->drawString(0, 18, "BLE: Scanning...");
  } else {
    display->drawString(0, 18, "BLE: Disconnected");
  }

  // Row 2: Shot counter
  display->drawString(0, 30, String("Shots TX: ") + String(status.shotsTx));
}

void OledDisplay::drawReceiverView(const BridgeStatus& status) {
  // Row 0: Role + output mode (large)
  display->setFont(ArialMT_Plain_16);
  String heading = (status.outputMode == ReceiverOutputMode::MQTT_OUTPUT)
    ? "RX  LoRa->MQTT"
    : "RX  LoRa->BLE";
  display->drawString(0, 0, heading);

  display->setFont(ArialMT_Plain_10);

  // Row 1: Last shot or waiting message
  if (status.hasLastShot) {
    char shotBuf[24];
    snprintf(shotBuf, sizeof(shotBuf), "#%u  %.3fs",
             status.lastShotNumber, status.lastShotTimeMs / 1000.0f);
    display->drawString(0, 18, shotBuf);
  } else {
    display->drawString(0, 18, "Waiting for LoRa...");
  }

  // Row 2: Shot count + RSSI
  String row2 = String("RX:") + String(status.shotsRx);
  if (status.shotsRx > 0) {
    row2 += String("  ") + String(status.lastRssi) + "dBm";
  }
  display->drawString(0, 30, row2);

  // Row 2 right: output-specific status
  if (status.outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    display->drawString(80, 30, status.mqttConnected ? "MQTT:OK" : "MQTT:--");
  } else {
    display->drawString(80, 30, String("BLE:") + String(status.bleClients));
  }
}

void OledDisplay::drawCommonFooter(const BridgeStatus& status) {
  // Bottom row: WiFi + uptime
  String footer;
  if (status.wifiConnected) {
    footer = "WiFi: OK";
  } else {
    footer = "WiFi: --";
  }
  footer += "  Up: " + String(status.uptimeMs / 1000) + "s";

  if (status.crcErrors > 0) {
    footer += "  E:" + String(status.crcErrors);
  }

  display->drawString(0, 52, footer);
}

bool OledDisplay::statusChanged(const BridgeStatus& a, const BridgeStatus& b) const {
  // Compare all displayed fields
  if (a.role != b.role) return true;
  if (a.outputMode != b.outputMode) return true;
  if (a.bleConnected != b.bleConnected) return true;
  if (a.bleScanning != b.bleScanning) return true;
  if (a.shotsTx != b.shotsTx) return true;
  if (a.shotsRx != b.shotsRx) return true;
  if (a.lastRssi != b.lastRssi) return true;
  if (a.mqttConnected != b.mqttConnected) return true;
  if (a.bleClients != b.bleClients) return true;
  if (a.wifiConnected != b.wifiConnected) return true;
  if (a.crcErrors != b.crcErrors) return true;
  if (a.hasLastShot != b.hasLastShot) return true;
  if (a.lastShotNumber != b.lastShotNumber) return true;

  // Uptime changes every second — throttle redraw to 1s
  if ((a.uptimeMs / 1000) != (b.uptimeMs / 1000)) return true;

  return false;
}

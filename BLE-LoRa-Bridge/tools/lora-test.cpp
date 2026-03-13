/**
 * @brief LoRa packet echo test for BLE-LoRa Bridge
 *
 * Flash on two LilyGo LoRa32 T3 v1.6.1 devices:
 *   Device A: Sends synthetic shot packets every 2 seconds
 *   Device B: Receives, validates CRC, and prints decoded data
 *
 * Both roles run simultaneously — each device transmits AND receives.
 * Use the serial monitor (115200 baud) to observe packet round-trips.
 *
 * Build: pio run -e lora-bridge-tools-lora-test -t upload
 * Monitor: pio device monitor -b 115200
 */
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <SSD1306Wire.h>
#include "common.h"
#include "LoRaPacket.h"
#include "Logger.h"

static SSD1306Wire display(OLED_I2C_ADDR, OLED_SDA_PIN, OLED_SCL_PIN);
static uint8_t txBuf[LoRaProtocol::MAX_PACKET_SIZE];
static uint8_t rxBuf[LoRaProtocol::MAX_PACKET_SIZE];

static uint32_t txCount = 0;
static uint32_t rxCount = 0;
static uint32_t crcErrors = 0;
static uint16_t fakeShotNum = 0;

void initLoRa() {
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("ERROR: LoRa init failed!");
    while (true) delay(1000);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();

  Serial.println("LoRa initialized (SF7/BW500/868MHz)");
}

void initDisplay() {
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "LoRa Test");
  display.drawString(0, 15, "TX + RX active");
  display.display();
}

void sendTestPacket() {
  fakeShotNum++;

  NormalizedShotData shot = {};
  shot.sessionId = 1;
  shot.shotNumber = fakeShotNum;
  shot.absoluteTimeMs = fakeShotNum * 1500;  // 1.5s splits
  shot.splitTimeMs = 1500;
  shot.isFirstShot = (fakeShotNum == 1);
  strncpy(shot.deviceModel, "LoRa Test", sizeof(shot.deviceModel) - 1);

  size_t len = LoRaProtocol::serializeShotDetected(txBuf, sizeof(txBuf), "TESTER", shot);
  if (len == 0) {
    Serial.println("ERROR: serialize failed");
    return;
  }

  LoRa.beginPacket();
  LoRa.write(txBuf, len);
  if (LoRa.endPacket()) {
    txCount++;
    Serial.printf("TX shot #%u (%u bytes) total: %lu\n",
                  fakeShotNum, (unsigned)len, (unsigned long)txCount);
  } else {
    Serial.println("TX failed");
  }
}

void checkReceive() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;
  if ((size_t)packetSize > sizeof(rxBuf)) {
    while (LoRa.available()) LoRa.read();
    Serial.println("RX: packet too large");
    return;
  }

  size_t bytesRead = 0;
  while (LoRa.available() && bytesRead < (size_t)packetSize) {
    rxBuf[bytesRead++] = (uint8_t)LoRa.read();
  }

  int rssi = LoRa.packetRssi();

  LoRaProtocol::ParsedPacket pkt;
  if (!LoRaProtocol::deserialize(rxBuf, bytesRead, pkt)) {
    crcErrors++;
    Serial.printf("RX: CRC/parse error (%u bytes, RSSI %d, errors: %lu)\n",
                  (unsigned)bytesRead, rssi, (unsigned long)crcErrors);
    return;
  }

  rxCount++;

  if (pkt.type == LoRaProtocol::PacketType::SHOT_DETECTED) {
    Serial.printf("RX shot #%u: %.3fs (split: %.3fs) from %s  RSSI: %d  total: %lu\n",
                  pkt.shot.shotNumber,
                  pkt.shot.absoluteTimeMs / 1000.0f,
                  pkt.shot.splitTimeMs / 1000.0f,
                  pkt.sourceId, rssi, (unsigned long)rxCount);
  } else {
    Serial.printf("RX type=0x%02X from %s  RSSI: %d\n",
                  (uint8_t)pkt.type, pkt.sourceId, rssi);
  }
}

void updateDisplay() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "LoRa Echo Test");
  display.drawString(0, 15, String("TX: ") + String(txCount));
  display.drawString(0, 28, String("RX: ") + String(rxCount));
  display.drawString(0, 41, String("CRC Err: ") + String(crcErrors));
  display.drawString(0, 54, String("Up: ") + String(millis() / 1000) + "s");
  display.display();
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Serial.println("\n=== LoRa Echo Test ===");

  initDisplay();
  initLoRa();
}

void loop() {
  // Send a test packet every 2 seconds
  static unsigned long lastTx = 0;
  if (millis() - lastTx >= 2000) {
    sendTestPacket();
    lastTx = millis();
  }

  // Always check for incoming packets
  checkReceive();

  // Update display every second
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 1000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  delay(10);
}

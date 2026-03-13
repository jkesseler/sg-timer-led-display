#include "SpecialPieBleServer.h"

bool SpecialPieBleServer::initialize() {
  BLEDevice::init("Special Pie M1A2+");

  pServer = BLEDevice::createServer();
  serverCallbacks.parent = this;
  pServer->setCallbacks(&serverCallbacks);

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pNotifyChar = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_NOTIFY
  );
  pNotifyChar->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  LOG_INFO("BLE", "Special Pie BLE server started (service %s)", SERVICE_UUID);
  return true;
}

void SpecialPieBleServer::update() {
  // Re-start advertising if no clients connected (auto-reconnect)
  if (connectedCount == 0) {
    BLEDevice::startAdvertising();
  }
}

void SpecialPieBleServer::sendSessionStart(uint8_t sessionId) {
  uint8_t frame[] = {
    FRAME_START_0, FRAME_START_1,
    MSG_SESSION_START,
    sessionId,
    FRAME_END_0, FRAME_END_1
  };
  sendNotification(frame, sizeof(frame));
  LOG_INFO("BLE", "TX session start (id=%u) to %u clients", sessionId, connectedCount);
}

void SpecialPieBleServer::sendShotDetected(uint32_t absoluteTimeMs, uint16_t shotNumber) {
  // Convert milliseconds → seconds + centiseconds (reverse of client parsing)
  uint8_t seconds = (uint8_t)(absoluteTimeMs / 1000);
  uint8_t centiseconds = (uint8_t)((absoluteTimeMs % 1000) / 10);

  // Shot number: Special Pie uses 0-indexed
  uint8_t shotIdx = (shotNumber > 0) ? (uint8_t)(shotNumber - 1) : 0;

  // Checksum byte (simple XOR of data bytes, matching observed protocol behavior)
  uint8_t checksum = seconds ^ centiseconds ^ shotIdx;

  uint8_t frame[] = {
    FRAME_START_0, FRAME_START_1,
    MSG_SHOT_DETECTED,
    0x00,           // reserved
    seconds,
    centiseconds,
    shotIdx,
    checksum,
    FRAME_END_0, FRAME_END_1
  };
  sendNotification(frame, sizeof(frame));
  LOG_DEBUG("BLE", "TX shot #%u (%u.%02us) to %u clients",
            shotNumber, seconds, centiseconds, connectedCount);
}

void SpecialPieBleServer::sendSessionStop(uint8_t sessionId) {
  uint8_t frame[] = {
    FRAME_START_0, FRAME_START_1,
    MSG_SESSION_STOP,
    sessionId,
    FRAME_END_0, FRAME_END_1
  };
  sendNotification(frame, sizeof(frame));
  LOG_INFO("BLE", "TX session stop (id=%u) to %u clients", sessionId, connectedCount);
}

void SpecialPieBleServer::sendNotification(const uint8_t* data, size_t len) {
  if (!pNotifyChar || connectedCount == 0) return;
  pNotifyChar->setValue(const_cast<uint8_t*>(data), len);
  pNotifyChar->notify();
}

// ─── Server callbacks ────────────────────────────────────────

void SpecialPieBleServer::ServerCallbacks::onConnect(BLEServer* /*pServer*/) {
  if (parent) {
    parent->connectedCount++;
    LOG_INFO("BLE", "Client connected (total: %u)", parent->connectedCount);
    // Allow multiple connections
    BLEDevice::startAdvertising();
  }
}

void SpecialPieBleServer::ServerCallbacks::onDisconnect(BLEServer* /*pServer*/) {
  if (parent && parent->connectedCount > 0) {
    parent->connectedCount--;
    LOG_INFO("BLE", "Client disconnected (remaining: %u)", parent->connectedCount);
  }
}

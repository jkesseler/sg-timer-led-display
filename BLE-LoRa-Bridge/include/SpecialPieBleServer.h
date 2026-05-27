#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "Logger.h"

/**
 * @brief BLE GATT server emulating a Special Pie M1A2+ peripheral
 *
 * Device 2 (Receiver) in BLE output mode advertises as a Special Pie
 * timer device. Any BLE central (e.g., another ESP32-S3 bridge running
 * the SpecialPieM1A2Plus client code) can connect and receive shot
 * notifications in the native Special Pie frame format:
 *
 *   Session start: F8 F9 34 [SESSION_ID] F9 F8
 *   Shot detected: F8 F9 36 00 [SEC] [CS] [SHOT#] [CHECKSUM] F9 F8
 *   Session stop:  F8 F9 18 [SESSION_ID] F9 F8
 *
 * Time values are converted from milliseconds back to seconds + centiseconds.
 */
class SpecialPieBleServer {
public:
  bool initialize();
  void update();

  // Transmit events to connected BLE centrals
  void sendSessionStart(uint8_t sessionId);
  void sendShotDetected(uint32_t absoluteTimeMs, uint16_t shotNumber);
  void sendSessionStop(uint8_t sessionId);

  bool hasConnectedClients() const { return connectedCount > 0; }
  uint16_t getConnectedCount() const { return connectedCount; }

private:
  // Special Pie protocol UUIDs
  static constexpr const char* SERVICE_UUID        = "0000FFF0-0000-1000-8000-00805F9B34FB";
  static constexpr const char* CHARACTERISTIC_UUID  = "0000FFF1-0000-1000-8000-00805F9B34FB";

  // Frame markers
  static constexpr uint8_t FRAME_START_0 = 0xF8;
  static constexpr uint8_t FRAME_START_1 = 0xF9;
  static constexpr uint8_t FRAME_END_0   = 0xF9;
  static constexpr uint8_t FRAME_END_1   = 0xF8;

  // Message types
  static constexpr uint8_t MSG_SESSION_START = 0x34;
  static constexpr uint8_t MSG_SHOT_DETECTED = 0x36;
  static constexpr uint8_t MSG_SESSION_STOP  = 0x18;

  BLEServer* pServer = nullptr;
  BLECharacteristic* pNotifyChar = nullptr;
  uint16_t connectedCount = 0;

  void sendNotification(const uint8_t* data, size_t len);

  // BLE server callbacks (track connected clients)
  class ServerCallbacks : public BLEServerCallbacks {
  public:
    SpecialPieBleServer* parent = nullptr;
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
  };

  ServerCallbacks serverCallbacks;
};

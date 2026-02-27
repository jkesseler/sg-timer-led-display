/**
 * @file BLEDevice.h
 * @brief Unified BLE API stub for native testing.
 *
 * Provides type-compatible stubs for all BLE classes used by the firmware.
 * Methods are no-ops or return safe defaults — only protocol parsing
 * logic needs to work, not actual BLE communication.
 */
#pragma once

#include <string>
#include <cstring>
#include <functional>

// ── Forward declarations ────────────────────────────────────────
class BLEClient;
class BLERemoteService;
class BLERemoteCharacteristic;
class BLEAdvertisedDevice;
class BLEScan;
class BLEScanResults;

// ── BLEUUID ─────────────────────────────────────────────────────
class BLEUUID {
  std::string _uuid;
public:
  BLEUUID() = default;
  BLEUUID(const char* uuid) : _uuid(uuid ? uuid : "") {}
  BLEUUID(const std::string& uuid) : _uuid(uuid) {}
  std::string toString() const { return _uuid; }
  bool equals(const BLEUUID& other) const { return _uuid == other._uuid; }
  bool operator==(const BLEUUID& other) const { return equals(other); }
};

// ── BLEAddress ──────────────────────────────────────────────────
class BLEAddress {
  std::string _addr;
public:
  BLEAddress() : _addr("00:00:00:00:00:00") {}
  BLEAddress(const char* addr) : _addr(addr ? addr : "00:00:00:00:00:00") {}
  BLEAddress(const std::string& addr) : _addr(addr) {}
  std::string toString() const { return _addr; }
  bool operator==(const BLEAddress& other) const { return _addr == other._addr; }
};

// ── BLERemoteCharacteristic ─────────────────────────────────────
class BLERemoteCharacteristic {
public:
  bool canNotify() { return true; }
  bool canRead()   { return true; }

  void registerForNotify(
    void (*callback)(BLERemoteCharacteristic*, uint8_t*, size_t, bool)) {}

  std::string readValue() { return ""; }
  BLEUUID getUUID() { return BLEUUID(); }
};

// ── BLERemoteService ────────────────────────────────────────────
class BLERemoteService {
public:
  BLERemoteCharacteristic* getCharacteristic(const char* uuid) { return nullptr; }
  BLERemoteCharacteristic* getCharacteristic(const BLEUUID& uuid) { return nullptr; }
};

// ── BLEClient ───────────────────────────────────────────────────
class BLEClient {
  bool _connected = false;
public:
  bool connect(BLEAdvertisedDevice* device) { _connected = true; return true; }
  bool connect(BLEAddress addr) { _connected = true; return true; }
  void disconnect() { _connected = false; }
  bool isConnected() { return _connected; }
  BLERemoteService* getService(const char* uuid) { return nullptr; }
  BLERemoteService* getService(const BLEUUID& uuid) { return nullptr; }
};

// ── BLEScanResults ──────────────────────────────────────────────
class BLEScanResults {
public:
  int getCount() { return 0; }
  BLEAdvertisedDevice getDevice(int idx);
};

// ── BLEScan ─────────────────────────────────────────────────────
class BLEScan {
public:
  void setActiveScan(bool) {}
  void setInterval(uint16_t) {}
  void setWindow(uint16_t) {}
  bool start(uint32_t duration, void (*scanCompleteCB)(BLEScanResults), bool is_continue = false) {
    if (scanCompleteCB) {
      scanCompleteCB(BLEScanResults());
    }
    return true;
  }
  BLEScanResults start(uint32_t duration, bool is_continue = false) { return BLEScanResults(); }
  BLEScanResults getResults() { return BLEScanResults(); }
  void stop() {}
  void clearResults() {}
};

// ── BLEAdvertisedDevice ─────────────────────────────────────────
class BLEAdvertisedDevice {
  std::string _name;
  BLEAddress _addr;
  BLEUUID _serviceUuid;
  bool _hasName = false;
  bool _hasServiceUuid = false;
public:
  BLEAdvertisedDevice() = default;

  // Test helpers — set up device properties
  void setName(const char* name) { _name = name; _hasName = true; }
  void setAddress(const char* addr) { _addr = BLEAddress(addr); }
  void setServiceUUID(const char* uuid) { _serviceUuid = BLEUUID(uuid); _hasServiceUuid = true; }

  bool haveName() { return _hasName; }
  std::string getName() { return _name; }
  bool haveServiceUUID() { return _hasServiceUuid; }
  bool isAdvertisingService(const BLEUUID& uuid) { return _hasServiceUuid && _serviceUuid == uuid; }
  BLEAddress getAddress() { return _addr; }
};

// Deferred definition (needs BLEAdvertisedDevice to be complete)
inline BLEAdvertisedDevice BLEScanResults::getDevice(int idx) { return BLEAdvertisedDevice(); }

// ── BLEDevice (static API) ──────────────────────────────────────
class BLEDevice {
public:
  static void init(const char*) {}
  static BLEClient* createClient() { return new BLEClient(); }
  static BLEScan* getScan() {
    static BLEScan scan;
    return &scan;
  }
};

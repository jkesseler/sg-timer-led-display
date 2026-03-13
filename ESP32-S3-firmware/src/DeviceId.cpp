
#include "common.h"
#include "DeviceId.h"
#include "Logger.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_mac.h>

#define STORAGE_NAMESPACE "deviceData" // Max. 15 chars
#define STORAGE_KEY       "deviceId"
#define ID_LENGTH         6

// Global singleton instance - declared extern in DeviceId.h
DeviceId deviceId;

static Preferences prefs;

/**
 * @brief 64-bit FNV-1a hash
 */
static uint64_t fnv1a64(const uint8_t *data, size_t len)
{
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

DeviceId::DeviceId()
{
  // ID is not available until initialize() is called
}

void DeviceId::initialize()
{
  LOG_DEBUG("DeviceId", "Reading ID from flash...");
  _deviceId = readFromFlash();

  if (_deviceId == EMPTY_DEVICE_ID) {
    LOG_DEBUG("DeviceId", "No valid ID found in flash, generating new one");
    _deviceId = generateId();
    writeToFlash();
  }

  LOG_INFO("DeviceId", "Device ID: %s", _deviceId.c_str());
}

String DeviceId::get() const
{
  if (_deviceId.isEmpty() || _deviceId == EMPTY_DEVICE_ID) {
    LOG_ERROR("DeviceId", "get() called before initialize() - ID not ready");
  }
  return _deviceId;
}

String DeviceId::generateId()
{
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  uint32_t hwRandom = esp_random();

  // ---- Visible MAC portion (last 3 bytes) ----
  uint32_t macPart = ((uint32_t)mac[3] << 16) |
                     ((uint32_t)mac[4] << 8)  |
                     ((uint32_t)mac[5]);

  // ---- Entropy mix: full MAC + hardware random ----
  uint8_t buffer[6 + 4];
  memcpy(buffer,     mac,       6);
  memcpy(buffer + 6, &hwRandom, 4);

  uint64_t hash = fnv1a64(buffer, sizeof(buffer));

  // Base-32 alphabet: no O, I, 0, 1 to avoid visual ambiguity
  static const char   ALPHABET[]     = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  static const size_t ALPHABET_SIZE  = sizeof(ALPHABET) - 1;

  char out[ID_LENGTH + 1];

  // ---- Encode entropy portion (last 3 chars) ----
  uint32_t entropyPart = (uint32_t)(hash & 0x7FFF); // 15 bits
  for (int i = 5; i >= 3; --i) {
    out[i] = ALPHABET[entropyPart % ALPHABET_SIZE];
    entropyPart /= ALPHABET_SIZE;
  }

  // ---- Encode visible MAC portion (first 3 chars) ----
  for (int i = 2; i >= 0; --i) {
    out[i] = ALPHABET[macPart % ALPHABET_SIZE];
    macPart /= ALPHABET_SIZE;
  }

  out[ID_LENGTH] = '\0';

  return String(out);
}

String DeviceId::readFromFlash()
{
  String stored;
  prefs.begin(STORAGE_NAMESPACE, true);
  stored = prefs.getString(STORAGE_KEY, EMPTY_DEVICE_ID);
  prefs.end();
  LOG_DEBUG("DeviceId", "Read ID from flash: %s", stored.c_str());
  return stored;
}

void DeviceId::writeToFlash()
{
  prefs.begin(STORAGE_NAMESPACE, false);
  bool ok = prefs.putString(STORAGE_KEY, _deviceId);
  prefs.end();
  if (!ok) {
    LOG_ERROR("DeviceId", "Failed to write device ID to flash");
  }
}

void DeviceId::reset()
{
  prefs.begin(STORAGE_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  _deviceId = EMPTY_DEVICE_ID;
}
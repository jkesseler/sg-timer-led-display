#include "WiFiConfig.h"
#include "Logger.h"
#include <WiFiManager.h>
#include <WiFi.h>
#include "common.h"

// Static member initialization
bool WiFiConfig::wifiConnected = false;
unsigned long WiFiConfig::lastConnectionCheck = 0;

// Global WiFiManager instance (persistent across WiFiConfig function calls)
static WiFiManager wifiManager;
static bool wifiManagerInitialized = false;

void WiFiConfig::initialize() {
  if (wifiManagerInitialized) {
    return;
  }

  LOG_SYSTEM("Initializing WiFi Manager");

  // Configure WiFi Manager settings for non-blocking operation
  wifiManager.setConnectTimeout(WIFI_CONFIG_TIMEOUT);
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

  // CRITICAL: Set config portal to non-blocking mode
  // This ensures autoConnect() returns immediately without halting execution
  wifiManager.setConfigPortalBlocking(false);

  // Attempt to connect using saved credentials or start AP for configuration
  // Returns immediately (non-blocking)
  wifiManager.autoConnect(AP_SSID);

  wifiManagerInitialized = true;

  LOG_SYSTEM("WiFiManager initialized (non-blocking mode)");
  LOG_SYSTEM("If WiFi not configured, connect to AP: %s for captive portal", AP_SSID);
  LOG_SYSTEM("Portal will auto-open in browser or visit http://192.168.4.1 if needed");

  // Store initial state
  lastConnectionCheck = millis();
  wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (wifiConnected) {
    LOG_SYSTEM("WiFi already connected. IP: %s", WiFi.localIP().toString().c_str());
    WiFi.setTxPower((wifi_power_t)WIFI_TX_POWER);
  }
}

void WiFiConfig::update() {
  if (!wifiManagerInitialized) {
    return;
  }

  // CRITICAL: Call process() to handle WiFiManager's state machine
  // This is required for non-blocking mode to work properly
  wifiManager.process();

  unsigned long now = millis();

  // Check connection status periodically
  if (now - lastConnectionCheck < CONNECTION_CHECK_INTERVAL) {
    return;
  }

  lastConnectionCheck = now;

  bool isNowConnected = (WiFi.status() == WL_CONNECTED);

  // Log state changes
  if (isNowConnected && !wifiConnected) {
    wifiConnected = true;
    LOG_SYSTEM("WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
    WiFi.setTxPower((wifi_power_t)WIFI_TX_POWER);
    LOG_SYSTEM("WiFi TX power set to minimize BLE interference");
  } else if (!isNowConnected && wifiConnected) {
    wifiConnected = false;
    LOG_SYSTEM("WiFi disconnected");
  }
}

bool WiFiConfig::isConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

const char* WiFiConfig::getLocalIP() {
  static char ipBuffer[16];
  if (isConnected()) {
    WiFi.localIP().toString().toCharArray(ipBuffer, sizeof(ipBuffer));
    return ipBuffer;
  }
  return "0.0.0.0";
}

void WiFiConfig::startConfigPortal() {
  LOG_SYSTEM("Starting WiFi configuration portal");
  LOG_SYSTEM("Connect to AP: %s - captive portal will open automatically", AP_SSID);
  LOG_SYSTEM("If portal doesn't open, visit http://192.168.4.1");

  // Disconnect from current WiFi to start AP mode
  WiFi.disconnect(true);  // true = turn off WiFi radio
  delay(100);

  // Start config portal (blocking - use with caution)
  wifiManager.startConfigPortal(AP_SSID);

  LOG_SYSTEM("Configuration portal closed");
}

void WiFiConfig::resetWiFiSettings() {
  LOG_SYSTEM("Resetting WiFi settings");
  wifiManager.resetSettings();
  wifiConnected = false;
  ESP.restart();
}

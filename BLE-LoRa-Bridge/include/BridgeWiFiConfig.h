#pragma once

#include <WiFiManager.h>
#include "common.h"
#include "Logger.h"

/**
 * @brief WiFi configuration portal for the BLE-LoRa Bridge
 *
 * Same non-blocking WiFiManager pattern as ESP32-S3-firmware WiFiConfig,
 * but with bridge-specific parameters: deviceRole, timerType, outputMode,
 * and MQTT settings. Configuration is stored in NVS under "bridge-cfg".
 *
 * Configuration portal available at 192.168.4.1 when in AP mode.
 * AP SSID: "J.K. PewPew LoRa Bridge AP"
 */
class BridgeWiFiConfig {
private:
  static bool wifiConnected;
  static unsigned long lastConnectionCheck;

  // Runtime configuration values (NVS-backed)
  static char device_role[12];     // "transmitter" / "receiver"
  static char timer_type[24];      // "sg-timer" / "special-pie-m1a2plus" / "special-pie-m1a2f"
  static char output_mode[18];     // "mqtt" / "ble-special-pie"
  static char mqtt_server[41];
  static char mqtt_port[7];
  static char mqtt_user[41];
  static char mqtt_password[41];

  static constexpr unsigned long CONNECTION_CHECK_INTERVAL = 5000;
  static constexpr unsigned long WIFI_CONNECT_TIMEOUT = 60;
  static constexpr unsigned long WIFI_PORTAL_TIMEOUT = 0;
  static constexpr const char* AP_SSID = "J.K. PewPew LoRa Bridge AP";
  static constexpr int WIFI_TX_POWER = 80;

  // WiFiManager custom parameters (persistent while portal is active)
  static WiFiManagerParameter* customDeviceRole;
  static WiFiManagerParameter* customTimerType;
  static WiFiManagerParameter* customOutputMode;
  static WiFiManagerParameter* customMqttServer;
  static WiFiManagerParameter* customMqttPort;
  static WiFiManagerParameter* customMqttUser;
  static WiFiManagerParameter* customMqttPassword;

  static void loadConfiguration();
  static void saveConfiguration();

public:
  static void initialize();
  static bool isInitialized();
  static void update();
  static bool isConnected();
  static String getLocalIP();
  static void startConfigPortal();
  static void resetSettings();

  // Accessors
  static BridgeRole getDeviceRole();
  static ReceiverOutputMode getOutputMode();
  static const char* getTimerTypeString();

  // MQTT settings (Receiver MQTT mode)
  static const char* getMqttServer();
  static int getMqttPort();
  static const char* getMqttUser();
  static const char* getMqttPassword();

  // Needed by shared WiFiConfig-dependent code
  static const char* getStartupText() { return STARTUP_TEXT; }
  static int getTimerType() { return TIMER_TYPE; }
};

// WiFiConfig compatibility shim — shared ESP32-S3-firmware code (MqttManager.cpp)
// references WiFiConfig:: directly. These aliases redirect to BridgeWiFiConfig.
class WiFiConfig {
public:
  static void initialize()      { BridgeWiFiConfig::initialize(); }
  static bool isInitialized()   { return BridgeWiFiConfig::isInitialized(); }
  static void update()          { BridgeWiFiConfig::update(); }
  static bool isConnected()     { return BridgeWiFiConfig::isConnected(); }
  static String getLocalIP()    { return BridgeWiFiConfig::getLocalIP(); }

  static const char* getMqttServer()   { return BridgeWiFiConfig::getMqttServer(); }
  static int getMqttPort()             { return BridgeWiFiConfig::getMqttPort(); }
  static const char* getMqttUser()     { return BridgeWiFiConfig::getMqttUser(); }
  static const char* getMqttPassword() { return BridgeWiFiConfig::getMqttPassword(); }
  static int getTimerType()            { return BridgeWiFiConfig::getTimerType(); }
  static const char* getStartupText()  { return BridgeWiFiConfig::getStartupText(); }
};

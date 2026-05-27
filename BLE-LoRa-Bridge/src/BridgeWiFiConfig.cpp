#include "BridgeWiFiConfig.h"
#include <Preferences.h>
#include <WiFi.h>
#include "common.h"
#include "DeviceId.h"

namespace {
int sanitizeMqttPort(int port) {
  if (port <= 0 || port > 65535) return MQTT_BROKER_PORT;
  return port;
}

void copyIfProvided(char* dest, size_t destSize, const WiFiManagerParameter* param, const char* name, bool sensitive = false) {
  if (!param) return;
  const char* val = param->getValue();
  if (!val || val[0] == '\0') return;
  if (strncmp(dest, val, destSize - 1) == 0) return;
  strncpy(dest, val, destSize - 1);
  dest[destSize - 1] = '\0';
  LOG_SYSTEM("Portal value updated for %s: %s", name, sensitive ? "[redacted]" : dest);
}
}  // namespace

// Static member initialization
bool BridgeWiFiConfig::wifiConnected = false;
unsigned long BridgeWiFiConfig::lastConnectionCheck = 0;
char BridgeWiFiConfig::apSsid[52] = "J.K. PewPew Long Range Bridge AP";

char BridgeWiFiConfig::device_role[4]     = "0";
char BridgeWiFiConfig::output_mode[4]     = "0";
char BridgeWiFiConfig::mqtt_server[41]    = MQTT_BROKER_IP;
char BridgeWiFiConfig::mqtt_port[7]       = "1883";
char BridgeWiFiConfig::mqtt_user[41]      = MQTT_USER;
char BridgeWiFiConfig::mqtt_password[41]  = MQTT_PASSWORD;

WiFiManagerParameter* BridgeWiFiConfig::customDeviceRole   = nullptr;
WiFiManagerParameter* BridgeWiFiConfig::customOutputMode   = nullptr;
WiFiManagerParameter* BridgeWiFiConfig::customMqttServer   = nullptr;
WiFiManagerParameter* BridgeWiFiConfig::customMqttPort     = nullptr;
WiFiManagerParameter* BridgeWiFiConfig::customMqttUser     = nullptr;
WiFiManagerParameter* BridgeWiFiConfig::customMqttPassword = nullptr;

static WiFiManager wifiManager;
static bool wifiManagerInitialized = false;

bool BridgeWiFiConfig::isInitialized() {
  return wifiManagerInitialized;
}

void BridgeWiFiConfig::loadConfiguration() {
  Preferences prefs;
  prefs.begin("bridge-cfg", true);

  prefs.getString("device_role", "0").toCharArray(device_role, sizeof(device_role));
  prefs.getString("output_mode", "0").toCharArray(output_mode, sizeof(output_mode));
  prefs.getString("mqtt_server", MQTT_BROKER_IP).toCharArray(mqtt_server, sizeof(mqtt_server));
  prefs.getString("mqtt_port",   String(MQTT_BROKER_PORT)).toCharArray(mqtt_port, sizeof(mqtt_port));
  prefs.getString("mqtt_user",   MQTT_USER).toCharArray(mqtt_user, sizeof(mqtt_user));
  prefs.getString("mqtt_pass",   MQTT_PASSWORD).toCharArray(mqtt_password, sizeof(mqtt_password));

  prefs.end();

  LOG_SYSTEM("Bridge configuration loaded from NVS:");
  LOG_SYSTEM("  Role: %s", device_role);
  LOG_SYSTEM("  Output Mode: %s", output_mode);
  LOG_SYSTEM("  MQTT Server: %s", mqtt_server);
  LOG_SYSTEM("  MQTT Port: %s", mqtt_port);
}

void BridgeWiFiConfig::saveConfiguration() {
  LOG_SYSTEM("Saving bridge configuration to NVS");

  int port = sanitizeMqttPort(atoi(mqtt_port));
  snprintf(mqtt_port, sizeof(mqtt_port), "%d", port);

  Preferences prefs;
  prefs.begin("bridge-cfg", false);
  prefs.putString("device_role", device_role);
  prefs.putString("output_mode", output_mode);
  prefs.putString("mqtt_server", mqtt_server);
  prefs.putString("mqtt_port",   mqtt_port);
  prefs.putString("mqtt_user",   mqtt_user);
  prefs.putString("mqtt_pass",   mqtt_password);
  prefs.end();

  LOG_SYSTEM("Configuration saved");
}

void BridgeWiFiConfig::initialize() {
  if (wifiManagerInitialized) return;

  LOG_SYSTEM("Initializing Bridge WiFi Manager");
  snprintf(apSsid, sizeof(apSsid), "J.K. PewPew Long Range Bridge AP %s", deviceId.get().c_str());
  loadConfiguration();

  // Create custom parameters for the web portal
  // Descriptions guide the user on valid values
  customDeviceRole   = new WiFiManagerParameter("device_role",   "Role: 0=Lora Transmitter, 1=Lora Receiver",      device_role,   sizeof(device_role) - 1);
  customOutputMode   = new WiFiManagerParameter("output_mode",   "Rx. Output: 0=MQTT, 1=Emulate Special Pie device",    output_mode,   sizeof(output_mode) - 1);
  customMqttServer   = new WiFiManagerParameter("mqtt_server",   "MQTT Server",                          mqtt_server,   sizeof(mqtt_server) - 1);
  customMqttPort     = new WiFiManagerParameter("mqtt_port",     "MQTT Port",                            mqtt_port,     sizeof(mqtt_port) - 1);
  customMqttUser     = new WiFiManagerParameter("mqtt_user",     "MQTT User",                            mqtt_user,     sizeof(mqtt_user) - 1);
  customMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT Password",                        mqtt_password, sizeof(mqtt_password) - 1);

  wifiManager.addParameter(customDeviceRole);
  wifiManager.addParameter(customOutputMode);
  wifiManager.addParameter(customMqttServer);
  wifiManager.addParameter(customMqttPort);
  wifiManager.addParameter(customMqttUser);
  wifiManager.addParameter(customMqttPassword);

  wifiManager.setSaveParamsCallback([]() {
    copyIfProvided(device_role,   sizeof(device_role),   customDeviceRole,   "device_role");
    copyIfProvided(output_mode,   sizeof(output_mode),   customOutputMode,   "output_mode");
    copyIfProvided(mqtt_server,   sizeof(mqtt_server),   customMqttServer,   "mqtt_server");
    copyIfProvided(mqtt_port,     sizeof(mqtt_port),     customMqttPort,     "mqtt_port");
    copyIfProvided(mqtt_user,     sizeof(mqtt_user),     customMqttUser,     "mqtt_user");
    copyIfProvided(mqtt_password, sizeof(mqtt_password), customMqttPassword, "mqtt_password", true);
    saveConfiguration();
  });

  WiFi.setTxPower((wifi_power_t)WIFI_TX_POWER);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
  wifiManager.autoConnect(apSsid);

  wifiManagerInitialized = true;
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  LOG_SYSTEM("WiFi Manager initialized (connected=%s)", wifiConnected ? "yes" : "no");
}

void BridgeWiFiConfig::update() {
  if (!wifiManagerInitialized) return;

  wifiManager.process();

  unsigned long now = millis();
  if (now - lastConnectionCheck < CONNECTION_CHECK_INTERVAL) return;
  lastConnectionCheck = now;

  bool nowConnected = (WiFi.status() == WL_CONNECTED);
  if (nowConnected != wifiConnected) {
    wifiConnected = nowConnected;
    if (wifiConnected) {
      LOG_SYSTEM("WiFi connected: %s", WiFi.localIP().toString().c_str());
    } else {
      LOG_WARN("SYSTEM", "WiFi disconnected");
    }
  }
}

bool BridgeWiFiConfig::isConnected() {
  return wifiConnected;
}

String BridgeWiFiConfig::getLocalIP() {
  return WiFi.localIP().toString();
}

void BridgeWiFiConfig::startConfigPortal() {
  wifiManager.startConfigPortal(apSsid);
}

void BridgeWiFiConfig::resetSettings() {
  wifiManager.resetSettings();
  Preferences prefs;
  prefs.begin("bridge-cfg", false);
  prefs.clear();
  prefs.end();
  LOG_SYSTEM("All settings cleared — restarting");
  ESP.restart();
}

BridgeRole BridgeWiFiConfig::getDeviceRole() {
  if (strcmp(device_role, "1") == 0) return BridgeRole::RECEIVER;
  return BridgeRole::TRANSMITTER;
}

ReceiverOutputMode BridgeWiFiConfig::getOutputMode() {
  if (strcmp(output_mode, "1") == 0) {
    return ReceiverOutputMode::BLE_SPECIAL_PIE;
  }

  return ReceiverOutputMode::MQTT_OUTPUT;
}

const char* BridgeWiFiConfig::getMqttServer() {
  return mqtt_server;
}

int BridgeWiFiConfig::getMqttPort() {
  return sanitizeMqttPort(atoi(mqtt_port));
}

const char* BridgeWiFiConfig::getMqttUser() {
  return mqtt_user;
}

const char* BridgeWiFiConfig::getMqttPassword() {
  return mqtt_password;
}

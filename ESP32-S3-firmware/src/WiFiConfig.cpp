#include "WiFiConfig.h"
#include "Logger.h"
#include <Preferences.h>
#include <WiFi.h>
#include "common.h"

namespace {
int sanitizeMqttPort(int port) {
  if (port <= 0 || port > 65535) {
    return MQTT_BROKER_PORT;
  }
  return port;
}

void copyIfProvided(char* destination, size_t destinationSize, const WiFiManagerParameter* parameter, const char* fieldName) {
  if (!parameter) {
    LOG_DEBUG("SYSTEM", "Portal parameter missing for %s; keeping existing value", fieldName);
    return;
  }

  const char* value = parameter->getValue();
  if (!value || value[0] == '\0') {
    LOG_DEBUG("SYSTEM", "Portal value empty for %s; keeping existing value: %s", fieldName, destination);
    return;
  }

  // Verify value actually changed before logging update
  if (strncmp(destination, value, destinationSize - 1) == 0) {
    LOG_DEBUG("SYSTEM", "Portal value unchanged for %s: %s", fieldName, value);
    return;
  }

  strncpy(destination, value, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
  LOG_SYSTEM("Portal value updated for %s: %s", fieldName, destination);
}

void copyOrClear(char* destination, size_t destinationSize, const WiFiManagerParameter* parameter, const char* fieldName) {
  if (!parameter) {
    LOG_DEBUG("SYSTEM", "Portal parameter missing for %s; keeping existing value", fieldName);
    return;
  }

  const char* value = parameter->getValue();
  if (!value) {
    LOG_DEBUG("SYSTEM", "Portal value missing for %s; keeping existing value: %s", fieldName, destination);
    return;
  }

  if (strncmp(destination, value, destinationSize - 1) == 0) {
    LOG_DEBUG("SYSTEM", "Portal value unchanged for %s: %s", fieldName, value);
    return;
  }

  strncpy(destination, value, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
  if (destination[0] == '\0') {
    LOG_SYSTEM("Portal value cleared for %s", fieldName);
  } else {
    LOG_SYSTEM("Portal value updated for %s: %s", fieldName, destination);
  }
}
}

// Static member initialization
bool WiFiConfig::wifiConnected = false;
unsigned long WiFiConfig::lastConnectionCheck = 0;

// Runtime configuration values (loaded from Preferences)
char WiFiConfig::mqtt_server[41] = MQTT_BROKER_IP;
char WiFiConfig::mqtt_port[7] = "1883";
char WiFiConfig::mqtt_user[41] = MQTT_USER;
char WiFiConfig::mqtt_password[41] = MQTT_PASSWORD;
char WiFiConfig::timer_type[7] = "";
char WiFiConfig::startup_text[41] = STARTUP_TEXT;

// Persistent WiFiManager custom parameters (required for non-blocking portal)
WiFiManagerParameter* WiFiConfig::customMqttServer = nullptr;
WiFiManagerParameter* WiFiConfig::customMqttPort = nullptr;
WiFiManagerParameter* WiFiConfig::customMqttUser = nullptr;
WiFiManagerParameter* WiFiConfig::customMqttPassword = nullptr;
WiFiManagerParameter* WiFiConfig::customTimerType = nullptr;
WiFiManagerParameter* WiFiConfig::customStartupText = nullptr;

// Global WiFiManager instance (persistent across WiFiConfig function calls)
static WiFiManager wifiManager;
static bool wifiManagerInitialized = false;

bool WiFiConfig::isInitialized() {
  return wifiManagerInitialized;
}

void WiFiConfig::loadConfiguration() {
  Preferences prefs;
  prefs.begin("wifi-config", /*readOnly=*/true);

  prefs.getString("mqtt_server", MQTT_BROKER_IP).toCharArray(mqtt_server, sizeof(mqtt_server));
  prefs.getString("mqtt_port",   String(MQTT_BROKER_PORT)).toCharArray(mqtt_port, sizeof(mqtt_port));
  prefs.getString("mqtt_user",   MQTT_USER).toCharArray(mqtt_user, sizeof(mqtt_user));
  prefs.getString("mqtt_pass",   MQTT_PASSWORD).toCharArray(mqtt_password, sizeof(mqtt_password));
  snprintf(timer_type, sizeof(timer_type), "%d", prefs.getInt("timer_type", TIMER_TYPE));
  prefs.getString("startup_text", STARTUP_TEXT).toCharArray(startup_text, sizeof(startup_text));

  prefs.end();

  LOG_SYSTEM("Configuration loaded from NVS:");
  LOG_SYSTEM("  MQTT Server: %s", mqtt_server);
  LOG_SYSTEM("  MQTT Port: %s", mqtt_port);
  LOG_SYSTEM("  MQTT User: %s", mqtt_user[0] ? mqtt_user : "(empty)");
  LOG_SYSTEM("  Timer Type: %s", timer_type);
  LOG_SYSTEM("  Startup Text: %s", startup_text);
}

void WiFiConfig::saveConfiguration() {
  LOG_SYSTEM("Saving configuration to NVS");

  // Sanitize port before saving
  int port = sanitizeMqttPort(atoi(mqtt_port));
  snprintf(mqtt_port, sizeof(mqtt_port), "%d", port);

  Preferences prefs;
  prefs.begin("wifi-config", /*readOnly=*/false);
  prefs.putString("mqtt_server",  mqtt_server);
  prefs.putString("mqtt_port",    mqtt_port);
  prefs.putString("mqtt_user",    mqtt_user);
  prefs.putString("mqtt_pass",    mqtt_password);
  prefs.putInt(   "timer_type",   atoi(timer_type));
  prefs.putString("startup_text", startup_text);
  prefs.end();

  LOG_SYSTEM("Configuration saved to NVS");
}

void WiFiConfig::initialize() {
  if (wifiManagerInitialized) {
    return;
  }

  LOG_SYSTEM("Initializing WiFi Manager");

  // Load configuration from NVS
  loadConfiguration();

  // Create WiFiManager custom parameters with current values from NVS
  // WARNING: Must recreate each time to ensure fresh parameter objects for non-blocking mode
  // Old objects are intentionally leaked (one-time cost) to simplify pointer management
  customMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT Server", mqtt_server, 40);
  customMqttPort = new WiFiManagerParameter("mqtt_port", "MQTT Port", mqtt_port, 6);
  customMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT User", mqtt_user, 40);
  customMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT Password", mqtt_password, 40);
  customTimerType = new WiFiManagerParameter("timer_type", "Timer Type (1=BLE, 2=MQTT)", timer_type, 6);
  customStartupText = new WiFiManagerParameter("startup_text", "Startup Text", startup_text, 40);

  // Add parameters to WiFiManager
  wifiManager.addParameter(customMqttServer);
  wifiManager.addParameter(customMqttPort);
  wifiManager.addParameter(customMqttUser);
  wifiManager.addParameter(customMqttPassword);
  wifiManager.addParameter(customTimerType);
  wifiManager.addParameter(customStartupText);

  // Set save params callback — fires when the user clicks Save on the custom params form
  wifiManager.setSaveParamsCallback([]() {
    LOG_SYSTEM("WiFiManager params saved — persisting to NVS");
    copyOrClear(mqtt_server, sizeof(mqtt_server), customMqttServer, "mqtt_server");
    copyOrClear(mqtt_port,   sizeof(mqtt_port),   customMqttPort,   "mqtt_port");
    int sanitizedPort = sanitizeMqttPort(atoi(mqtt_port));
    snprintf(mqtt_port, sizeof(mqtt_port), "%d", sanitizedPort);
    copyOrClear   (mqtt_user,     sizeof(mqtt_user),     customMqttUser,     "mqtt_user");
    copyOrClear   (mqtt_password, sizeof(mqtt_password), customMqttPassword, "mqtt_password");
    copyIfProvided(timer_type,    sizeof(timer_type),    customTimerType,    "timer_type");
    copyIfProvided(startup_text,  sizeof(startup_text),  customStartupText,  "startup_text");
    WiFiConfig::saveConfiguration();
  });

  // Configure WiFi Manager settings for non-blocking operation
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);

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

    // Log network diagnostics for debugging MQTT connection issues
    LOG_SYSTEM("WiFi Network Info:");
    LOG_SYSTEM("  SSID: %s", WiFi.SSID().c_str());
    LOG_SYSTEM("  IP Address: %s", WiFi.localIP().toString().c_str());
    LOG_SYSTEM("  Gateway: %s", WiFi.gatewayIP().toString().c_str());
    LOG_SYSTEM("  DNS: %s", WiFi.dnsIP().toString().c_str());
    LOG_SYSTEM("  Subnet: %s", WiFi.subnetMask().toString().c_str());
    LOG_SYSTEM("  RSSI: %d dBm", WiFi.RSSI());
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

    // Log network diagnostics for debugging MQTT connection issues
    LOG_SYSTEM("WiFi Network Info:");
    LOG_SYSTEM("  SSID: %s", WiFi.SSID().c_str());
    LOG_SYSTEM("  Gateway: %s", WiFi.gatewayIP().toString().c_str());
    LOG_SYSTEM("  DNS: %s", WiFi.dnsIP().toString().c_str());
    LOG_SYSTEM("  Subnet: %s", WiFi.subnetMask().toString().c_str());
    LOG_SYSTEM("  RSSI: %d dBm", WiFi.RSSI());
  } else if (!isNowConnected && wifiConnected) {
    wifiConnected = false;
    LOG_SYSTEM("WiFi disconnected");
  }
}

bool WiFiConfig::isConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

String WiFiConfig::getLocalIP() {
  if (isConnected()) {
    return WiFi.localIP().toString();
  }
  return String("0.0.0.0");
}

void WiFiConfig::startConfigPortal() {
  LOG_SYSTEM("Starting WiFi configuration portal");
  LOG_SYSTEM("Connect to AP: %s - captive portal will open automatically", AP_SSID);
  LOG_SYSTEM("If portal doesn't open, visit http://192.168.4.1");

  // Disconnect from current WiFi to start AP mode
  WiFi.disconnect(true);  // true = turn off WiFi radio
  delay(100);

  // CRITICAL: Recreate parameters with current buffer values before portal
  // This ensures portal displays latest values from NVS and receives fresh parameters
  // for user submissions. Non-blocking mode requires fresh parameter objects.
  customMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT Server", mqtt_server, 40);
  customMqttPort = new WiFiManagerParameter("mqtt_port", "MQTT Port", mqtt_port, 6);
  customMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT User", mqtt_user, 40);
  customMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT Password", mqtt_password, 40);
  customTimerType = new WiFiManagerParameter("timer_type", "Timer Type (1=BLE, 2=MQTT)", timer_type, 6);
  customStartupText = new WiFiManagerParameter("startup_text", "Startup Text", startup_text, 40);

  // Re-add parameters to portal (old ones are replaced)
  wifiManager.addParameter(customMqttServer);
  wifiManager.addParameter(customMqttPort);
  wifiManager.addParameter(customMqttUser);
  wifiManager.addParameter(customMqttPassword);
  wifiManager.addParameter(customTimerType);
  wifiManager.addParameter(customStartupText);

  // Start config portal (blocking)
  // setSaveParamsCallback (set in initialize()) fires on Save and persists to NVS
  wifiManager.startConfigPortal(AP_SSID);

  LOG_SYSTEM("Configuration portal closed");
}

void WiFiConfig::resetWiFiSettings() {
  LOG_SYSTEM("Resetting WiFi settings");
  wifiManager.resetSettings();
  wifiConnected = false;
  ESP.restart();
}

const char* WiFiConfig::getMqttServer() {
  return mqtt_server;
}

int WiFiConfig::getMqttPort() {
  return sanitizeMqttPort(atoi(mqtt_port));
}

const char* WiFiConfig::getMqttUser() {
  return mqtt_user;
}

const char* WiFiConfig::getMqttPassword() {
  return mqtt_password;
}

int WiFiConfig::getTimerType() {
  int type = atoi(timer_type);
  return (type > 0) ? type : TIMER_TYPE;
}

const char* WiFiConfig::getStartupText() {
  return startup_text[0] ? startup_text : STARTUP_TEXT;
}

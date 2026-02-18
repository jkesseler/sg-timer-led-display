#include "WiFiConfig.h"
#include "Logger.h"
#include <WiFi.h>
#include "common.h"

namespace {
int sanitizeMqttPort(int port) {
  if (port <= 0 || port > 65535) {
    return MQTT_BROKER_PORT;
  }
  return port;
}
}

// Static member initialization
bool WiFiConfig::wifiConnected = false;
unsigned long WiFiConfig::lastConnectionCheck = 0;
Preferences WiFiConfig::preferences;
bool WiFiConfig::shouldSaveConfig = false;

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

void WiFiConfig::saveConfigCallback() {
  shouldSaveConfig = true;
}

void WiFiConfig::loadConfiguration() {
  preferences.begin("wifi-config", false);

  // Load MQTT server
  String savedMqttServer = preferences.getString("mqtt_server", MQTT_BROKER_IP);
  savedMqttServer.toCharArray(mqtt_server, sizeof(mqtt_server));

  // Load MQTT port
  int savedMqttPort = sanitizeMqttPort(preferences.getInt("mqtt_port", MQTT_BROKER_PORT));
  snprintf(mqtt_port, sizeof(mqtt_port), "%d", savedMqttPort);

  // Load MQTT user
  String savedMqttUser = preferences.getString("mqtt_user", MQTT_USER);
  savedMqttUser.toCharArray(mqtt_user, sizeof(mqtt_user));

  // Load MQTT password
  String savedMqttPassword = preferences.getString("mqtt_password", MQTT_PASSWORD);
  savedMqttPassword.toCharArray(mqtt_password, sizeof(mqtt_password));

  // Load timer type
  int savedTimerType = preferences.getInt("timer_type", TIMER_TYPE);
  snprintf(timer_type, sizeof(timer_type), "%d", savedTimerType);

  // Load startup text
  String savedStartupText = preferences.getString("startup_text", STARTUP_TEXT);
  savedStartupText.toCharArray(startup_text, sizeof(startup_text));

  preferences.end();

  LOG_SYSTEM("Configuration loaded from NVS:");
  LOG_SYSTEM("  MQTT Server: %s", mqtt_server);
  LOG_SYSTEM("  MQTT Port: %s", mqtt_port);
  LOG_SYSTEM("  MQTT User: %s", mqtt_user[0] ? mqtt_user : "(empty)");
  LOG_SYSTEM("  Timer Type: %s", timer_type);
  LOG_SYSTEM("  Startup Text: %s", startup_text);
}

void WiFiConfig::saveConfiguration() {
  preferences.begin("wifi-config", false);

  preferences.putString("mqtt_server", mqtt_server);
  int port = sanitizeMqttPort(atoi(mqtt_port));
  snprintf(mqtt_port, sizeof(mqtt_port), "%d", port);
  preferences.putInt("mqtt_port", port);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_password", mqtt_password);
  preferences.putInt("timer_type", atoi(timer_type));
  preferences.putString("startup_text", startup_text);

  preferences.end();

  LOG_SYSTEM("Configuration saved to NVS");
}

void WiFiConfig::initialize() {
  if (wifiManagerInitialized) {
    return;
  }

  LOG_SYSTEM("Initializing WiFi Manager");

  // Load configuration from NVS
  loadConfiguration();

  // Reset save flag
  shouldSaveConfig = false;

  // Create WiFiManager custom parameters (persist for portal lifetime)
  if (!customMqttServer) {
    customMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT Server", mqtt_server, 40);
    customMqttPort = new WiFiManagerParameter("mqtt_port", "MQTT Port", mqtt_port, 6);
    customMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT User", mqtt_user, 40);
    customMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT Password", mqtt_password, 40);
    customTimerType = new WiFiManagerParameter("timer_type", "Timer Type (1=BLE, 2=MQTT)", timer_type, 6);
    customStartupText = new WiFiManagerParameter("startup_text", "Startup Text", startup_text, 40);
  }

  // Add parameters to WiFiManager
  wifiManager.addParameter(customMqttServer);
  wifiManager.addParameter(customMqttPort);
  wifiManager.addParameter(customMqttUser);
  wifiManager.addParameter(customMqttPassword);
  wifiManager.addParameter(customTimerType);
  wifiManager.addParameter(customStartupText);

  // Set save config callback
  wifiManager.setSaveConfigCallback([]() {
    WiFiConfig::saveConfigCallback();
  });

  // Configure WiFi Manager settings for non-blocking operation
  wifiManager.setConnectTimeout(WIFI_CONFIG_TIMEOUT);
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

  // CRITICAL: Set config portal to non-blocking mode
  // This ensures autoConnect() returns immediately without halting execution
  wifiManager.setConfigPortalBlocking(false);

  // Attempt to connect using saved credentials or start AP for configuration
  // Returns immediately (non-blocking)
  wifiManager.autoConnect(AP_SSID);

  // Read parameter values after autoConnect
  strncpy(mqtt_server, customMqttServer->getValue(), sizeof(mqtt_server) - 1);
  mqtt_server[sizeof(mqtt_server) - 1] = '\0';
  strncpy(mqtt_port, customMqttPort->getValue(), sizeof(mqtt_port) - 1);
  mqtt_port[sizeof(mqtt_port) - 1] = '\0';
  int sanitizedPort = sanitizeMqttPort(atoi(mqtt_port));
  snprintf(mqtt_port, sizeof(mqtt_port), "%d", sanitizedPort);
  strncpy(mqtt_user, customMqttUser->getValue(), sizeof(mqtt_user) - 1);
  mqtt_user[sizeof(mqtt_user) - 1] = '\0';
  strncpy(mqtt_password, customMqttPassword->getValue(), sizeof(mqtt_password) - 1);
  mqtt_password[sizeof(mqtt_password) - 1] = '\0';
  strncpy(timer_type, customTimerType->getValue(), sizeof(timer_type) - 1);
  timer_type[sizeof(timer_type) - 1] = '\0';
  strncpy(startup_text, customStartupText->getValue(), sizeof(startup_text) - 1);
  startup_text[sizeof(startup_text) - 1] = '\0';

  // Save configuration if requested
  if (shouldSaveConfig) {
    saveConfiguration();
    shouldSaveConfig = false;
  }

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

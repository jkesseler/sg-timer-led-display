/**
 * @file wifi-config.cpp
 * @brief Config tool — opens a WiFi AP portal to edit and persist MQTT/display settings.
 *
 * Usage:
 *   pio run -e tools-wifi-config -t upload && pio device monitor
 *
 * Connect to WiFi AP "PewPewTimer-Config", open 192.168.4.1, edit fields, Save.
 * Values are written to NVS and read back by the main firmware on next boot.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>

static constexpr const char* NVS_NS = "wifi-config";

void setup() {
  Serial.begin(115200);
  delay(500);

  // Load current values from NVS to pre-fill the form
  char mqtt_server[64], mqtt_port[8], mqtt_user[64], mqtt_password[64], startup_text[64];
  {
    Preferences prefs;
    prefs.begin(NVS_NS, /*readOnly=*/true);
    prefs.getString("mqtt_server",  "").toCharArray(mqtt_server,   sizeof(mqtt_server));
    prefs.getString("mqtt_port",    "1883").toCharArray(mqtt_port, sizeof(mqtt_port));
    prefs.getString("mqtt_user",    "").toCharArray(mqtt_user,     sizeof(mqtt_user));
    prefs.getString("mqtt_pass",    "").toCharArray(mqtt_password, sizeof(mqtt_password));
    prefs.getString("startup_text", "PEW PEW").toCharArray(startup_text, sizeof(startup_text));
    prefs.end();
  }

  Serial.printf("[NVS] server=%s port=%s user=%s startup=%s\n",
    mqtt_server, mqtt_port, mqtt_user[0] ? mqtt_user : "(empty)", startup_text);

  WiFiManagerParameter pServer  ("mqtt_server",   "MQTT Server",   mqtt_server,   63);
  WiFiManagerParameter pPort    ("mqtt_port",     "MQTT Port",     mqtt_port,      7);
  WiFiManagerParameter pUser    ("mqtt_user",     "MQTT User",     mqtt_user,     63);
  WiFiManagerParameter pPassword("mqtt_password", "MQTT Password", mqtt_password, 63);
  WiFiManagerParameter pStartup ("startup_text",  "Startup Text",  startup_text,  63);

  WiFiManager wm;
  wm.addParameter(&pServer);
  wm.addParameter(&pPort);
  wm.addParameter(&pUser);
  wm.addParameter(&pPassword);
  wm.addParameter(&pStartup);

  // Fires when the user clicks Save on the params form
  wm.setSaveParamsCallback([&]() {
    Preferences prefs;
    prefs.begin(NVS_NS, /*readOnly=*/false);
    prefs.putString("mqtt_server",  pServer  .getValue());
    prefs.putString("mqtt_port",    pPort    .getValue());
    prefs.putString("mqtt_user",    pUser    .getValue());
    prefs.putString("mqtt_pass",    pPassword.getValue());
    prefs.putString("startup_text", pStartup .getValue());
    prefs.end();
    Serial.printf("[NVS] Saved — server=%s port=%s user=%s startup=%s\n",
      pServer.getValue(), pPort.getValue(),
      pUser.getValue()[0] ? pUser.getValue() : "(empty)",
      pStartup.getValue());
  });

  // Always open the AP portal — this is a config tool, not an app
  Serial.println("[WiFi] Opening config portal at 192.168.4.1 ...");
  wm.startConfigPortal("PewPewTimer-Config");

  Serial.println("[DONE] Portal closed.");
}

void loop() {}

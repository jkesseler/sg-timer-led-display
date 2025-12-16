#pragma once

#include "ITimerDevice.h"
#include "Logger.h"
#include "WiFiConfig.h"
#include <memory>

/**
 * @brief Manages Wi-Fi connectivity and MQTT publishing
 *
 * Bridges BLE timer events to MQTT topics for PWA display consumption.
 * WiFi connectivity is managed by WiFiConfig class (non-blocking).
 * MQTT connection is established on-demand when WiFi is available.
 */
class MqttManager {
private:
  bool isConnected;
  unsigned long lastMqttCheck;

  // Connection management
  void ensureMqttConnected();
  void disconnectMqtt();

  // Helper methods
  void publishJson(const char* topic, const char* jsonPayload);

public:
  MqttManager();
  ~MqttManager();

  // Lifecycle
  bool initialize();
  void update();  // Called in main loop to check connection status
  bool isHealthy() const { return isConnected && WiFiConfig::isConnected(); }

  // Event publishers - called by TimerApplication
  void publishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel);
  void publishDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion);
  void publishSessionStarted(uint32_t sessionId, uint16_t startDelaySeconds);
  void publishSessionStopped(uint32_t sessionId, uint16_t totalShots);
  void publishSessionSuspended(uint32_t sessionId);
  void publishSessionResumed(uint32_t sessionId);
  void publishShotDetected(const NormalizedShotData& shotData);
  void publishCountdownComplete(uint32_t sessionId);

  // Settings/status
  void reconnect();
  const char* getMqttClientId() const;
};

#pragma once

/**
 * @brief Manages Wi-Fi configuration using WiFiManager
 *
 * Provides non-blocking Wi-Fi setup with fallback to AP mode for configuration.
 * The timer application continues to work even if Wi-Fi is unavailable.
 */
class WiFiConfig {
private:
  static bool wifiConnected;
  static unsigned long lastConnectionCheck;

  // Configuration constants
  static constexpr unsigned long CONNECTION_CHECK_INTERVAL = 5000;  // Check every 5 seconds
  static constexpr unsigned long WIFI_CONFIG_TIMEOUT = 60;          // WiFiManager portal timeout (seconds)
  static constexpr const char* AP_SSID = "J.K. PewPewTimer AP";      // Access point name
  static constexpr int WIFI_TX_POWER = 80;                          // TX power (0-84)

public:
  /**
   * @brief Initialize WiFi with WiFiManager
   * Non-blocking - returns immediately and manages connection in background
   * Should only be called after a BLE device has successfully connected
   */
  static void initialize();

  /**
   * @brief Check if WiFi initialization has been started
   */
  static bool isInitialized();

  /**
   * @brief Update WiFi connection status
   * Call this regularly from main loop
   */
  static void update();

  /**
   * @brief Check if WiFi is currently connected
   */
  static bool isConnected();

  /**
   * @brief Get the local IP address as string
   */
  static const char* getLocalIP();

  /**
   * @brief Trigger WiFi manager portal (for reconfiguration)
   * Portal will be available at 192.168.4.1 when in AP mode
   */
  static void startConfigPortal();

  /**
   * @brief Reset stored WiFi credentials and restart
   */
  static void resetWiFiSettings();
};

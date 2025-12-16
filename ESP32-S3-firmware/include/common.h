#ifndef COMMON_H
#define COMMON_H

// =============================================================================
// Hardware Pin Definitions
// =============================================================================

// Potentiometer configuration for brightness control
#define POTENTIOMETER_PIN A0        // ADC pin for potentiometer (GPIO1 on ESP32-S3)

// Button configuration for manual reset
#define RESET_BUTTON_PIN 4          // GPIO pin for reset button
#define BUTTON_DEBOUNCE_MS 50       // Button debounce delay

// =============================================================================
// ADC Configuration
// =============================================================================

#define POTENTIOMETER_MAX_VALUE 4095  // 12-bit ADC resolution (0-4095)
#define ADC_RESOLUTION 12             // ADC resolution in bits

// =============================================================================
// Display Configuration
// =============================================================================

// HUB75 LED Panel Configuration
// Two 64x32 panels chained = 128x32 total
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define PANEL_CHAIN 2

// Display brightness settings
#define DEFAULT_BRIGHTNESS 128       // Default brightness (0-255)
#define MIN_BRIGHTNESS 10            // Minimum brightness to ensure display is always visible
#define MAX_BRIGHTNESS 255           // Maximum brightness

// Display scrolling settings
#define MARQUEE_SCROLL_GAP_PIXELS 60 // Gap between repeated text in marquee scroll

// =============================================================================
// Timing Configuration
// =============================================================================

#define BRIGHTNESS_UPDATE_INTERVAL 200  // Update brightness every 200ms (in milliseconds)
#define MAIN_LOOP_DELAY 10              // Main loop delay in milliseconds

// Startup message delay - shorter for debug builds to speed up development
#ifdef DEBUG_BUILD
  #define STARTUP_MESSAGE_DELAY 2000   // 2 seconds for debug builds
#else
  #define STARTUP_MESSAGE_DELAY 20000  // 20 seconds for production builds
#endif

// =============================================================================
// Serial Communication
// =============================================================================

#define SERIAL_BAUD_RATE 115200

// =============================================================================
// BLE Configuration
// =============================================================================

#define BLE_DEVICE_NAME "ESP32-S3 Shot Timer Bridge"

// =============================================================================
// System Configuration
// =============================================================================

// =============================================================================
// System Configuration
// =============================================================================

#define BRIGHTNESS_CHANGE_THRESHOLD 2   // Minimum change required to update brightness (reduces flickering)

// =============================================================================
// BLE Configuration Constants
// =============================================================================

#define BLE_SCAN_DURATION 10           // BLE scan duration in seconds
#define BLE_RECONNECT_INTERVAL 5000    // Reconnection attempt interval in milliseconds
#define BLE_SCAN_INTERVAL 1349         // BLE scan interval
#define BLE_SCAN_WINDOW 449            // BLE scan window
#define BLE_CONNECTION_DELAY_MS 2000   // Delay before connection attempt to stabilize BLE stack
#define BLE_HEARTBEAT_INTERVAL_MS 30000 // Heartbeat log interval in milliseconds
#define BLE_SCAN_RETRY_INTERVAL_MS 5000 // Minimum interval between scan attempts


// =============================================================================
// MQTT Configuration
// =============================================================================

// MQTT Broker settings
// TODO: add Settings and a "settings page" to configure these at runtime
#define MQTT_BROKER_IP "192.168.1.198"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "j.k.pewpew-timer-bridge"
#define MQTT_CHECK_INTERVAL 5000        // Check MQTT connection every 5 seconds
#define MQTT_RECONNECT_INTERVAL 5000   // Reconnection attempt interval in milliseconds

// Note: If your MQTT broker requires authentication, uncomment and set these:
// #define MQTT_USER "username"
// #define MQTT_PASSWORD "password"

// =============================================================================
// Radio Coexistence Configuration (BLE/WiFi Sharing 2.4GHz Antenna)
// =============================================================================

// Enable radio coexistence prioritization (recommended for shot timer)
#define ENABLE_RADIO_COEXISTENCE 1
// Prioritization: 0 = No preference, 1 = Prefer BLE (recommended)
#define RADIO_COEXISTENCE_PREFERENCE 1

// Event queue size for async MQTT publishing
// Larger = more buffering but more memory, Smaller = less latency but risk of drops
#define MQTT_EVENT_QUEUE_MAX_SIZE 10

// BLE-only window duration (ms) after each BLE event
// This prevents WiFi from interfering with BLE reception
#define BLE_WINDOW_MS 5

// =============================================================================
// Protocol Constants
// =============================================================================

#define MAX_SHOTS_PER_SESSION 100      // Maximum shots to read from shot list
#define SHOT_LIST_READ_DELAY 10        // Delay between shot list reads in milliseconds

#define STARTUP_TEXT "J.K. PewPew Timer"

#endif // COMMON_H
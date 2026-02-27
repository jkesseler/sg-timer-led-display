#ifndef COMMON_H
#define COMMON_H

// =============================================================================
// Connection Type Configuration
// =============================================================================
#define TIMER_TYPE_BLE 1   // Timer messages come from BLE device directly (S.G. Timer or Special Pie Timer)
#define TIMER_TYPE_MQTT 2  // Timer messages come via MQTT
#define TIMER_TYPE TIMER_TYPE_BLE // Set the timer type for this build

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
#define MAIN_LOOP_DELAY 10              // Main loop delay in milliseconds

// Startup message delay - shorter for debug builds to speed up development
#ifdef DEBUG_BUILD
  #define STARTUP_MESSAGE_DELAY 1000   // 1 second  for debug builds
#else
  #define STARTUP_MESSAGE_DELAY 5000  // 5 seconds for production builds
#endif

// =============================================================================
// Serial Communication
// =============================================================================

#define SERIAL_BAUD_RATE 115200

// =============================================================================
// System Configuration
// =============================================================================

#define BRIGHTNESS_CHANGE_THRESHOLD 2   // Minimum change required to update brightness (reduces flickering)

// =============================================================================
// BLE Configuration Constants
// =============================================================================
#define BLE_DEVICE_NAME "J.K. PewPew Timer Bridge"
#define BLE_SCAN_DURATION 10            // BLE scan duration in seconds
#define BLE_RECONNECT_INTERVAL 5000     // Reconnection attempt interval in milliseconds
#define BLE_SCAN_INTERVAL 1349          // BLE scan interval
#define BLE_SCAN_WINDOW 449             // BLE scan window
#define BLE_CONNECTION_DELAY_MS 2000    // Delay before connection attempt to stabilize BLE stack
#define BLE_HEARTBEAT_INTERVAL_MS 30000 // Heartbeat log interval in milliseconds
#define BLE_SCAN_RETRY_INTERVAL_MS 5000 // Minimum interval between scan attempts


// =============================================================================
// MQTT Configuration
// =============================================================================

// MQTT Broker settings (defaults - can be configured at runtime via WiFiConfig)
#define MQTT_BROKER_IP ""
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "j.k.pewpew-timer-bridge"
#define MQTT_USER ""
#define MQTT_PASSWORD ""

// =============================================================================
// Protocol Constants
// =============================================================================

#define MAX_SHOTS_PER_SESSION 100      // Maximum shots to read from shot list
#define SHOT_LIST_READ_DELAY 10        // Delay between shot list reads in milliseconds

#define STARTUP_TEXT "J.K. PewPew Timer"

#define EMPTY_DEVICE_ID "000000" // Fallback ID if flash read fails

#endif // COMMON_H
#ifndef COMMON_H
#define COMMON_H

// =============================================================================
// Development Configuration
// =============================================================================

// Uncomment to use the SG Timer Simulator instead of real device
// #define USE_SIMULATOR

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

// =============================================================================
// Timing Configuration
// =============================================================================

#define BRIGHTNESS_UPDATE_INTERVAL 200  // Update brightness every 200ms (in milliseconds)
#define MAIN_LOOP_DELAY 100             // Main loop delay in milliseconds
#define STARTUP_DELAY 1000              // Initial startup delay
#define STARTUP_MESSAGE_DELAY 2000      // How long to show startup message

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

// =============================================================================
// Protocol Constants
// =============================================================================

#define MAX_SHOTS_PER_SESSION 100      // Maximum shots to read from shot list
#define SHOT_LIST_READ_DELAY 10        // Delay between shot list reads in milliseconds

#endif // COMMON_H
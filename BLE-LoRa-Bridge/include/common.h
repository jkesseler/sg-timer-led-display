#ifndef COMMON_H
#define COMMON_H

#include <cstdint>

// =============================================================================
// BLE-LoRa Bridge — LilyGo LoRa32 T3 v1.6.1 Hardware & Application Constants
// =============================================================================

// =============================================================================
// Connection Type Configuration (shared with ESP32-S3-firmware)
// =============================================================================
#define TIMER_TYPE_BLE 1
#define TIMER_TYPE_MQTT 2
#define TIMER_TYPE TIMER_TYPE_BLE

// =============================================================================
// Device Role (configured at runtime via web portal, stored in NVS)
// =============================================================================
enum class BridgeRole : uint8_t {
  TRANSMITTER = 0,  // BLE → LoRa
  RECEIVER    = 1   // LoRa → MQTT  or  LoRa → BLE (Special Pie)
};

enum class ReceiverOutputMode : uint8_t {
  MQTT_OUTPUT        = 0,  // Publish to MQTT broker (same topics as ESP32-S3-firmware)
  BLE_SPECIAL_PIE    = 1   // Re-transmit as Special Pie M1A2+ BLE peripheral
};

// =============================================================================
// LoRa Radio Configuration — SX1276, 868 MHz EU band
// Short-range / high-throughput profile (≤500 m line-of-sight)
// Air-time per 42-byte shot packet: ~12 ms @ SF7/BW500
// =============================================================================
#define LORA_FREQUENCY        868E6    // Hz — EU 868 MHz ISM band
#define LORA_SPREADING_FACTOR 7        // SF7 — fastest, shortest range
#define LORA_BANDWIDTH        500E3    // 500 kHz — widest SX1276 BW
#define LORA_CODING_RATE      5        // 4/5 — minimum overhead
#define LORA_TX_POWER         14       // dBm — max legal EU 868
#define LORA_SYNC_WORD        0x77     // Private network sync word (avoid LoRaWAN 0x34/0x12)
#define LORA_PREAMBLE_LENGTH  8        // Default preamble length

// =============================================================================
// LoRa32 T3 v1.6.1 — SPI pin mapping for SX1276
// =============================================================================
#define LORA_SCK_PIN   5
#define LORA_MISO_PIN  19
#define LORA_MOSI_PIN  27
#define LORA_CS_PIN    18
#define LORA_RST_PIN   14     // Some v1.6 batches use GPIO23 — check silkscreen
#define LORA_DIO0_PIN  26

// =============================================================================
// LoRa32 T3 v1.6.1 — OLED SSD1306 128×64 (I²C)
// =============================================================================
#define OLED_SDA_PIN   21
#define OLED_SCL_PIN   22
#define OLED_RST_PIN   -1     // No hardware reset on T3 v1.6.1
#define OLED_I2C_ADDR  0x3C
#define OLED_WIDTH     128
#define OLED_HEIGHT    64

// =============================================================================
// Timing Configuration
// =============================================================================
#define MAIN_LOOP_DELAY          10   // ms — main loop yield
#define LORA_HEARTBEAT_INTERVAL  30000 // ms — transmitter heartbeat packet
#define LORA_RX_POLL_INTERVAL    0     // ms — 0 = poll every loop iteration

// Startup delay — shorter for debug builds
#ifdef DEBUG_BUILD
  #define STARTUP_MESSAGE_DELAY 1000
#else
  #define STARTUP_MESSAGE_DELAY 3000
#endif

// =============================================================================
// Serial Communication
// =============================================================================
#define SERIAL_BAUD_RATE 115200

// =============================================================================
// BLE Configuration (Transmitter role — shared with ESP32-S3-firmware headers)
// =============================================================================
#define BLE_DEVICE_NAME           "J.K. PewPew LoRa Bridge"
#define BLE_SCAN_DURATION         10
#define BLE_RECONNECT_INTERVAL    5000
#define BLE_SCAN_INTERVAL         1349
#define BLE_SCAN_WINDOW           449
#define BLE_CONNECTION_DELAY_MS   2000
#define BLE_HEARTBEAT_INTERVAL_MS 30000
#define BLE_SCAN_RETRY_INTERVAL_MS 5000

// =============================================================================
// MQTT Configuration (Receiver MQTT mode defaults)
// =============================================================================
#define MQTT_BROKER_IP   ""
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID   "j.k.pewpew-lora-bridge"
#define MQTT_USER        ""
#define MQTT_PASSWORD    ""

// =============================================================================
// Protocol Constants (shared with ESP32-S3-firmware)
// =============================================================================
#define MAX_SHOTS_PER_SESSION 100
#define SHOT_LIST_READ_DELAY  10

#define STARTUP_TEXT  "J.K. PewPew LoRa"
#define EMPTY_DEVICE_ID "000000"

// Timer type defines (referenced by shared ESP32-S3-firmware code)
#define TIMER_TYPE_BLE  1
#define TIMER_TYPE_MQTT 2
#define TIMER_TYPE TIMER_TYPE_BLE

#endif // COMMON_H

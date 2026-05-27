// WiFiConfig.h shim for BLE-LoRa-Bridge
// Redirects all WiFiConfig references from shared ESP32-S3-firmware code
// (MqttManager.h/cpp) to BridgeWiFiConfig which provides a WiFiConfig
// compatibility class.
#pragma once
#include "BridgeWiFiConfig.h"

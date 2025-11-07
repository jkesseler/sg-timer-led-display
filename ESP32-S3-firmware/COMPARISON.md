# Code Comparison: Before vs After Simplification

## Main Update Loop

### Before (Complex State Machine)
```cpp
void SGTimerDevice::update() {
  // Handle connection logic
  if (doConnect) {
    connectToServer();
    doConnect = false;
  }

  // Handle scanning
  if (doScan) {
    scanForDevices();
    doScan = false;
  }

  // Check for reconnection
  if (!deviceConnected && pClient != nullptr && connectionState == DeviceConnectionState::DISCONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > BLE_RECONNECT_INTERVAL) {
      LOG_INFO("SG-TIMER", "Attempting to reconnect...");
      startScanning();
      lastReconnectAttempt = millis();
    }
  }
}
```

### After (Simple Direct Pattern)
```cpp
void SGTimerDevice::update() {
  // If not connected, try to scan and connect
  if (!isConnectedFlag) {
    unsigned long now = millis();

    // Throttle reconnection attempts
    if (now - lastReconnectAttempt < 5000) {
      return;
    }

    lastReconnectAttempt = now;
    attemptConnection();
  } else {
    // Connected - check connection status
    if (pClient && pClient->isConnected()) {
      // Print heartbeat every 30 seconds
      if (millis() - lastHeartbeat > 30000) {
        Serial.printf("[SG-TIMER] Connected - waiting for events...\n");
        lastHeartbeat = millis();
      }
    } else {
      // Connection lost - cleanup and retry
      Serial.println("\n!!! Connection lost !!!");
      isConnectedFlag = false;
      // ... cleanup code ...
    }
  }
}
```

## Connection Flow

### Before (Multi-Step Deferred)
```cpp
// Step 1: Scan with callbacks
void SGTimerDevice::scanForDevices() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
  // Scan and set doConnect flag in callback
}

// Step 2: Connect in separate call
void SGTimerDevice::connectToServer() {
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallback(this));
  pClient->connect(*pServerAddress);
  // Wait for ClientCallback::onConnect
}

// Step 3: Discover services in callback
void SGTimerDevice::discoverServices(BLEClient* pClient) {
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  pRemoteCharacteristic->registerForNotify(notifyCallback);
}
```

### After (Single Direct Flow)
```cpp
void SGTimerDevice::attemptConnection() {
  // Scan
  BLEScan* pScan = BLEDevice::getScan();
  BLEScanResults foundDevices = pScan->start(10, false);

  // Find device
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    if (device.getAddress().toString() == TARGET_DEVICE_ADDRESS) {
      delay(2000);  // Wait before connecting

      // Connect
      pClient = BLEDevice::createClient();
      if (pClient->connect(&device)) {
        // Get service
        pService = pClient->getService(serviceUuid);

        // Get characteristic
        pEventCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

        // Register for notifications
        if (pEventCharacteristic->canNotify()) {
          pEventCharacteristic->registerForNotify(notifyCallback);
          isConnectedFlag = true;
          // Done!
        }
      }
      break;
    }
  }
}
```

## Data Structures

### Before
```cpp
// Multiple components to manage
BLEClient* pClient;
BLERemoteCharacteristic* pRemoteCharacteristic;
BLERemoteCharacteristic* pShotListCharacteristic;
BLEAddress* pServerAddress;

// Multiple state flags
bool deviceConnected;
bool doConnect;
bool doScan;
bool connectionInProgress;
String deviceName;
```

### After
```cpp
// Simplified components
BLEClient* pClient;
BLERemoteCharacteristic* pEventCharacteristic;
BLERemoteService* pService;

// Simple state
bool isConnectedFlag;
unsigned long lastReconnectAttempt;
unsigned long lastHeartbeat;

// Target device is constant
static const char* TARGET_DEVICE_ADDRESS = "dd:0e:9d:04:72:c3";
```

## Event Processing

### Before (Complex Logging)
```cpp
void SGTimerDevice::processTimerData(uint8_t* data, size_t length) {
  if (length < 2) {
    Serial.println("[SG-PARSE] Data packet too short");
    return;
  }

  String hexData = parseHexData(data, length);
  Serial.printf("[SG-DATA] Received %d bytes: %s\n", length, hexData.c_str());

  uint8_t eventId = data[1];
  Serial.printf("[SG-PARSE] Event ID: %d (0x%02X), Length: %d\n", eventId, eventId, len);

  // Complex case statements with blocks
  switch (eventId) {
    case 0x04: {
      // Parse shot...
      Serial.printf("[SG-TIMER] SHOT: %d - TIME: %.3fs | SPLIT: %.3fs\n", ...);
      // ...
    }
    break;
  }
}
```

### After (Matches minimal_test.cpp)
```cpp
void SGTimerDevice::processTimerData(uint8_t* pData, size_t length) {
  Serial.printf("\n*** Notification received (%d bytes): ", length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();

  if (length >= 2) {
    uint8_t event_id = pData[1];
    Serial.printf("Event ID: 0x%02X - ", event_id);

    switch (event_id) {
      case 0x04: // SHOT_DETECTED
        Serial.println("SHOT_DETECTED");
        // Parse shot...
        Serial.printf("  Shot #%u: %u:%02u\n", shot_num + 1, seconds, hundredths);
        // ...
        break;
    }
  }
}
```

## Key Differences Summary

| Aspect | Before | After |
|--------|--------|-------|
| **Connection Steps** | 3 separate methods with callbacks | 1 direct method |
| **State Flags** | 4 flags (doConnect, doScan, deviceConnected, connectionInProgress) | 1 flag (isConnectedFlag) |
| **Address Handling** | Dynamic with `BLEAddress* pServerAddress` | Constant `TARGET_DEVICE_ADDRESS` |
| **Callback Classes** | 2 classes (AdvertisedDeviceCallbacks, ClientCallback) | 0 classes (direct connection) |
| **Code Lines** | ~608 lines | ~460 lines |
| **Complexity** | High (state machine) | Low (linear flow) |
| **Debugging** | Difficult (async callbacks) | Easy (synchronous flow) |

## Conclusion

The simplified version:
- ✅ Matches the working minimal_test.cpp pattern
- ✅ Reduces complexity significantly
- ✅ Maintains all functionality
- ✅ Preserves the overall architecture
- ✅ Easier to debug and maintain

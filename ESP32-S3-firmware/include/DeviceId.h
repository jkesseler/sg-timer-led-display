#pragma once

#include <Arduino.h>

/**
 * @brief Manages a persistent unique device identifier
 *
 * On first boot, generates a UUID and stores it in non-volatile flash.
 * On subsequent boots, reads the stored ID from flash.
 * Accessible globally via the `deviceId` extern instance.
 */
class DeviceId {
public:
  DeviceId();

  /**
   * @brief Must be called once during setup() before get() is used
   */
  void initialize();

  /**
   * @brief Returns the device ID string (6-char alphanumeric)
   */
  String get() const;

  /**
   * @brief Clears the stored ID from flash and resets the in-memory ID
   */
  void reset();

private:
  String _deviceId;
  String generateId();
  String readFromFlash();
  void writeToFlash();
};

// Global singleton instance - include this header to access deviceId.get() anywhere
extern DeviceId deviceId;
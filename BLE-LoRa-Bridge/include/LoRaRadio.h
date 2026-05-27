#pragma once

#include <LoRa.h>
#include <SPI.h>
#include "common.h"

namespace LoRaRadio {

// Single source of truth for SX1276 radio parameters. Both LoRaTransmitter
// and LoRaReceiver must use identical settings (frequency, bandwidth,
// spreading factor, sync word) — otherwise they will not hear each other.
//
// Pass txPower >= 0 to configure transmit power (transmitter role only);
// a negative value leaves it at the library default (receiver role).
// Returns true if LoRa.begin() succeeded, false otherwise.
inline bool initialize(int txPower = -1) {
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    return false;
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  if (txPower >= 0) {
    LoRa.setTxPower(txPower);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
  LoRa.enableCrc();
  return true;
}

}  // namespace LoRaRadio

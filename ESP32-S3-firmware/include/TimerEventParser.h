#pragma once

#include "MqttEvent.h"
#include <cstddef>
#include <cstdint>

/** event is the topic after "timer/<id>/"; false for presence, unused events and malformed payloads. */
bool parseTimerEvent(const char* event, const uint8_t* payload, size_t length, MqttEvent& out);

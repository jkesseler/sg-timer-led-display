/**
 * @file Arduino.h
 * @brief Minimal Arduino API stub for native (host) testing.
 *
 * Provides just enough of the Arduino API to compile and test
 * protocol parsing, time formatting, and other pure logic
 * without ESP32 hardware.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

// ── Arduino-compatible type aliases ──────────────────────────────
typedef uint8_t byte;

// ── Controllable millis() for deterministic tests ────────────────
namespace ArduinoMock {
  inline unsigned long& millisValue() {
    static unsigned long ms = 0;
    return ms;
  }
  inline void setMillis(unsigned long ms) { millisValue() = ms; }
  inline void advanceMillis(unsigned long delta) { millisValue() += delta; }
  inline void resetMillis() { millisValue() = 0; }
}

inline unsigned long millis() { return ArduinoMock::millisValue(); }
inline void delay(unsigned long ms) { ArduinoMock::millisValue() += ms; }

// ── Minimal Serial stub (output goes to stderr) ─────────────────
class HardwareSerial {
public:
  void begin(unsigned long) {}

  int printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(stderr, fmt, args);
    va_end(args);
    return ret;
  }

  void println(const char* s) { fprintf(stderr, "%s\n", s); }
  void println()              { fprintf(stderr, "\n"); }
  void print(const char* s)   { fprintf(stderr, "%s", s); }

  size_t write(uint8_t c) { fputc(c, stderr); return 1; }
};

// Single global instance (matches Arduino runtime)
inline HardwareSerial& Serial = *[]() -> HardwareSerial* {
  static HardwareSerial s;
  return &s;
}();

// ── Minimal Arduino String class ────────────────────────────────
class String {
  std::string _data;
public:
  String() = default;
  String(const char* s) : _data(s ? s : "") {}
  String(const std::string& s) : _data(s) {}
  String(const String&) = default;
  String& operator=(const String&) = default;

  String& operator=(const char* s) { _data = s ? s : ""; return *this; }

  const char* c_str() const { return _data.c_str(); }
  size_t length() const     { return _data.length(); }

  bool startsWith(const char* prefix) const {
    if (!prefix) return false;
    size_t prefixLen = strlen(prefix);
    return _data.length() >= prefixLen && _data.compare(0, prefixLen, prefix) == 0;
  }

  char charAt(size_t index) const {
    if (index < _data.length()) return _data[index];
    return 0;
  }

  // Concatenation
  String operator+(const String& rhs) const { return String((_data + rhs._data).c_str()); }
  String operator+(const char* rhs) const   { return String((_data + (rhs ? rhs : "")).c_str()); }

  friend String operator+(const char* lhs, const String& rhs) {
    return String((std::string(lhs ? lhs : "") + rhs._data).c_str());
  }

  bool operator==(const String& other) const { return _data == other._data; }
  bool operator!=(const String& other) const { return _data != other._data; }
};

// ── F() macro (no-op on native) ─────────────────────────────────
#define F(str) str

// ── ESP32 system stubs ──────────────────────────────────────────
class EspClass {
public:
  uint32_t getFreeHeap() { return 300000; }
};

inline EspClass ESP;

// ── FreeRTOS stubs ──────────────────────────────────────────────
#define pdMS_TO_TICKS(ms) (ms)
inline void vTaskDelay(unsigned long) {}

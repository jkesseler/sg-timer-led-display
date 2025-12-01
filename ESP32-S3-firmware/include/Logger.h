#pragma once

#include <Arduino.h>

// Logging levels
enum class LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  NONE = 4
};

// Logger configuration
class Logger {
private:
  static LogLevel currentLevel;
  static const char* getLevelString(LogLevel level);
  static const char* getComponentColor(const char* component);

public:
  static void setLevel(LogLevel level);
  static LogLevel getLevel();
  static void log(LogLevel level, const char* component, const char* format, ...);

  // Convenience macros will be defined below
};

// Logging macros for easier use
#define LOG_DEBUG(component, ...) Logger::log(LogLevel::DEBUG, component, __VA_ARGS__)
#define LOG_INFO(component, ...)  Logger::log(LogLevel::INFO, component, __VA_ARGS__)
#define LOG_WARN(component, ...)  Logger::log(LogLevel::WARN, component, __VA_ARGS__)
#define LOG_ERROR(component, ...) Logger::log(LogLevel::ERROR, component, __VA_ARGS__)

// Component-specific logging macros
#define LOG_DISPLAY(...) LOG_INFO("DISPLAY", __VA_ARGS__)
#define LOG_BLE(...) LOG_INFO("BLE", __VA_ARGS__)
#define LOG_TIMER(...) LOG_INFO("TIMER", __VA_ARGS__)
#define LOG_BRIGHTNESS(...) LOG_DEBUG("BRIGHTNESS", __VA_ARGS__)
#define LOG_SYSTEM(...) LOG_INFO("SYSTEM", __VA_ARGS__)
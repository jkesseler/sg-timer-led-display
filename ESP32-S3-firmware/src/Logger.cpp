#include "Logger.h"
#include <stdarg.h>

LogLevel Logger::currentLevel = LogLevel::INFO;

void Logger::setLevel(LogLevel level) {
  currentLevel = level;
}

const char* Logger::getLevelString(LogLevel level) {
  switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO:  return "INFO ";
    case LogLevel::WARN:  return "WARN ";
    case LogLevel::ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

const char* Logger::getComponentColor(const char* component) {
  // Simple hash to assign consistent colors to components
  uint32_t hash = 0;
  for (const char* p = component; *p; p++) {
    hash = hash * 31 + *p;
  }

  // Return ANSI color codes based on hash
  switch (hash % 6) {
    case 0: return "\033[36m"; // Cyan
    case 1: return "\033[33m"; // Yellow
    case 2: return "\033[35m"; // Magenta
    case 3: return "\033[32m"; // Green
    case 4: return "\033[34m"; // Blue
    case 5: return "\033[31m"; // Red
    default: return "\033[37m"; // White
  }
}

void Logger::log(LogLevel level, const char* component, const char* format, ...) {
  if (level < currentLevel) {
    return;
  }

  // Print timestamp
  Serial.printf("[%8lu] ", millis());

  // Print level with color
  const char* levelColor = (level == LogLevel::ERROR) ? "\033[31m" :
                          (level == LogLevel::WARN) ? "\033[33m" : "\033[37m";
  Serial.printf("%s%s\033[0m ", levelColor, getLevelString(level));

  // Print component with color
  Serial.printf("%s%-10s\033[0m ", getComponentColor(component), component);

  // Print the actual message
  va_list args;
  va_start(args, format);
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.println(buffer);
}
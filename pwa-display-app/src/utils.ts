/**
 * Utility functions for SG Timer PWA Display
 * Time formatting utilities matching ESP32 firmware
 */

/**
 * Format a timer value (elapsed time or split time) to hundredths precision.
 * Under 60s: "S.CC" (e.g., "2.34"). At/over 60s: "M:SS.CC" (e.g., "1:23.40").
 * Shot timers conventionally report to the hundredth of a second.
 */
export function formatTimeValue(timeMs: number): string {
  // Quantise to centiseconds first, then split into fields. Rounding after the
  // split would let 59.999s render as "60.00" (and 119.999s as "1:60.00")
  // instead of carrying the second into the minutes field.
  const totalCentiseconds = Math.round(timeMs / 10);
  const centiseconds = totalCentiseconds % 100;
  const totalSeconds = Math.floor(totalCentiseconds / 100);
  const centisecondsText = String(centiseconds).padStart(2, '0');

  if (totalSeconds < 60) {
    return `${totalSeconds}.${centisecondsText}`;
  }

  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;

  return `${minutes}:${String(seconds).padStart(2, '0')}.${centisecondsText}`;
}

/**
 * Format countdown display
 * Shows remaining seconds with one decimal place
 */
export function formatCountdown(remainingSeconds: number): string {
  return remainingSeconds.toFixed(1);
}

/**
 * Parse MQTT message payload
 */
export function parseMqttMessage<T = any>(topic: string, payload: Buffer): T | null {
  try {
    return JSON.parse(payload.toString()) as T;
  } catch (error) {
    console.error(`Failed to parse MQTT message on topic ${topic}:`, error);
    return null;
  }
}

/**
 * Local storage helpers with type safety
 */
export const storage = {
  get<T>(key: string, defaultValue: T): T {
    try {
      const item = localStorage.getItem(key);
      return item ? JSON.parse(item) as T : defaultValue;
    } catch {
      return defaultValue;
    }
  },

  set<T>(key: string, value: T): boolean {
    try {
      localStorage.setItem(key, JSON.stringify(value));
      return true;
    } catch {
      return false;
    }
  },

  remove(key: string): boolean {
    try {
      localStorage.removeItem(key);
      return true;
    } catch {
      return false;
    }
  },

  clear(): boolean {
    try {
      localStorage.clear();
      return true;
    } catch {
      return false;
    }
  }
};

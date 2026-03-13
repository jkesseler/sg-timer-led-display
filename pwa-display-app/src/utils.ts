/**
 * Utility functions for SG Timer PWA Display
 * Time formatting utilities matching ESP32 firmware
 */

/**
 * Format milliseconds to display string
 * Under 60s: "SS.mmm" (e.g., "02.345")
 * Over 60s: "MM:SS.t" (e.g., "01:23.4")
 */
export function formatTime(timeMs: number): string {
  const totalSeconds = timeMs / 1000;

  if (totalSeconds < 60) {
    // Format as SS.mmm
    const seconds = Math.floor(totalSeconds);
    const milliseconds = Math.floor((timeMs % 1000));
    return `${seconds.toString().padStart(2, '0')}.${milliseconds.toString().padStart(3, '0')}`;
  } else {
    // Format as MM:SS.t
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = Math.floor(totalSeconds % 60);
    const tenths = Math.floor((timeMs % 1000) / 100);
    return `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}.${tenths}`;
  }
}

/**
 * Format split time (time since previous shot)
 * Always format as S.mmm or SS.mmm
 */
export function formatSplitTime(timeMs: number): string {
  const totalSeconds = timeMs / 1000;
  const seconds = Math.floor(totalSeconds);
  const milliseconds = Math.floor((timeMs % 1000));
  return `${seconds}.${milliseconds.toString().padStart(3, '0')}`;
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
 * Get text width estimation for scrolling
 * Roughly matches ESP32 character width calculations
 */
export function estimateTextWidth(text: string, fontSize: number): number {
  // Rough estimate: ~0.6 * fontSize per character
  return text.length * fontSize * 0.6;
}

/**
 * Calculate pixel scaling for responsive display
 */
export function calculatePixelSize(
  containerWidth: number,
  containerHeight: number,
  displayWidth: number,
  displayHeight: number
): number {
  const widthScale = containerWidth / displayWidth;
  const heightScale = containerHeight / displayHeight;
  return Math.floor(Math.min(widthScale, heightScale));
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

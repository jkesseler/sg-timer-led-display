/** "S.CC" under a minute, "M:SS.CC" from a minute up. */
export function formatTimeValue(timeMs: number): string {
  const totalSeconds = timeMs / 1000;

  if (totalSeconds < 60) {
    return totalSeconds.toFixed(2);
  }

  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds - minutes * 60;

  return `${minutes}:${seconds.toFixed(2).padStart(5, '0')}`;
}

export function formatCountdown(remainingSeconds: number): string {
  return remainingSeconds.toFixed(1);
}

export function parseMqttMessage<T = any>(topic: string, payload: Buffer): T | null {
  try {
    return JSON.parse(payload.toString()) as T;
  } catch (error) {
    console.error(`Failed to parse MQTT message on topic ${topic}:`, error);

    return null;
  }
}

/** localStorage that never throws (private mode, quota, server render). */
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

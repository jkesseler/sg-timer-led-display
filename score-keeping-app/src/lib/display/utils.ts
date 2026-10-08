/** Seconds with hundredths ("S.CC"), never rolled into minutes: 70 s is "70.00". */
export function formatTimeValue(timeMs: number): string {
  // Integer centiseconds avoid float artefacts in toFixed (1.005.toFixed(2) === "1.00").
  const totalCentiseconds = Math.round(timeMs / 10);
  const centisecondsText = String(totalCentiseconds % 100).padStart(2, '0');

  return `${Math.floor(totalCentiseconds / 100)}.${centisecondsText}`;
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

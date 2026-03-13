import { useState, useEffect, useCallback, useRef } from 'react';
import mqtt, { MqttClient } from 'mqtt';
import { MqttTopics, DefaultMqttSettings, parseDeviceTopic } from '../constants';
import { parseMqttMessage, storage } from '../utils';
import type { MqttSettings, MqttMessageHandler, UseMqttReturn, KnownDevice } from '../types';

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * Match a topic pattern (with '+' single-level wildcard) against a concrete topic.
 * '#' multi-level wildcard is supported only at the end.
 */
function matchPattern(pattern: string, topic: string): boolean {
  const pp = pattern.split('/');
  const tp = topic.split('/');
  if (pp[pp.length - 1] === '#') {
    if (tp.length < pp.length - 1) return false;
    return pp.slice(0, -1).every((seg, i) => seg === '+' || seg === tp[i]);
  }
  if (pp.length !== tp.length) return false;
  return pp.every((seg, i) => seg === '+' || seg === tp[i]);
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

/**
 * Custom hook for MQTT connection and multi-device message routing.
 *
 * Topic scheme: timer/<deviceId>/<event>
 *
 * The hook subscribes to 'timer/#' in a single call, then routes each
 * incoming message to handlers that were registered with wildcard patterns
 * (e.g. MqttTopics.SHOT_DETECTED = 'timer/+/shot/detected').
 *
 * Retained messages (presence, connection/state, device/info) are
 * delivered by the broker immediately upon subscription, ensuring displays
 * that power on *after* devices are running still receive current state.
 *
 * selectedDeviceId:
 *   null   → auto-mode: routes messages from the first online device found
 *   string → only messages from that specific device reach handlers
 */
export function useMqtt(): UseMqttReturn {
  // Ref that always holds the live client so closures (cleanup, connect guard)
  // can access it without being part of any dependency array.
  const clientRef = useRef<MqttClient | null>(null);
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [connectionError, setConnectionError] = useState<string | null>(null);
  const [settings, setSettings] = useState<MqttSettings>(() => {
    return storage.get<MqttSettings>('mqttSettings', DefaultMqttSettings);
  });
  const [knownDevices, setKnownDevices] = useState<KnownDevice[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState<string | null>(null);

  const messageHandlersRef = useRef<Record<string, MqttMessageHandler>>({});
  // Mirror of knownDevices state as a ref so the MQTT message handler can
  // read the latest value without being part of a state-updater closure.
  const knownDevicesRef = useRef<KnownDevice[]>([]);

  // The "active" device for auto-mode: first online device, or the pinned one.
  const resolveActiveDeviceId = useCallback(
    (devices: KnownDevice[], pinned: string | null): string | null => {
      if (pinned) return pinned;
      return devices.find(d => d.presence === 'online')?.deviceId ?? null;
    },
    []
  );

  // -------------------------------------------------------------------------
  // Device registry – updated from timer/<id>/presence messages
  // -------------------------------------------------------------------------
  const updateDevicePresence = useCallback(
    (deviceId: string, presence: 'online' | 'offline') => {
      setKnownDevices(prev => {
        const idx = prev.findIndex(d => d.deviceId === deviceId);
        const next = idx === -1
          ? [...prev, { deviceId, presence, lastSeenMs: Date.now() }]
          : prev.map((d, i) => i === idx ? { ...d, presence, lastSeenMs: Date.now() } : d);
        knownDevicesRef.current = next;
        return next;
      });
    },
    []
  );

  const updateDeviceInfo = useCallback(
    (deviceId: string, patch: Partial<Pick<KnownDevice, 'deviceName' | 'deviceModel' | 'firmwareVersion'>>) => {
      setKnownDevices(prev => {
        const idx = prev.findIndex(d => d.deviceId === deviceId);
        const next: KnownDevice[] = idx === -1
          ? [...prev, { deviceId, presence: 'online' as const, lastSeenMs: Date.now(), ...patch }]
          : prev.map((d, i) => i === idx ? { ...d, ...patch, lastSeenMs: Date.now() } : d);
        knownDevicesRef.current = next;
        return next;
      });
    },
    []
  );

  // -------------------------------------------------------------------------
  // MQTT connect
  // -------------------------------------------------------------------------
  const connect = useCallback((customSettings: MqttSettings | null = null) => {
    const mqttSettings = customSettings || settings;

    // Guard: end any existing client before creating a new one.
    // This prevents duplicate clients (and duplicate message handlers) if
    // connect() is called while a client is already alive – e.g. React 18
    // StrictMode double-mounting the auto-connect effect in development.
    if (clientRef.current) {
      clientRef.current.end(true);
      clientRef.current = null;
      setIsConnected(false);
    }

    try {
      // Always generate a fresh clientId so multiple tabs / page reloads never
      // share the same ID.  Two clients with the same ID cause the broker to
      // kick one out, triggering an infinite reconnect loop.
      const freshClientId = `pwa-display-${Math.random().toString(16).substring(2, 10)}`;

      const options: mqtt.IClientOptions = {
        clientId: freshClientId,
        clean: true,
        reconnectPeriod: 5000,
        connectTimeout: 30000
      };

      if (mqttSettings.username) {
        options.username = mqttSettings.username;
        options.password = mqttSettings.password;
      }

      console.log('Connecting to MQTT broker:', mqttSettings.broker);
      const mqttClient = mqtt.connect(mqttSettings.broker, options);

      mqttClient.on('connect', () => {
        console.log('MQTT connected');
        setIsConnected(true);
        setConnectionError(null);

        // Single wildcard subscription covers all device topics.
        // The broker will replay retained messages (presence, connection/state,
        // device/info) immediately – no re-announce needed from devices.
        mqttClient.subscribe('timer/#', { qos: 1 }, (err) => {
          if (err) {
            console.error('Failed to subscribe to timer/#:', err);
          } else {
            console.log('Subscribed to timer/# (all devices)');
          }
        });
      });

      mqttClient.on('error', (error: Error) => {
        console.error('MQTT error:', error);
        setConnectionError(error.message);
        setIsConnected(false);
      });

      mqttClient.on('close', () => {
        console.log('MQTT connection closed');
        setIsConnected(false);
      });

      mqttClient.on('offline', () => {
        console.log('MQTT client offline');
        setIsConnected(false);
      });

      mqttClient.on('reconnect', () => {
        console.log('MQTT reconnecting...');
      });

      mqttClient.on('message', (topic: string, payload: Buffer) => {
        const parsed = parseDeviceTopic(topic);
        if (!parsed) return;

        const { deviceId, event } = parsed;

        // --- Presence: handle directly in hook (built-in device discovery) ---
        if (event === 'presence') {
          const presence = payload.toString() as 'online' | 'offline';
          updateDevicePresence(deviceId, presence);
          // Also forward to any registered presence handler
          const handler = messageHandlersRef.current[MqttTopics.PRESENCE];
          if (handler) handler({ presence }, deviceId);
          return;
        }

        // --- Device info: update registry in addition to forwarding ---
        if (event === 'device/info') {
          const msg = parseMqttMessage<any>(topic, payload);
          if (msg) {
            updateDeviceInfo(deviceId, {
              deviceName: msg.deviceName,
              deviceModel: msg.deviceModel,
              firmwareVersion: msg.firmwareVersion,
            });
          }
        }

        // --- Route to registered pattern handlers ---
        // Read current devices and pinned selection directly from refs —
        // never use setKnownDevices as a state reader; React may call updater
        // functions multiple times (StrictMode / concurrent mode), which would
        // fire handlers more than once per message.
        const activeId = resolveActiveDeviceId(knownDevicesRef.current, pinnedDeviceRef.current);

        if (activeId && deviceId !== activeId) {
          return; // message not from active device – ignore
        }

        // Find a registered handler whose pattern matches this topic
        for (const [pattern, handler] of Object.entries(messageHandlersRef.current)) {
          if (matchPattern(pattern, topic)) {
            const msg = parseMqttMessage(topic, payload);
            if (msg) handler(msg, deviceId);
            break;
          }
        }
      });

      clientRef.current = mqttClient;
    } catch (error) {
      console.error('Failed to connect to MQTT:', error);
      setConnectionError((error as Error).message);
    }
  }, [settings, updateDevicePresence, updateDeviceInfo, resolveActiveDeviceId]);

  // We need a stable ref to selectedDeviceId so the message handler inside
  // the 'message' event closure always reads the latest value.
  const pinnedDeviceRef = useRef<string | null>(null);
  useEffect(() => {
    pinnedDeviceRef.current = selectedDeviceId;
  }, [selectedDeviceId]);

  // -------------------------------------------------------------------------
  // Disconnect
  // -------------------------------------------------------------------------
  const disconnect = useCallback(() => {
    if (clientRef.current) {
      clientRef.current.end();
      clientRef.current = null;
      setIsConnected(false);
    }
  }, []);

  // -------------------------------------------------------------------------
  // Register message handler for a topic pattern
  // -------------------------------------------------------------------------
  const onMessage = useCallback(<T = any>(topic: string, handler: MqttMessageHandler<T>) => {
    messageHandlersRef.current[topic] = handler as MqttMessageHandler;
  }, []);

  // -------------------------------------------------------------------------
  // Select device
  // -------------------------------------------------------------------------
  const selectDevice = useCallback((deviceId: string | null) => {
    setSelectedDeviceId(deviceId);
  }, []);

  // -------------------------------------------------------------------------
  // Settings
  // -------------------------------------------------------------------------
  const updateSettings = useCallback((newSettings: MqttSettings) => {
    setSettings(newSettings);
    storage.set('mqttSettings', newSettings);
  }, []);

  // Auto-connect on mount
  useEffect(() => {
    if (settings.broker) {
      connect();
    }

    return () => {
      // Use the ref – not the state – so the cleanup always sees the live client
      // even when React 18 StrictMode unmounts and remounts the component.
      if (clientRef.current) {
        clientRef.current.end(true);
        clientRef.current = null;
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // Only run on mount

  return {
    isConnected,
    connectionError,
    settings,
    knownDevices,
    selectedDeviceId,
    selectDevice,
    connect,
    disconnect,
    onMessage,
    updateSettings
  };
}


import { useState, useEffect, useCallback, useRef } from 'react';
import mqtt, { MqttClient } from 'mqtt';
import { MqttTopics, DefaultMqttSettings } from '../constants';
import { parseMqttMessage, storage } from '../utils';
import type { MqttSettings, MqttMessageHandler, UseMqttReturn } from '../types';

/**
 * Custom hook for MQTT connection and message handling
 */
export function useMqtt(): UseMqttReturn {
  const [client, setClient] = useState<MqttClient | null>(null);
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [connectionError, setConnectionError] = useState<string | null>(null);
  const [settings, setSettings] = useState<MqttSettings>(() => {
    return storage.get<MqttSettings>('mqttSettings', DefaultMqttSettings);
  });

  const messageHandlersRef = useRef<Record<string, MqttMessageHandler>>({});

  // Connect to MQTT broker
  const connect = useCallback((customSettings: MqttSettings | null = null) => {
    const mqttSettings = customSettings || settings;

    try {
      const options: mqtt.IClientOptions = {
        clientId: mqttSettings.clientId,
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

        // Subscribe to all topics
        Object.values(MqttTopics).forEach(topic => {
          mqttClient.subscribe(topic, (err) => {
            if (err) {
              console.error(`Failed to subscribe to ${topic}:`, err);
            } else {
              console.log(`Subscribed to ${topic}`);
            }
          });
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
        const message = parseMqttMessage(topic, payload);
        if (message && messageHandlersRef.current[topic]) {
          messageHandlersRef.current[topic](message);
        }
      });

      setClient(mqttClient);
    } catch (error) {
      console.error('Failed to connect to MQTT:', error);
      setConnectionError((error as Error).message);
    }
  }, [settings]);

  // Disconnect from MQTT broker
  const disconnect = useCallback(() => {
    if (client) {
      client.end();
      setClient(null);
      setIsConnected(false);
    }
  }, [client]);

  // Register message handler for a topic
  const onMessage = useCallback(<T = any>(topic: string, handler: MqttMessageHandler<T>) => {
    messageHandlersRef.current[topic] = handler as MqttMessageHandler;
  }, []);

  // Update settings and persist
  const updateSettings = useCallback((newSettings: MqttSettings) => {
    setSettings(newSettings);
    storage.set('mqttSettings', newSettings);
  }, []);

  // Auto-connect on mount if settings exist
  useEffect(() => {
    if (settings.broker && !client) {
      connect();
    }

    return () => {
      if (client) {
        client.end();
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // Only run on mount

  return {
    isConnected,
    connectionError,
    settings,
    connect,
    disconnect,
    onMessage,
    updateSettings
  };
}

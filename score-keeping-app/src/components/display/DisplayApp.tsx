'use client';

import { useEffect, useRef, useState } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { STARTUP_DISPLAY_MS } from '@/lib/display/constants';
import {
  startConnecting,
  startupComplete,
  selectDevice,
  selectIsConnected,
  selectConnectionError,
  selectKnownDevices,
  selectSelectedDeviceId,
  selectIsCurrentDeviceConnected,
  selectCurrentDeviceId
} from '@/store/mqttSlice';
import { disconnectMqttClient } from '@/store/mqttMiddleware';
import {
  selectSettings,
  updateSettings,
  resetBrokerSettings,
  increaseBrightness,
  decreaseBrightness
} from '@/store/settingsSlice';
import type { RosterInfo } from '@/lib/match/types';
import type { MqttSettings } from '@/lib/mqtt/types';
import { getRosterForDevice } from '@/app/display/actions';
import Settings from './Settings';
import { TimerDisplay } from './TimerDisplay';
import './DisplayApp.css';

// Only the roster names poll; shot events arrive instantly over MQTT.
const ROSTER_POLL_INTERVAL_MS = 3000;

function DisplayApp() {
  const dispatch = useDispatch();

  const isConnected = useSelector(selectIsConnected);
  const connectionError = useSelector(selectConnectionError);
  const knownDevices = useSelector(selectKnownDevices);
  const selectedDeviceId = useSelector(selectSelectedDeviceId);
  const isCurrentDeviceConnected = useSelector(selectIsCurrentDeviceConnected);
  const currentDeviceId = useSelector(selectCurrentDeviceId);
  const settings = useSelector(selectSettings);

  // Status bar starts hidden so a freshly booted kiosk shows a clean display.
  const [showSettings, setShowSettings] = useState(false);
  const [showStatus, setShowStatus] = useState(false);
  const [roster, setRoster] = useState<RosterInfo | null>(null);

  // The startup effect connects on first render, so the reconnect effect skips it.
  const isInitialMount = useRef(true);

  useEffect(() => {
    if (settings.broker) {
      dispatch(startConnecting());
    }

    const timer = setTimeout(() => {
      dispatch(startupComplete());
    }, STARTUP_DISPLAY_MS);

    return () => {
      clearTimeout(timer);
      disconnectMqttClient();
    };
  }, []);

  useEffect(() => {
    if (isInitialMount.current) {
      isInitialMount.current = false;

      return;
    }

    disconnectMqttClient();
    const timer = setTimeout(() => dispatch(startConnecting()), 500);

    return () => clearTimeout(timer);
  }, [settings.broker, settings.username, settings.password, dispatch]);

  useEffect(() => {
    if (!currentDeviceId) {
      setRoster(null);

      return;
    }

    let cancelled = false;
    const poll = () => {
      getRosterForDevice(currentDeviceId)
        .then((result) => {
          if (!cancelled) {
            setRoster(result);
          }
        })
        .catch(error => console.error('[display] failed to fetch roster', error));
    };

    poll();
    const interval = setInterval(poll, ROSTER_POLL_INTERVAL_MS);

    return () => {
      cancelled = true;
      clearInterval(interval);
    };
  }, [currentDeviceId]);

  const handleSaveSettings = (newSettings: MqttSettings) => {
    dispatch(
      updateSettings({
        broker: newSettings.broker,
        username: newSettings.username,
        password: newSettings.password,
        brightness: newSettings.brightness ?? settings.brightness
      })
    );

    setShowSettings(false);
  };

  useEffect(() => {
    const handleKeyPress = (e: KeyboardEvent) => {
      if (e.key === 's' || e.key === 'S') {
        setShowSettings(true);
      } else if (e.key === 'i' || e.key === 'I') {
        setShowStatus(prev => !prev);
      } else if (e.key === 'Escape') {
        setShowSettings(false);
      }
    };

    window.addEventListener('keydown', handleKeyPress);

    return () => window.removeEventListener('keydown', handleKeyPress);
  }, []);

  const brightnessFilter = `brightness(${(settings.brightness / 255).toFixed(2)})`;

  return (
    <div className="app" style={{ filter: brightnessFilter }}>
      {showStatus && (
        <div className="status-bar">
          <div className="status-item">
            <span className={`status-indicator ${isConnected ? 'connected' : 'disconnected'}`} />
            <span>{isConnected ? 'MQTT Connected' : 'MQTT Disconnected'}</span>
          </div>
          {connectionError && (
            <div className="status-item error">
              Error:
              {connectionError}
            </div>
          )}
          <div className="status-item">{settings.broker}</div>
          {knownDevices.length > 1 && (
            <div className="status-item">
              <label htmlFor="device-select">Display: </label>
              <select
                id="device-select"
                value={selectedDeviceId ?? ''}
                onChange={e => dispatch(selectDevice(e.target.value || null))}
              >
                <option value="">
                  Auto (
                  {knownDevices.find(device => device.presence === 'online')?.deviceId ?? 'none'}
                  )
                </option>
                {knownDevices.map(device => (
                  <option key={device.deviceId} value={device.deviceId}>
                    {device.deviceName ?? device.deviceId}
                    {' '}
                    {device.presence === 'offline' ? '(offline)' : ''}
                  </option>
                ))}
              </select>
            </div>
          )}
          {knownDevices.length === 1 && (
            <div className="status-item">
              Display:
              {' '}
              {knownDevices[0].deviceName ?? knownDevices[0].deviceId}
              {knownDevices[0].presence === 'offline' ? ' (offline)' : ''}
            </div>
          )}
        </div>
      )}

      <div className="stage">
        {isCurrentDeviceConnected
          ? (
              <TimerDisplay roster={roster} />
            )
          : (
              <div className="no-device">
                <span className="no-device__title">No display found</span>
                <span className="no-device__detail">Waiting for a bridge device to come online</span>
              </div>
            )}
      </div>

      <div className="controls">
        <button className="control-button" onClick={() => setShowSettings(true)} title="Open settings (S)">
          Settings
        </button>
        <button
          className="control-button"
          onClick={() => setShowStatus(prev => !prev)}
          title="Toggle status bar (I)"
        >
          {showStatus ? 'Hide status' : 'Show status'}
        </button>
        <div className="brightness-control" title="Screen brightness">
          <button
            className="control-button control-button--icon"
            onClick={() => dispatch(decreaseBrightness())}
            aria-label="Dim screen"
          >
            −
          </button>
          <button
            className="control-button control-button--icon"
            onClick={() => dispatch(increaseBrightness())}
            aria-label="Brighten screen"
          >
            +
          </button>
        </div>
      </div>

      {showSettings && (
        <Settings
          onSave={handleSaveSettings}
          onClose={() => setShowSettings(false)}
          onConnect={() => dispatch(startConnecting())}
          onDisconnect={disconnectMqttClient}
          onResetBroker={() => {
            dispatch(resetBrokerSettings());
            setShowSettings(false);
          }}
        />
      )}
    </div>
  );
}

export default DisplayApp;

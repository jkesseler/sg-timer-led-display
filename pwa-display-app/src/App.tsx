import { useEffect, useRef, useState } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import TimerDisplay from './components/TimerDisplay';
import Settings from './components/Settings';
import { STARTUP_DISPLAY_MS } from './constants';
import {
  startConnecting,
  startupComplete,
  selectDevice,
  selectIsConnected,
  selectConnectionError,
  selectKnownDevices,
  selectSelectedDeviceId,
  selectIsCurrentDeviceConnected,
} from './store/mqttSlice';
import { disconnectMqttClient } from './store/mqttMiddleware';
import {
  selectSettings,
  updateSettings,
  increaseBrightness,
  decreaseBrightness,
} from './store/settingsSlice';
import type { MqttSettings, KnownDevice } from './types';
import './App.css';

function App() {
  const dispatch = useDispatch();

  // Redux state
  const isConnected = useSelector(selectIsConnected);
  const connectionError = useSelector(selectConnectionError);
  const knownDevices = useSelector(selectKnownDevices);
  const selectedDeviceId = useSelector(selectSelectedDeviceId);
  const isCurrentDeviceConnected = useSelector(selectIsCurrentDeviceConnected);
  const settings = useSelector(selectSettings);

  // Local UI state — status bar starts hidden so a freshly booted kiosk
  // shows a clean display; press "I" to check connection details.
  const [showSettings, setShowSettings] = useState(false);
  const [showStatus, setShowStatus] = useState(false);

  // Track whether this is the initial mount so the reconnect effect
  // doesn't fire on first render (the startup effect handles that).
  const isInitialMount = useRef(true);

  // Startup: auto-connect and schedule startup-complete
  useEffect(() => {
    if (settings.broker) {
      dispatch(startConnecting());
    }

    const timer = setTimeout(() => {
      dispatch(startupComplete());
    }, STARTUP_DISPLAY_MS);

    // Teardown on unmount
    return () => {
      clearTimeout(timer);
      disconnectMqttClient();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Reconnect whenever connection-relevant settings change
  useEffect(() => {
    if (isInitialMount.current) {
      isInitialMount.current = false;
      return;
    }

    disconnectMqttClient();
    const timer = setTimeout(() => dispatch(startConnecting()), 500);
    return () => clearTimeout(timer);
  }, [settings.broker, settings.username, settings.password, dispatch]);

  // Handle settings save
  const handleSaveSettings = (newSettings: MqttSettings) => {
    dispatch(
      updateSettings({
        broker: newSettings.broker,
        username: newSettings.username,
        password: newSettings.password,
        brightness: newSettings.brightness ?? settings.brightness,
      }),
    );

    setShowSettings(false);
  };

  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyPress = (e: KeyboardEvent) => {
      if (e.key === 's' || e.key === 'S') {
        setShowSettings(true);
      } else if (e.key === 'i' || e.key === 'I') {
        setShowStatus((prev) => !prev);
      } else if (e.key === 'Escape') {
        setShowSettings(false);
      }
    };

    window.addEventListener('keydown', handleKeyPress);
    return () => window.removeEventListener('keydown', handleKeyPress);
  }, []);

  // Only apply the filter when actually dimming — at full brightness it would be
  // brightness(1.00), a no-op that still forces .app onto its own compositing
  // layer and makes it the containing block for the fixed-position UI.
  const isDimmed = settings.brightness < 255;
  const appStyle = isDimmed
    ? { filter: `brightness(${(settings.brightness / 255).toFixed(2)})` }
    : undefined;

  return (
    <div className="app" style={appStyle}>
      {/* Status Bar */}
      {showStatus && (
        <div className="status-bar">
          <div className="status-item">
            <span className={`status-indicator ${isConnected ? 'connected' : 'disconnected'}`} />
            <span>{isConnected ? 'MQTT Connected' : 'MQTT Disconnected'}</span>
          </div>
          {connectionError && (
            <div className="status-item error">Error: {connectionError}</div>
          )}
          <div className="status-item">{settings.broker}</div>
          {/* Device selector – shown when more than one device is online */}
          {knownDevices.length > 1 && (
            <div className="status-item">
              <label htmlFor="device-select">Display: </label>
              <select
                id="device-select"
                value={selectedDeviceId ?? ''}
                onChange={(e) => dispatch(selectDevice(e.target.value || null))}
              >
                <option value="">
                  Auto ({knownDevices.find((d: KnownDevice) => d.presence === 'online')?.deviceId ?? 'none'})
                </option>
                {knownDevices.map((d: KnownDevice) => (
                  <option key={d.deviceId} value={d.deviceId}>
                    {d.deviceName ?? d.deviceId}{' '}
                    {d.presence === 'offline' ? '(offline)' : ''}
                  </option>
                ))}
              </select>
            </div>
          )}
          {knownDevices.length === 1 && (
            <div className="status-item">
              Display: {knownDevices[0].deviceName ?? knownDevices[0].deviceId}
              {knownDevices[0].presence === 'offline' ? ' (offline)' : ''}
            </div>
          )}
        </div>
      )}

      <div className="stage">
        {isCurrentDeviceConnected ? (
          <TimerDisplay />
        ) : (
          <div className="no-device">
            <span className="no-device__title">No display found</span>
            <span className="no-device__detail">Waiting for a bridge device to come online</span>
          </div>
        )}
      </div>

      {/* Controls */}
      <div className="controls">
        <button className="control-button" onClick={() => setShowSettings(true)} title="Open settings (S)">
          Settings
        </button>
        <button
          className="control-button"
          onClick={() => setShowStatus((prev) => !prev)}
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

      {/* Settings Modal */}
      {showSettings && (
        <Settings
          onSave={handleSaveSettings}
          onClose={() => setShowSettings(false)}
          onConnect={() => dispatch(startConnecting())}
          onDisconnect={disconnectMqttClient}
        />
      )}
    </div>
  );
}

export default App;

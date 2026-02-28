import { useEffect, useRef, useState } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import LEDMatrix from './components/LEDMatrix';
import Settings from './components/Settings';
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

  // Local UI state
  const [showSettings, setShowSettings] = useState(false);
  const [showStatus, setShowStatus] = useState(true);

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
    }, 3000);

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

  return (
    <div className="app">
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

      {isCurrentDeviceConnected && <LEDMatrix />}
      {!isCurrentDeviceConnected && (
        <div className="no-device">
          <p>Selected display is offline</p>
        </div>
      )
      }
      {/* Controls */}
      <div className="controls">
        <button
          className="control-button"
          onClick={() => setShowSettings(true)}
          title="Settings (S)"
        >
          ⚙️ Settings
        </button>
        <button
          className="control-button"
          onClick={() => setShowStatus((prev) => !prev)}
          title="Toggle Status (I)"
        >
          {showStatus ? '🔼' : '🔽'} Status
        </button>
        <button
          className="control-button"
          onClick={() => dispatch(increaseBrightness())}
          title="Increase Brightness"
        >
          🔆 +
        </button>
        <button
          className="control-button"
          onClick={() => dispatch(decreaseBrightness())}
          title="Decrease Brightness"
        >
          🔅 -
        </button>
      </div>

      {/* Settings Modal */}
      {showSettings && (
        <Settings
          onSave={handleSaveSettings}
          onClose={() => setShowSettings(false)}
        />
      )}

      {/* Instructions */}
      <div className="instructions">
        <p>
          Press <kbd>S</kbd> for Settings • <kbd>I</kbd> to toggle status bar
        </p>
      </div>
    </div>
  );
}

export default App;

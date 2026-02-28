import React, { useState } from 'react';
import { useSelector } from 'react-redux';
import { DefaultMqttSettings } from '../constants';
import { selectSettings } from '../store/settingsSlice';
import { selectIsConnected } from '../store/mqttSlice';
import type { MqttSettings } from '../types';
import './Settings.css';

interface SettingsProps {
  onSave: (settings: MqttSettings) => void;
  onClose: () => void;
}

const Settings: React.FC<SettingsProps> = ({ onSave, onClose }) => {
  const settings = useSelector(selectSettings);
  const isConnected = useSelector(selectIsConnected);

  const [broker, setBroker] = useState<string>(settings.broker || DefaultMqttSettings.broker);
  const [username, setUsername] = useState<string>(settings.username || '');
  const [password, setPassword] = useState<string>(settings.password || '');
  const [brightness, setBrightness] = useState<number>(settings.brightness || 200);

  const handleSave = () => {
    const newSettings = {
      ...settings,
      broker,
      username,
      password,
      brightness
    };
    onSave(newSettings);
  };

  return (
    <div className="settings-overlay" onClick={onClose}>
      <div className="settings-panel" onClick={(e) => e.stopPropagation()}>
        <div className="settings-header">
          <h2>Settings</h2>
          <button className="close-button" onClick={onClose}>×</button>
        </div>

        <div className="settings-content">
          <div className="settings-section">
            <h3>MQTT Connection</h3>

            <div className="form-group">
              <label htmlFor="broker">Broker URL</label>
              <input
                id="broker"
                type="text"
                value={broker}
                onChange={(e) => setBroker(e.target.value)}
                placeholder="ws://localhost:9001"
                disabled={isConnected}
              />
              <small>WebSocket URL (ws:// or wss://)</small>
            </div>

            <div className="form-group">
              <label htmlFor="username">Username (optional)</label>
              <input
                id="username"
                type="text"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                disabled={isConnected}
              />
            </div>

            <div className="form-group">
              <label htmlFor="password">Password (optional)</label>
              <input
                id="password"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                disabled={isConnected}
              />
            </div>
          </div>

          <div className="settings-section">
            <h3>Display</h3>

            <div className="form-group">
              <label htmlFor="brightness">
                Brightness: {Math.round((brightness / 255) * 100)}%
              </label>
              <input
                id="brightness"
                type="range"
                min="10"
                max="255"
                value={brightness}
                onChange={(e) => setBrightness(parseInt(e.target.value))}
              />
            </div>
          </div>

          <div className="settings-section">
            <h3>Information</h3>
            <div className="info-group">
              <div className="info-row">
                <span className="info-label">Status:</span>
                <span className={`info-value ${isConnected ? 'connected' : 'disconnected'}`}>
                  {isConnected ? 'Connected' : 'Disconnected'}
                </span>
              </div>
              <div className="info-row">
                <span className="info-label">Client ID:</span>
                <span className="info-value">{settings.clientId}</span>
              </div>
            </div>
          </div>
        </div>

        <div className="settings-footer">
          <button className="button button-secondary" onClick={onClose}>
            Cancel
          </button>
          <button className="button button-primary" onClick={handleSave}>
            Save Settings
          </button>
        </div>
      </div>
    </div>
  );
};

export default Settings;

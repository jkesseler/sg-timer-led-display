import { useState, useEffect, useCallback } from 'react';
import LEDMatrix from './components/LEDMatrix';
import Settings from './components/Settings';
import { useMqtt } from './hooks/useMqtt';
import { DisplayState, ConnectionState, MqttTopics } from './constants';
import { storage } from './utils';
import type {
  ShotData,
  SessionData,
  MqttSettings,
  ConnectionStateMessage,
  SessionStartedMessage,
  SessionStoppedMessage,
  ShotDetectedMessage,
  CountdownCompleteMessage,
  DeviceInfoMessage
} from './types';
import './App.css';

function App() {
  // MQTT connection
  const { isConnected, connectionError, settings, onMessage, updateSettings, connect, disconnect } = useMqtt();

  // Display state
  const [displayState, setDisplayState] = useState<DisplayState>(DisplayState.STARTUP);
  const [shotData, setShotData] = useState<ShotData | null>(null);
  const [sessionData, setSessionData] = useState<SessionData | null>(null);
  const [deviceName, setDeviceName] = useState<string | null>(null);
  const [brightness, setBrightness] = useState<number>(() => storage.get('brightness', 200));

  // UI state
  const [showSettings, setShowSettings] = useState<boolean>(false);
  const [showStatus, setShowStatus] = useState<boolean>(true);

  // Handle startup sequence
  useEffect(() => {
    const timer = setTimeout(() => {
      if (displayState === DisplayState.STARTUP) {
        setDisplayState(isConnected ? DisplayState.DISCONNECTED : DisplayState.DISCONNECTED);
      }
    }, 3000); // Show startup for 3 seconds

    return () => clearTimeout(timer);
  }, [displayState, isConnected]);

  // MQTT Message Handlers
  useEffect(() => {
    // Connection state changes
    onMessage<ConnectionStateMessage>(MqttTopics.CONNECTION_STATE, (message) => {
      console.log('Connection state:', message);
      setDeviceName(message.deviceName || null);

      switch (message.state) {
        case ConnectionState.CONNECTED:
          setDisplayState(DisplayState.CONNECTED);
          break;
        case ConnectionState.SCANNING:
          setDisplayState(DisplayState.SCANNING);
          break;
        case ConnectionState.CONNECTING:
          setDisplayState(DisplayState.CONNECTING);
          break;
        case ConnectionState.DISCONNECTED:
        case ConnectionState.ERROR:
          setDisplayState(DisplayState.DISCONNECTED);
          setDeviceName(null);
          break;
        default:
          break;
      }
    });

    // Session started (with countdown)
    onMessage<SessionStartedMessage>(MqttTopics.SESSION_STARTED, (message) => {
      console.log('Session started:', message);
      const newSessionData: SessionData = {
        sessionId: message.sessionId,
        isActive: true,
        totalShots: 0,
        startTimestamp: message.timestamp,
        startDelaySeconds: message.startDelaySeconds || 0,
        countdownStartTime: Date.now()
      };
      setSessionData(newSessionData);

      if (message.startDelaySeconds && message.startDelaySeconds > 0) {
        setDisplayState(DisplayState.COUNTDOWN);
        // Auto-transition after countdown
        setTimeout(() => {
          setDisplayState(DisplayState.WAITING_FOR_SHOTS);
        }, message.startDelaySeconds * 1000);
      } else {
        setDisplayState(DisplayState.WAITING_FOR_SHOTS);
      }
    });

    // Countdown complete
    onMessage<CountdownCompleteMessage>(MqttTopics.COUNTDOWN_COMPLETE, (message) => {
      console.log('Countdown complete:', message);
      setDisplayState(DisplayState.WAITING_FOR_SHOTS);
    });

    // Shot detected
    onMessage<ShotDetectedMessage>(MqttTopics.SHOT_DETECTED, (message) => {
      console.log('Shot detected:', message);
      setShotData({
        sessionId: message.sessionId,
        shotNumber: message.shotNumber,
        absoluteTimeMs: message.absoluteTimeMs,
        splitTimeMs: message.splitTimeMs,
        deviceModel: message.deviceModel,
        isFirstShot: message.isFirstShot,
        timestamp: message.timestamp
      });
      setDisplayState(DisplayState.SHOWING_SHOT);

      // Update session shot count
      if (sessionData) {
        setSessionData({
          ...sessionData,
          totalShots: Math.max(sessionData.totalShots, message.shotNumber)
        });
      }
    });

    // Session stopped
    onMessage<SessionStoppedMessage>(MqttTopics.SESSION_STOPPED, (message) => {
      console.log('Session stopped:', message);
      if (sessionData) {
        setSessionData({
          ...sessionData,
          isActive: false,
          totalShots: message.totalShots || sessionData.totalShots
        });
      }
      setDisplayState(DisplayState.SESSION_ENDED);

      // Auto-clear after 10 seconds
      setTimeout(() => {
        setDisplayState(DisplayState.CONNECTED);
        setShotData(null);
      }, 10000);
    });

    // Device info
    onMessage<DeviceInfoMessage>(MqttTopics.DEVICE_INFO, (message) => {
      console.log('Device info:', message);
      setDeviceName(message.deviceName || message.deviceModel || null);
    });
  }, [onMessage, sessionData]);

  // Update brightness in storage
  useEffect(() => {
    storage.set('brightness', brightness);
  }, [brightness]);

  // Handle settings save
  const handleSaveSettings = useCallback((newSettings: MqttSettings) => {
    const needsReconnect = newSettings.broker !== settings.broker ||
                          newSettings.username !== settings.username ||
                          newSettings.password !== settings.password;

    setBrightness(newSettings.brightness || 200);
    updateSettings(newSettings);

    if (needsReconnect) {
      disconnect();
      setTimeout(() => connect(newSettings), 500);
    }

    setShowSettings(false);
  }, [settings, updateSettings, disconnect, connect]);

  // Keyboard shortcuts
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
            <div className="status-item error">
              Error: {connectionError}
            </div>
          )}
          <div className="status-item">
            {settings.broker}
          </div>
        </div>
      )}

      {/* Main Display */}
      <LEDMatrix
        displayState={displayState}
        shotData={shotData}
        sessionData={sessionData}
        connectionState={isConnected ? 'CONNECTED' : 'DISCONNECTED'}
        deviceName={deviceName}
        brightness={brightness}
      />

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
          onClick={() => setShowStatus(prev => !prev)}
          title="Toggle Status (I)"
        >
          {showStatus ? '🔼' : '🔽'} Status
        </button>
        <button
          className="control-button"
          onClick={() => setBrightness(prev => Math.min(255, prev + 25))}
          title="Increase Brightness"
        >
          🔆 +
        </button>
        <button
          className="control-button"
          onClick={() => setBrightness(prev => Math.max(10, prev - 25))}
          title="Decrease Brightness"
        >
          🔅 -
        </button>
      </div>

      {/* Settings Modal */}
      {showSettings && (
        <Settings
          settings={{ ...settings, brightness }}
          onSave={handleSaveSettings}
          onClose={() => setShowSettings(false)}
          isConnected={isConnected}
        />
      )}

      {/* Instructions */}
      <div className="instructions">
        <p>Press <kbd>S</kbd> for Settings • <kbd>I</kbd> to toggle status bar</p>
      </div>
    </div>
  );
}

export default App;

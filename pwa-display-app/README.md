# SG Timer PWA Display

A Progressive Web App (PWA) that mimics the HUB75 LED matrix display for shot timer data. Instead of using BLE, this app receives data via MQTT from an ESP32 bridge device.

![Display States](docs/screenshots/display-states-preview.png)

## Features

### 🎯 LED Matrix Simulation
- **Pixel-perfect 128x32 display** matching physical HUB75 panels
- **Canvas-based rendering** with authentic LED appearance
- **Color-accurate RGB565** matching ESP32 firmware
- **Responsive scaling** adapts to any screen size
- **Scrolling text** for long messages

### 📡 Real-time MQTT Integration
- **WebSocket MQTT client** for real-time updates
- **Auto-reconnect** with 5-second retry interval
- **Connection status** display
- **Multiple topics** for different event types
- **Persistent settings** saved in localStorage

### 🎨 Display States
All states from the ESP32 firmware:
- **STARTUP** - Green scrolling boot message
- **DISCONNECTED** - Red "NO DEVICE"
- **SCANNING** - Yellow "SCANNING..."
- **CONNECTING** - Yellow "CONNECTING..."
- **CONNECTED** - Light blue with device name
- **COUNTDOWN** - Pre-session countdown timer
- **WAITING_FOR_SHOTS** - "READY" state
- **SHOWING_SHOT** - Shot number and time
- **SESSION_ENDED** - Summary with statistics

### 📱 PWA Capabilities
- **Installable** on mobile and desktop
- **Offline-ready** with service worker caching
- **Responsive design** works on all devices
- **Landscape optimized** for best viewing
- **Add to home screen** for app-like experience

### ⚙️ Configuration
- **MQTT broker settings** (URL, username, password)
- **Brightness control** (10-255)
- **Keyboard shortcuts** (S for settings, I for status)
- **Persistent storage** across sessions

## Architecture

```
┌─────────────┐         ┌──────────────┐         ┌──────────────┐
│ Shot Timer  │   BLE   │   ESP32-S3   │  WiFi   │ MQTT Broker  │
│  (SG/Pie)   ├────────>│    Bridge    ├────────>│ (Mosquitto)  │
└─────────────┘         └──────┬───────┘         └──────┬───────┘
                               │                         │
                               │ (optional)              │ WebSocket
                               v                         v
                        ┌──────────────┐         ┌──────────────┐
                        │  HUB75 LED   │         │  PWA Display │
                        │   Display    │         │ (Browser)    │
                        └──────────────┘         └──────────────┘
```

The ESP32 acts as a WiFi client, receiving BLE data from shot timers and publishing to MQTT topics for display.

## MQTT Topics

### Published by ESP32 (Subscribed by PWA)

- `timer/connection/state` - Connection state changes
- `timer/session/started` - Session start event with countdown
- `timer/session/stopped` - Session end event
- `timer/shot/detected` - Shot detection with timing data
- `timer/device/info` - Device information

### Message Formats

#### Connection State
```json
{
  "state": "CONNECTED|DISCONNECTED|SCANNING|CONNECTING",
  "deviceName": "SG Timer Sport",
  "timestamp": 1234567890
}
```

#### Session Started
```json
{
  "sessionId": 12345,
  "startDelaySeconds": 3.0,
  "timestamp": 1234567890
}
```

#### Shot Detected
```json
{
  "sessionId": 12345,
  "shotNumber": 5,
  "absoluteTimeMs": 12345,
  "splitTimeMs": 1778,
  "deviceModel": "SG Timer Sport",
  "isFirstShot": false,
  "timestamp": 1234567890
}
```

#### Session Stopped
```json
{
  "sessionId": 12345,
  "totalShots": 15,
  "timestamp": 1234567890
}
```

## Display States

1. **STARTUP** - Boot message (green text)
2. **DISCONNECTED** - No device connected
3. **SCANNING** - Looking for devices (yellow)
4. **CONNECTING** - Establishing connection
5. **CONNECTED** - Device ready (light blue)
6. **COUNTDOWN** - Pre-session countdown
7. **WAITING_FOR_SHOTS** - Session active, waiting
8. **SHOWING_SHOT** - Display shot data
9. **SESSION_ENDED** - Final statistics (red)

## Development

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Build for production
npm run build

# Preview production build
npm run preview
```

## Configuration

Access settings via the gear icon to configure:
- MQTT broker URL and port
- Username/password (if required)
- Display brightness
- Color scheme preferences

## Deployment

The PWA can be deployed to any static hosting service:
- Netlify
- Vercel
- GitHub Pages
- Firebase Hosting
- Or self-hosted on local network

## Browser Compatibility

- Chrome/Edge 90+
- Safari 15.4+
- Firefox 90+

Requires WebSocket support for MQTT over WebSocket connection.

## Screenshots

### Display States
![Startup](docs/screenshots/startup.png)
![Connected](docs/screenshots/connected.png)
![Shot Display](docs/screenshots/shot.png)
![Session End](docs/screenshots/session-end.png)

### Settings UI
![Settings Panel](docs/screenshots/settings.png)

## Project Structure

```
pwa-display-app/
├── src/
│   ├── components/
│   │   ├── LEDMatrix.jsx       # Canvas-based LED display
│   │   └── Settings.jsx        # Configuration modal
│   ├── hooks/
│   │   └── useMqtt.js          # MQTT connection management
│   ├── App.jsx                 # Main application
│   ├── constants.js            # Display states & colors
│   └── utils.js                # Helper functions
├── docs/
│   ├── DEVELOPMENT.md          # Development guide
│   ├── TESTING.md              # Testing procedures
│   └── ESP32-MQTT-BRIDGE.md    # ESP32 implementation
├── public/                     # Static assets
├── index.html
├── vite.config.js              # Build configuration
└── package.json
```

## Technology Stack

- **React 18** - UI framework
- **Vite** - Build tool and dev server
- **MQTT.js** - MQTT client for WebSocket
- **Canvas API** - LED matrix rendering
- **Vite PWA Plugin** - Progressive web app features
- **localStorage** - Settings persistence

## Use Cases

### Primary Use Case
Display shot timer data on a large screen (TV, monitor, tablet) at a shooting range for spectators and competitors.

### Alternative Uses
- **Portable display** on a tablet instead of LED matrix
- **Multi-display setup** multiple screens from one timer
- **Remote viewing** display data over network/internet
- **Recording sessions** screen capture for video analysis
- **Testing/development** simulate timer without hardware

## Contributing

This is part of the SG Timer LED Display project. See main project README for contribution guidelines.

## Roadmap

- [ ] Historical session data storage
- [ ] Shot list replay feature
- [ ] Customizable color schemes
- [ ] Graph/chart visualizations
- [ ] Multi-timer support
- [ ] Audio feedback for shots
- [ ] Session statistics dashboard

## Acknowledgments

- Inspired by the ESP32 HUB75 LED matrix firmware
- Built for competitive shooting sports community
- Uses open-source MQTT protocol

## License

Same as parent SG Timer LED Display project.

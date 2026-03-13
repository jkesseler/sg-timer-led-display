# PWA Display App - Development Guide

## Quick Start

```bash
# Install dependencies
npm install

# Run development server
npm run dev

# Build for production
npm run build

# Preview production build
npm run preview
```

The app will be available at `http://localhost:3000`

## Project Structure

```
pwa-display-app/
├── src/
│   ├── components/
│   │   ├── LEDMatrix.jsx       # Main LED matrix display component
│   │   ├── LEDMatrix.css       # Display styling with scanline effects
│   │   ├── Settings.jsx        # Settings modal component
│   │   └── Settings.css        # Settings styling
│   ├── hooks/
│   │   └── useMqtt.js          # MQTT connection hook
│   ├── App.jsx                 # Main application component
│   ├── App.css                 # Application styling
│   ├── main.jsx                # React entry point
│   ├── index.css               # Global styles
│   ├── constants.js            # Display states, colors, MQTT topics
│   └── utils.js                # Utility functions
├── public/                     # Static assets (icons, manifest)
├── index.html                  # HTML entry point
├── vite.config.js              # Vite + PWA configuration
└── package.json                # Dependencies
```

## MQTT Configuration

### Broker Setup

The app expects an MQTT broker with WebSocket support. Common options:

1. **Mosquitto** (recommended for local development)
   ```bash
   # Install mosquitto
   # Add to mosquitto.conf:
   listener 9001
   protocol websockets

   # Start broker
   mosquitto -c mosquitto.conf
   ```

2. **HiveMQ Cloud** (cloud-based)
   - Free tier available
   - WebSocket URL: `wss://your-cluster.s1.eu.hivemq.cloud:8884/mqtt`

3. **EMQX** (self-hosted or cloud)
   - Docker: `docker run -d --name emqx -p 1883:1883 -p 8083:8083 -p 8084:8084 -p 8883:8883 -p 18083:18083 emqx/emqx`
   - WebSocket port: 8083

### Default Settings

```javascript
{
  broker: 'ws://localhost:9001',
  username: '',
  password: '',
  clientId: 'pwa-display-xxxxxx'
}
```

Settings are persisted in localStorage and can be changed via the Settings UI.

## MQTT Message Format

### Connection State
**Topic:** `timer/connection/state`
```json
{
  "state": "CONNECTED",
  "deviceName": "SG Timer Sport",
  "timestamp": 1702345678000
}
```
States: `DISCONNECTED`, `SCANNING`, `CONNECTING`, `CONNECTED`, `ERROR`

### Session Started
**Topic:** `timer/session/started`
```json
{
  "sessionId": 12345,
  "startDelaySeconds": 3.0,
  "timestamp": 1702345678000
}
```

### Shot Detected
**Topic:** `timer/shot/detected`
```json
{
  "sessionId": 12345,
  "shotNumber": 5,
  "absoluteTimeMs": 12345,
  "splitTimeMs": 1778,
  "deviceModel": "SG Timer Sport",
  "isFirstShot": false,
  "timestamp": 1702345678000
}
```

### Session Stopped
**Topic:** `timer/session/stopped`
```json
{
  "sessionId": 12345,
  "totalShots": 15,
  "timestamp": 1702345678000
}
```

## Display States

The app implements all display states from the ESP32 firmware:

1. **STARTUP** - Green scrolling boot message
2. **DISCONNECTED** - Red "NO DEVICE" text
3. **SCANNING** - Yellow "SCANNING..." text
4. **CONNECTING** - Yellow "CONNECTING..." text
5. **CONNECTED** - Light blue "CONNECTED" with device name
6. **COUNTDOWN** - Pre-session countdown timer
7. **WAITING_FOR_SHOTS** - "READY" with 00:00.000
8. **SHOWING_SHOT** - Shot number and time
9. **SESSION_ENDED** - Red "SESSION END" with stats

## Features

### LED Matrix Display
- **Resolution:** 128x32 pixels (matching HUB75 panels)
- **Responsive:** Auto-scales to fit screen
- **Pixel-perfect:** Canvas rendering with `pixelated` image-rendering
- **Color accurate:** Matches RGB565 colors from firmware
- **Animations:** Scrolling text for long messages

### MQTT Integration
- **Auto-reconnect:** 5-second reconnect interval
- **Topic subscription:** All timer topics auto-subscribed
- **Error handling:** Connection error display
- **Persistence:** Settings saved to localStorage

### PWA Features
- **Installable:** Add to home screen on mobile/desktop
- **Offline-ready:** Service worker caching
- **Responsive:** Works on phones, tablets, laptops
- **Landscape optimized:** Best in landscape mode

### Controls
- **Brightness:** Adjustable from 10-255
- **Settings:** MQTT broker configuration
- **Status bar:** Connection status, broker URL
- **Keyboard shortcuts:**
  - `S` - Open settings
  - `I` - Toggle status bar
  - `ESC` - Close settings

## Testing

### Manual MQTT Testing

Use an MQTT client to send test messages:

```bash
# Using mosquitto_pub
mosquitto_pub -h localhost -t timer/connection/state -m '{"state":"CONNECTED","deviceName":"Test Timer"}'

mosquitto_pub -h localhost -t timer/session/started -m '{"sessionId":123,"startDelaySeconds":3.0,"timestamp":1702345678000}'

mosquitto_pub -h localhost -t timer/shot/detected -m '{"sessionId":123,"shotNumber":1,"absoluteTimeMs":2345,"splitTimeMs":0,"deviceModel":"Test","isFirstShot":true,"timestamp":1702345678000}'

mosquitto_pub -h localhost -t timer/session/stopped -m '{"sessionId":123,"totalShots":10,"timestamp":1702345678000}'
```

### Development Mode

```bash
npm run dev
```
- Hot module replacement enabled
- Errors shown in browser console
- MQTT messages logged to console

### Production Build

```bash
npm run build
npm run preview
```
- Minified and optimized
- Service worker registered
- Static assets cached

## Deployment

### Static Hosting

Build and deploy to any static host:

```bash
npm run build
# Upload dist/ folder to:
# - Netlify
# - Vercel
# - GitHub Pages
# - Firebase Hosting
# - Your own server
```

### Local Network

For local network access (e.g., tablet on same WiFi):

1. Build the app: `npm run build`
2. Serve with any static server:
   ```bash
   npx serve dist -l 3000
   ```
3. Access from other devices: `http://YOUR_IP:3000`

### HTTPS for PWA

PWAs require HTTPS in production. Options:

1. **Let's Encrypt** - Free SSL certificates
2. **Cloudflare** - Free SSL proxy
3. **Hosting platform** - Most provide free SSL

## Customization

### Display Colors

Edit `src/constants.js`:
```javascript
export const DisplayColors = {
  RED: '#FF0000',
  GREEN: '#00FF00',
  // ... customize colors
};
```

### Display Size

Edit `src/constants.js`:
```javascript
export const DisplayConfig = {
  PANEL_WIDTH: 64,
  PANEL_HEIGHT: 32,
  PANEL_CHAIN: 2,
  PIXEL_SIZE: 4  // CSS pixels per LED pixel
};
```

### Visual Effects

Enable/disable in `src/components/LEDMatrix.jsx`:
```css
/* Scanline effect */
.led-matrix-container.scanlines canvas { ... }

/* Glow effect */
.led-matrix-container.glow canvas { ... }
```

## Troubleshooting

### MQTT Connection Fails
- Check broker URL (must start with `ws://` or `wss://`)
- Verify broker has WebSocket listener enabled
- Check firewall/network settings
- Try connecting with MQTT Explorer first

### Display Not Updating
- Open browser console for MQTT messages
- Verify messages match expected JSON format
- Check topic names match constants
- Ensure timestamps are in milliseconds

### PWA Not Installing
- Requires HTTPS (except localhost)
- Check manifest.json is served correctly
- Verify service worker registration
- Use Lighthouse audit in Chrome DevTools

### Performance Issues
- Reduce pixel size in responsive scaling
- Disable visual effects (scanlines, glow)
- Check for console errors
- Limit MQTT message rate

## Browser Compatibility

- **Chrome/Edge:** 90+
- **Safari:** 15.4+
- **Firefox:** 90+

Requires:
- Canvas API
- WebSocket support
- Service Worker API (for PWA)
- localStorage

## License

Same as parent SG Timer LED Display project.

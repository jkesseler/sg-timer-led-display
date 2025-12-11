# Quick Start Guide - PWA Display

Get the PWA display running in 5 minutes!

## Prerequisites

- Node.js 18+ installed
- MQTT broker (Mosquitto recommended)
- Modern web browser

## Step 1: Install Dependencies

```bash
cd pwa-display-app
npm install
```

## Step 2: Start MQTT Broker

### Option A: Local Mosquitto (Recommended)

**Windows:**
```powershell
# Install via Chocolatey
choco install mosquitto

# Create config file: C:\Program Files\mosquitto\mosquitto.conf
listener 1883
listener 9001
protocol websockets
allow_anonymous true

# Start service
net start mosquitto
```

**Linux/Mac:**
```bash
# Install
sudo apt-get install mosquitto  # Ubuntu/Debian
brew install mosquitto           # macOS

# Edit config
sudo nano /etc/mosquitto/mosquitto.conf
# Add the same lines as Windows above

# Start/restart
sudo systemctl restart mosquitto  # Linux
brew services restart mosquitto   # macOS
```

### Option B: Docker (Easiest)

```bash
docker run -d --name mosquitto \
  -p 1883:1883 \
  -p 9001:9001 \
  -v $PWD/mosquitto.conf:/mosquitto/config/mosquitto.conf \
  eclipse-mosquitto
```

Create `mosquitto.conf`:
```
listener 1883
listener 9001
protocol websockets
allow_anonymous true
```

### Option C: Cloud Broker

Use HiveMQ Cloud (free tier):
1. Sign up at https://www.hivemq.com/mqtt-cloud-broker/
2. Get WebSocket URL (wss://...)
3. Use in settings (step 4)

## Step 3: Start Development Server

```bash
npm run dev
```

App runs at: `http://localhost:3000`

## Step 4: Configure MQTT Connection

1. Press `S` to open Settings
2. Set broker URL:
   - Local: `ws://localhost:9001`
   - Cloud: `wss://your-cluster.hivemq.cloud:8884/mqtt`
3. Add username/password if required
4. Click "Save Settings"
5. Check status bar shows "MQTT Connected" ✅

## Step 5: Test with Sample Data

Open a new terminal and send test messages:

```bash
# Test connection
mosquitto_pub -h localhost -t timer/connection/state \
  -m '{"state":"CONNECTED","deviceName":"Test Timer","timestamp":1702345678000}'

# Test shot detection
mosquitto_pub -h localhost -t timer/shot/detected \
  -m '{"sessionId":123,"shotNumber":1,"absoluteTimeMs":2345,"splitTimeMs":0,"deviceModel":"Test","isFirstShot":true,"timestamp":1702345678000}'
```

**PowerShell:**
```powershell
# Test connection
mosquitto_pub -h localhost -t timer/connection/state `
  -m '{"state":"CONNECTED","deviceName":"Test Timer","timestamp":1702345678000}'

# Test shot
mosquitto_pub -h localhost -t timer/shot/detected `
  -m '{"sessionId":123,"shotNumber":1,"absoluteTimeMs":2345,"splitTimeMs":0,"deviceModel":"Test","isFirstShot":true,"timestamp":1702345678000}'
```

## Step 6: Run Complete Test Session

Use the automated test script:

**Linux/Mac:**
```bash
chmod +x docs/test-session.sh
./docs/test-session.sh
```

**Windows:**
```powershell
.\docs\test-session.ps1
```

You should see:
1. Connection message
2. 3-second countdown
3. 10 shots fired (one per second)
4. Session end summary

## Keyboard Shortcuts

- `S` - Open Settings
- `I` - Toggle status bar
- `ESC` - Close settings
- `+` / `-` buttons - Adjust brightness

## Troubleshooting

### MQTT Won't Connect

**Check broker is running:**
```bash
# Test with mosquitto_sub
mosquitto_sub -h localhost -t test

# In another terminal:
mosquitto_pub -h localhost -t test -m "hello"
```

If you see "hello", broker is working.

**Check browser console:**
- Press F12 → Console tab
- Look for WebSocket errors
- Verify broker URL is correct (ws:// not http://)

### Display Not Updating

1. Check MQTT messages in browser console (should see logs)
2. Verify JSON format is correct
3. Try clearing localStorage: `localStorage.clear()` in console
4. Refresh page

### PWA Won't Install

- Must use HTTPS in production (localhost is OK for dev)
- Check manifest.json is accessible
- Use Chrome DevTools → Application → Manifest

## Next Steps

1. **Production Build:**
   ```bash
   npm run build
   npm run preview
   ```

2. **Deploy:** Upload `dist/` folder to:
   - Netlify (drag & drop)
   - Vercel (`vercel deploy`)
   - GitHub Pages
   - Your own server

3. **Connect ESP32:** Follow `docs/ESP32-MQTT-BRIDGE.md` to set up the real hardware bridge

4. **Customize:** Edit colors, text, or display logic in `src/components/LEDMatrix.jsx`

## Common Issues

| Issue | Solution |
|-------|----------|
| Port 9001 in use | Change mosquitto config or use different port |
| Can't install mosquitto_pub | Install mosquitto-clients package |
| Browser CORS error | Ensure broker allows WebSocket connections |
| Display too small/large | Adjust window size, display auto-scales |
| Settings not saving | Check browser allows localStorage |

## Learn More

- **Development Guide:** `docs/DEVELOPMENT.md`
- **Testing Guide:** `docs/TESTING.md`
- **ESP32 Integration:** `docs/ESP32-MQTT-BRIDGE.md`
- **Main Project:** `../README.md`

## Support

- Check browser console for errors (F12)
- Review MQTT message format in docs
- Verify broker configuration
- Test with MQTT Explorer desktop app first

---

**Congratulations! 🎉** Your PWA display should now be running and ready to receive shot timer data.

For the full development workflow, see `docs/DEVELOPMENT.md`.

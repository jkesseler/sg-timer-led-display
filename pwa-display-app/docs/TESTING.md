# SG Timer PWA Display - Testing Guide

## Test Scenarios

### 1. MQTT Connection Testing

#### Setup Local Mosquitto Broker

**Windows (PowerShell):**
```powershell
# Install Mosquitto (using Chocolatey)
choco install mosquitto

# Create config file
@"
listener 1883
listener 9001
protocol websockets
allow_anonymous true
"@ | Out-File -FilePath "C:\Program Files\mosquitto\mosquitto.conf" -Encoding ASCII

# Start broker
net start mosquitto
```

**Linux/Mac:**
```bash
# Install mosquitto
sudo apt-get install mosquitto mosquitto-clients  # Ubuntu/Debian
brew install mosquitto                             # macOS

# Edit config
sudo nano /etc/mosquitto/mosquitto.conf
# Add:
listener 1883
listener 9001
protocol websockets
allow_anonymous true

# Restart
sudo systemctl restart mosquitto  # Linux
brew services restart mosquitto   # macOS
```

#### Verify Broker

```bash
# Subscribe to all timer topics
mosquitto_sub -h localhost -t 'timer/#' -v

# In another terminal, publish test message
mosquitto_pub -h localhost -t timer/connection/state -m '{"state":"CONNECTED","deviceName":"Test Timer","timestamp":1702345678000}'
```

### 2. PWA Functionality Tests

#### Installation Test
1. Build production: `npm run build`
2. Serve: `npm run preview`
3. Open in Chrome
4. Check console for "Service worker registered"
5. Click install prompt (⊕ icon in address bar)
6. Verify app opens in standalone window

#### Offline Test
1. Start app with MQTT connected
2. Open DevTools → Network → Offline
3. Refresh page
4. App should load (cached by service worker)
5. MQTT won't connect (expected)
6. Go back online
7. MQTT should auto-reconnect

#### Settings Persistence
1. Open Settings (S key)
2. Change broker URL
3. Change brightness
4. Close app/refresh
5. Open Settings again
6. Verify settings persisted

### 3. Display State Testing

Send test messages to verify all display states:

```bash
# 1. STARTUP (automatic on page load)

# 2. DISCONNECTED
mosquitto_pub -h localhost -t timer/connection/state \
  -m '{"state":"DISCONNECTED","timestamp":1702345678000}'

# 3. SCANNING
mosquitto_pub -h localhost -t timer/connection/state \
  -m '{"state":"SCANNING","timestamp":1702345678000}'

# 4. CONNECTING
mosquitto_pub -h localhost -t timer/connection/state \
  -m '{"state":"CONNECTING","timestamp":1702345678000}'

# 5. CONNECTED
mosquitto_pub -h localhost -t timer/connection/state \
  -m '{"state":"CONNECTED","deviceName":"SG Timer Sport","timestamp":1702345678000}'

# 6. COUNTDOWN (session with 3 second delay)
mosquitto_pub -h localhost -t timer/session/started \
  -m '{"sessionId":12345,"startDelaySeconds":3.0,"timestamp":1702345678000}'

# 7. WAITING_FOR_SHOTS (after countdown or immediate session)
# (automatic after countdown completes)

# 8. SHOWING_SHOT
mosquitto_pub -h localhost -t timer/shot/detected \
  -m '{"sessionId":12345,"shotNumber":1,"absoluteTimeMs":2345,"splitTimeMs":0,"deviceModel":"SG Timer","isFirstShot":true,"timestamp":1702345678000}'

mosquitto_pub -h localhost -t timer/shot/detected \
  -m '{"sessionId":12345,"shotNumber":5,"absoluteTimeMs":12789,"splitTimeMs":1778,"deviceModel":"SG Timer","isFirstShot":false,"timestamp":1702345678000}'

# 9. SESSION_ENDED
mosquitto_pub -h localhost -t timer/session/stopped \
  -m '{"sessionId":12345,"totalShots":15,"timestamp":1702345678000}'
```

### 4. Complete Session Simulation

Automated script to simulate a complete shooting session:

**test-session.sh:**
```bash
#!/bin/bash
HOST="localhost"
TOPIC_PREFIX="timer"

echo "=== Starting Test Session ==="

echo "1. Device connected..."
mosquitto_pub -h $HOST -t ${TOPIC_PREFIX}/connection/state \
  -m '{"state":"CONNECTED","deviceName":"Test Timer","timestamp":'$(date +%s)000'}'
sleep 2

echo "2. Starting session with 3s countdown..."
mosquitto_pub -h $HOST -t ${TOPIC_PREFIX}/session/started \
  -m '{"sessionId":99999,"startDelaySeconds":3.0,"timestamp":'$(date +%s)000'}'
sleep 4

echo "3. Firing 10 shots..."
for i in {1..10}; do
  TIME=$((2000 + i * 1500))
  SPLIT=1500
  if [ $i -eq 1 ]; then
    SPLIT=0
  fi

  mosquitto_pub -h $HOST -t ${TOPIC_PREFIX}/shot/detected \
    -m '{"sessionId":99999,"shotNumber":'$i',"absoluteTimeMs":'$TIME',"splitTimeMs":'$SPLIT',"deviceModel":"Test Timer","isFirstShot":'$([ $i -eq 1 ] && echo "true" || echo "false")',"timestamp":'$(date +%s)000'}'

  sleep 1
done

echo "4. Ending session..."
sleep 2
mosquitto_pub -h $HOST -t ${TOPIC_PREFIX}/session/stopped \
  -m '{"sessionId":99999,"totalShots":10,"timestamp":'$(date +%s)000'}'

echo "=== Test Complete ==="
```

**PowerShell version (test-session.ps1):**
```powershell
$HOST = "localhost"
$TOPIC_PREFIX = "timer"

Write-Host "=== Starting Test Session ==="

Write-Host "1. Device connected..."
mosquitto_pub -h $HOST -t "$TOPIC_PREFIX/connection/state" `
  -m "{`"state`":`"CONNECTED`",`"deviceName`":`"Test Timer`",`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"
Start-Sleep -Seconds 2

Write-Host "2. Starting session with 3s countdown..."
mosquitto_pub -h $HOST -t "$TOPIC_PREFIX/session/started" `
  -m "{`"sessionId`":99999,`"startDelaySeconds`":3.0,`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"
Start-Sleep -Seconds 4

Write-Host "3. Firing 10 shots..."
for ($i = 1; $i -le 10; $i++) {
  $TIME = 2000 + ($i * 1500)
  $SPLIT = if ($i -eq 1) { 0 } else { 1500 }
  $IS_FIRST = if ($i -eq 1) { "true" } else { "false" }

  mosquitto_pub -h $HOST -t "$TOPIC_PREFIX/shot/detected" `
    -m "{`"sessionId`":99999,`"shotNumber`":$i,`"absoluteTimeMs`":$TIME,`"splitTimeMs`":$SPLIT,`"deviceModel`":`"Test Timer`",`"isFirstShot`":$IS_FIRST,`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"

  Start-Sleep -Seconds 1
}

Write-Host "4. Ending session..."
Start-Sleep -Seconds 2
mosquitto_pub -h $HOST -t "$TOPIC_PREFIX/session/stopped" `
  -m "{`"sessionId`":99999,`"totalShots`":10,`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"

Write-Host "=== Test Complete ==="
```

### 5. Performance Testing

#### Rapid Shot Rate
Test display updates at high frequency:
```bash
# Fire 20 shots in 5 seconds (0.25s intervals)
for i in {1..20}; do
  TIME=$((i * 250))
  mosquitto_pub -h localhost -t timer/shot/detected \
    -m '{"sessionId":88888,"shotNumber":'$i',"absoluteTimeMs":'$TIME',"splitTimeMs":250,"deviceModel":"Speed Test","isFirstShot":false,"timestamp":'$(date +%s)000'}'
  sleep 0.25
done
```

Verify:
- Display updates smoothly
- No lag or stuttering
- Canvas renders correctly
- No memory leaks (check DevTools)

#### Long Session
Test display with many shots:
```bash
# 100 shots simulation
for i in {1..100}; do
  TIME=$((i * 1000))
  mosquitto_pub -h localhost -t timer/shot/detected \
    -m '{"sessionId":77777,"shotNumber":'$i',"absoluteTimeMs":'$TIME',"splitTimeMs":1000,"deviceModel":"Endurance Test","isFirstShot":false,"timestamp":'$(date +%s)000'}'
  sleep 0.5
done
```

### 6. Browser Compatibility

Test in multiple browsers:
- [ ] Chrome 90+ (desktop)
- [ ] Chrome (Android)
- [ ] Safari 15.4+ (iOS)
- [ ] Safari (macOS)
- [ ] Edge 90+
- [ ] Firefox 90+

Check:
- Canvas rendering
- MQTT WebSocket connection
- Service worker registration
- localStorage persistence
- Responsive scaling

### 7. Device Testing

Test on various devices:
- [ ] Desktop (Windows/Mac/Linux)
- [ ] Laptop
- [ ] Tablet (iPad/Android)
- [ ] Large smartphone
- [ ] Small smartphone

Verify:
- Pixel scaling appropriate
- Controls accessible
- Readable text sizes
- Touch interactions work
- Landscape orientation preferred

### 8. Network Conditions

Test under various network conditions:

**Chrome DevTools → Network:**
- Fast 3G
- Slow 3G
- Offline
- Custom (high latency)

Verify:
- Auto-reconnect works
- No crashes on disconnect
- User sees connection status
- Graceful degradation

### 9. Edge Cases

#### Invalid Messages
```bash
# Malformed JSON
mosquitto_pub -h localhost -t timer/shot/detected -m 'invalid json'

# Missing required fields
mosquitto_pub -h localhost -t timer/shot/detected -m '{"sessionId":123}'

# Invalid values
mosquitto_pub -h localhost -t timer/shot/detected \
  -m '{"sessionId":"abc","shotNumber":-1,"absoluteTimeMs":"not a number"}'
```

Verify:
- No crashes
- Errors logged to console
- Display remains functional

#### State Transitions
Test unusual state transitions:
- Shot without session start
- Session end without shots
- Multiple rapid session starts
- Duplicate shot numbers

### 10. Accessibility

- [ ] Keyboard navigation works (S, I, ESC)
- [ ] Color contrast sufficient
- [ ] Text readable at small sizes
- [ ] No flashing/seizure triggers
- [ ] Screen reader compatible (basic)

## Automated Testing

For CI/CD, consider adding:

```bash
# Install dependencies
npm ci

# Lint
npm run lint

# Build
npm run build

# Check build output
ls -la dist/

# Lighthouse CI for PWA score
npm install -g @lhci/cli
lhci autorun --collect.url=http://localhost:3000
```

## Troubleshooting Common Issues

**MQTT not connecting:**
```javascript
// Check browser console for:
- WebSocket connection errors
- CORS issues
- Invalid broker URL
- Authentication failures
```

**Display not updating:**
```javascript
// Enable debug logging:
localStorage.setItem('debug', 'mqtt:*');
// Refresh page and check console
```

**Service worker issues:**
```javascript
// Unregister all service workers:
navigator.serviceWorker.getRegistrations()
  .then(registrations => {
    registrations.forEach(r => r.unregister());
  });
```

## Test Checklist

- [ ] MQTT connection successful
- [ ] All display states render correctly
- [ ] Settings persist across sessions
- [ ] PWA installable
- [ ] Works offline (cached assets)
- [ ] Responsive on all devices
- [ ] Brightness control works
- [ ] Keyboard shortcuts work
- [ ] Auto-reconnect functions
- [ ] No console errors
- [ ] Performance acceptable (60fps)
- [ ] Memory usage stable

## Next Steps

After testing locally:
1. Deploy to staging environment
2. Test with real ESP32 MQTT bridge
3. Test with actual shot timer data
4. Gather user feedback
5. Optimize based on real-world usage

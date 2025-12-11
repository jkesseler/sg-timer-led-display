# MQTT Simulator - Quick Reference

## Installation

```bash
cd mqtt-simulator
npm install
```

## Test Connection

```bash
npm run test-connection
# or with custom broker:
npm run test-connection ws://192.168.1.100:9001
```

## Run Simulations

### State Mode (Jump to Specific Display State) ⚡ NEW
```bash
# Jump to shot display (shot #5 by default)
npm run start state -- --state shot

# Jump to specific shot number
npm run start state -- --state shot --shot-num 10

# Jump to waiting for shots
npm run start state -- --state waiting

# Jump to session ended
npm run start state -- --state ended --total-shots 15

# Jump to connected only
npm run start state -- --state connected
```
**Available states:** `connected`, `waiting`, `shot`, `ended`

### Simple Mode (Default)
```bash
npm run start
```
- 10 shots
- 3s countdown
- 1.5s shot intervals

### Competitive Mode
```bash
npm run start competitive
```
- 12-15 rounds (random)
- Realistic timing (draw, splits, reloads)
- Target transitions

### Continuous Mode
```bash
npm run start continuous
```
- Runs stages continuously
- Press Ctrl+C to stop

## Customization

```bash
# Custom shot count
npm run start -- --shots 15

# Custom countdown
npm run start -- --delay 5

# Custom shot interval
npm run start -- --interval 1000

# Different device
npm run start -- --device "My Timer" --model sg-sport

# All together
npm run start competitive -- --shots 20 --delay 5
```

## Device Models

- `simulated` (default)
- `sg-sport` - SG Timer Sport
- `sg-go` - SG Timer GO
- `special-pie` - Special Pie M1A2+

## Monitor MQTT Messages

```bash
# Subscribe to all timer topics
mosquitto_sub -h localhost -t 'timer/#' -v

# With pretty JSON (requires jq)
mosquitto_sub -h localhost -t 'timer/#' | jq
```

## Testing Workflow

```powershell
# Terminal 1: MQTT Broker
mosquitto -c mosquitto.conf -v

# Terminal 2: PWA Display
cd pwa-display-app
npm run dev

# Terminal 3: Simulator
cd mqtt-simulator
npm run start competitive
```

## Troubleshooting

### MQTT Connection Failed
1. Check Mosquitto is running
2. Verify `mosquitto.conf` has WebSocket listener:
   ```
   listener 9001
   protocol websockets
   allow_anonymous true
   ```
3. Restart Mosquitto: `net start mosquitto` (Windows) or `sudo systemctl restart mosquitto` (Linux)

### No Messages in PWA
1. Check browser console (F12)
2. Verify MQTT settings in PWA (press `S`)
3. Ensure broker URL is `ws://localhost:9001`

### Permission Errors
- Run `npm install` to ensure dependencies are installed
- Check Node.js version: `node --version` (should be 18+)
- Try removing node_modules and reinstalling: `rm -rf node_modules && npm install`

## Example Output

```
╔════════════════════════════════════════════╗
║   SG Timer MQTT Simulator                  ║
║   Shot Timer Event Publisher               ║
╚════════════════════════════════════════════╝

Configuration:
  Broker:   ws://localhost:9001
  Device:   Simulated SG Timer
  Model:    Simulated Timer
  Mode:     competitive

🔌 Connecting to MQTT broker: ws://localhost:9001
✅ Connected to MQTT broker

📱 Simulating device connection...

📡 Connection state: SCANNING
📡 Connection state: CONNECTING
📡 Connection state: CONNECTED (Simulated SG Timer)
ℹ️  Device: Simulated SG Timer (Simulated Timer)

🏆 Simulating competitive USPSA/IPSC stage...

🎬 Session started: ID=1, Delay=3s
⏳ Countdown: 3 seconds...
⏱️  Countdown complete for session 1

🎯 Competitor firing 13 rounds...

🎯 Shot #1: 1.432s (split: 0.000s)
🎯 Shot #2: 1.668s (split: 0.236s)
🎯 Shot #3: 1.941s (split: 0.273s)
...
```

## Files

- `src/index.ts` - CLI entry point
- `src/simulator.ts` - Simulation logic
- `src/types.ts` - TypeScript types
- `src/test-connection.ts` - Connection test
- `package.json` - Dependencies
- `tsconfig.json` - TypeScript config

# SG Timer MQTT Simulator

TypeScript CLI tool (using Node.js) to simulate shot timer devices publishing MQTT events. Perfect for testing the PWA display without physical hardware.

## Features

- 🎯 **Realistic shot timing** - Simulates competitive shooting scenarios
- 📡 **MQTT publishing** - Publishes to same topics as ESP32 bridge
- 🎬 **Multiple modes** - Simple, competitive, or continuous simulation
- ⚙️ **Configurable** - Customize shots, timing, device info
- 🎨 **Color output** - Beautiful terminal output with emojis

## Prerequisites

- [Node.js](https://nodejs.org) 18+ installed
- MQTT broker running (Mosquitto with WebSocket support on port 9001)

## Installation

```bash
cd mqtt-simulator
npm install
```

## Quick Start

### Test Display States Instantly

Jump directly to any display state without going through the full connection sequence:

```bash
# Jump to shot display (shows shot #5 by default)
npm run start state -- --state shot

# Jump to session ended
npm run start state -- --state ended

# Jump to waiting for shots
npm run start state -- --state waiting

# Custom shot number
npm run start state -- --state shot --shot-num 10
```

### 1. Test MQTT Connection

```bash
npm run test-connection
```

This verifies your MQTT broker is running and accepting WebSocket connections.

### 2. Run Simple Simulation

```bash
npm run start
```

Simulates:
- Device connection sequence (scanning → connecting → connected)
- Session start with 3-second countdown
- 10 shots at 1.5-second intervals
- Session end

### 3. Run Competitive Simulation

```bash
npm run start competitive
```

Simulates a realistic USPSA/IPSC stage:
- 12-15 rounds
- Realistic draw time (1.2-1.6s)
- Variable split times (0.2-0.35s)
- Target transitions (0.5-0.8s)
- Magazine reload (1.8-2.2s)

## Usage

```bash
npm run start [options] [mode]
```

### Modes

- **simple** (default) - Basic session with configured number of shots
- **competitive** - Realistic competitive shooting stage
- **continuous** - Run sessions continuously until stopped (Ctrl+C)
- **state** - Jump directly to a specific display state (skip connection sequence)

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `--broker <url>` | MQTT broker URL | `ws://localhost:9001` |
| `--shots <number>` | Number of shots per session | `10` |
| `--delay <seconds>` | Start delay countdown | `3.0` |
| `--interval <ms>` | Time between shots (ms) | `1500` |
| `--device <name>` | Device name | `"Simulated SG Timer"` |
| `--model <model>` | Device model (see below) | `simulated` |
| `--state <state>` | Display state to jump to (state mode only) | - |
| `--shot-num <number>` | Shot number for 'shot' state | `5` |
| `--total-shots <number>` | Total shots for 'ended' state | `12` |

### Device Models

- `simulated` - Generic simulated timer
- `sg-sport` - SG Timer Sport
- `sg-go` - SG Timer GO
- `special-pie` - Special Pie M1A2+

### Display States (for `state` mode)

- `connected` - Device connected, idle
- `waiting` - Session started, waiting for first shot
- `shot` - Actively showing shot data
- `ended` - Session complete with statistics

## Examples

### Basic Simulation
```bash
npm run start
```

### Competitive Stage
```bash
npm run start competitive
```

### Custom Configuration
```bash
# 15 shots with 5-second delay and 1-second intervals
npm run start -- --shots 15 --delay 5 --interval 1000
```

### Different Device
```bash
npm run start -- --model sg-sport --device "SG Timer Sport #123"
```

### Continuous Mode (Testing)
```bash
# Runs stages continuously until stopped
npm run start continuous
```

### Jump to Specific Display State
```bash
# Jump directly to showing shot #8 (skips connection sequence)
npm run start state -- --state shot --shot-num 8

# Jump to waiting for shots state
npm run start state -- --state waiting

# Jump to session ended with 15 shots
npm run start state -- --state ended --total-shots 15

# Jump to connected state only
npm run start state -- --state connected
```

### Custom Broker
```bash
# Connect to remote broker
npm run start -- --broker wss://mqtt.example.com:8884
```

## MQTT Topics Published

The simulator publishes to the same topics as the ESP32 bridge:

| Topic | When | Message |
|-------|------|---------|
| `timer/connection/state` | Connection changes | Connection state, device name |
| `timer/device/info` | After connection | Device model and name |
| `timer/session/started` | Session begins | Session ID, start delay |
| `timer/countdown/complete` | Countdown ends | Session ID |
| `timer/shot/detected` | Each shot | Shot number, times, device |
| `timer/session/stopped` | Session ends | Session ID, total shots |

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

🎯 Competitor firing 14 rounds...

🎯 Shot #1: 1.387s (split: 0.000s)
🎯 Shot #2: 1.612s (split: 0.225s)
🎯 Shot #3: 1.917s (split: 0.305s)
🎯 Shot #4: 2.245s (split: 0.328s)
   🔄 Magazine reload...
🎯 Shot #5: 4.156s (split: 1.911s)
...
🎯 Shot #14: 9.834s (split: 0.267s)

🏁 Session stopped: ID=1, Total shots=14

✅ Stage complete: 14 rounds in 9.83s

👋 Disconnecting from broker...
📡 Connection state: DISCONNECTED

✅ Simulation complete!
```

## Monitoring Messages

You can monitor published messages using mosquitto_sub:

```bash
# Subscribe to all timer topics
mosquitto_sub -h localhost -t 'timer/#' -v

# Pretty print JSON
mosquitto_sub -h localhost -t 'timer/#' -v | jq
```

## Troubleshooting

### Connection Fails

**Error:** `Connection failed: connect ECONNREFUSED`

**Solution:**
1. Check Mosquitto is running: `mosquitto -v`
2. Verify WebSocket listener in `mosquitto.conf`:
   ```
   listener 9001
   protocol websockets
   allow_anonymous true
   ```
3. Restart Mosquitto after config changes

### No Messages Received

1. Check broker logs: `mosquitto -v -c mosquitto.conf`
2. Verify topics match: `timer/#` pattern
3. Test with mosquitto_sub first

### Permission Denied

Ensure Node.js and npm are installed and up to date:
```bash
## Development

### Project Structure

```
mqtt-simulator/
├── src/
│   ├── index.ts           # CLI entry point
│   ├── simulator.ts       # Core simulation logic
│   ├── types.ts           # TypeScript types
│   └── test-connection.ts # Connection test utility
├── package.json
├── tsconfig.json
└── README.md
```
```
mqtt-simulator/
├── src/
│   ├── index.ts           # CLI entry point
│   ├── simulator.ts       # Core simulation logic
│   ├── types.ts           # TypeScript types
│   └── test-connection.ts # Connection test utility
├── package.json
└── README.md
```

### Adding New Scenarios

Edit `src/simulator.ts` and add new methods:

```typescript
async simulateCustomScenario(): Promise<void> {
  // Your custom simulation logic
## Integration with PWA Display

1. Start MQTT broker (Mosquitto)
2. Start PWA display (`cd pwa-display-app && npm run dev`)
3. Configure PWA to connect to `ws://localhost:9001`
4. Run simulator: `npm run start competitive`
5. Watch the display update in real-time!

## Testing Workflow

```bash
# Terminal 1: Start broker
mosquitto -c mosquitto.conf -v

# Terminal 2: Monitor messages
mosquitto_sub -h localhost -t 'timer/#' -v

# Terminal 3: Start PWA
cd pwa-display-app
npm run dev

# Terminal 4: Run simulator
cd mqtt-simulator
npm run start competitive
```
# Terminal 2: Monitor messages
mosquitto_sub -h localhost -t 'timer/#' -v

# Terminal 3: Start PWA
cd pwa-display-app
npm run dev

# Terminal 4: Run simulator
cd mqtt-simulator
bun run start competitive
```

## Tips

- Use **competitive mode** for realistic timing
- Use **continuous mode** for long-term testing
- Adjust `--interval` to test rapid fire scenarios
- Monitor MQTT messages to debug issues
- Check PWA browser console for errors

## License

Same as parent SG Timer LED Display project.

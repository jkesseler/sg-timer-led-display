# BLE bridge

A software version of the ESP32 bridge. It connects to a BLE shot timer and publishes the timer events to MQTT. It uses the same `timer/<deviceId>/<event>` topics and payloads as the ESP32 firmware, so the timekeeper, `/display` and the LED display accept it without changes.

Supported timers, in scan priority order (the first match wins): Special Pie M1A2 (by name `SP M1A2 Timer xxxx`), SG Timer, Special Pie M1A2+ and ASN Tracker (by service UUID).

## Setup

The bridge runs on the host, not in Docker. It needs the Bluetooth adapter.

1. Install the dependencies: `npm install` (in this folder). No compiler is needed: [`@stoprocent/noble`](https://github.com/stoprocent/noble) ships prebuilt binaries for linux-x64 and linux-arm64, so `allowScripts` in `package.json` skips its native rebuild.
2. Give Node raw HCI socket access, so that you do not have to run it as root:

   ```bash
   sudo setcap cap_net_raw+eip $(readlink -f $(which node))
   ```

   Do this again after each Node upgrade, because the capability is set on the binary.
3. Copy `.env.example` to `.env` and set `BRIDGE_DEVICE_ID`.
4. In Payload admin, add a `devices` entry with the same ID. Select that device on the match, as you do for an ESP32.

## Run

```bash
npm run bridge        # from score-keeping-app/
npm start             # from ble-bridge/
```

The bridge writes one line to stdout for each connect, disconnect, session event and shot. It writes errors to stderr. When it loses the timer, it waits 5 s and scans again.

## Configuration

| Variable | Default | Description |
|---|---|---|
| `MQTT_BROKER_URL` | `mqtt://localhost:1883` | Broker URL (TCP). |
| `MQTT_USERNAME` / `MQTT_PASSWORD` | not set | Optional broker credentials. |
| `BRIDGE_DEVICE_ID` | required | The ID for the topics `timer/<id>/...`. |
| `TIMER_NAME_FILTER` | not set | Connect only to a timer whose name contains this text. |
| `TIMER_ADDRESS` | not set | Connect only to the timer with this BLE address. |

## Test

```bash
npm test              # protocol parsers, device matching, MQTT payloads
npm run typecheck
```

The tests do not load noble. The BLE scan, connect and reconnect code is tested only on hardware.

## If noble conflicts with bluetoothd

noble uses raw HCI sockets. On a desktop where `bluetoothd` also uses the adapter, noble and `bluetoothd` can conflict: scans find nothing, or connections drop. If this occurs, try noble's D-Bus binding, which talks to BlueZ instead of the raw socket: `withBindings('dbus')` from `@stoprocent/noble`. It needs `dbus-next` installed (last released in 2022) and is not in noble's type definitions. The parsers in `src/protocols/` and the publisher stay the same.

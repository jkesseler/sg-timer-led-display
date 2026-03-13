# Device Comparison (Current Firmware)

This document reflects the timer device implementations currently compiled into `main-firmware`.

## Implementations in code

- `SGTimer`
- `SpecialPieM1A2F`
- `SpecialPieM1A2Plus`
- `ASNTracker`

All devices inherit from `BaseTimerDevice` and implement the `ITimerDevice` interface.

## Discovery and connection order

`TimerApplication::scanForDevices()` checks each advertised device in this priority order:

1. `SpecialPieM1A2F::matchesDevice()`
2. `SGTimer::matchesDevice()`
3. `SpecialPieM1A2Plus::matchesDevice()`
4. `ASNTracker::matchesDevice()`

First successful connection wins; only one timer is connected at a time.

## Quick comparison matrix

| Device | Discovery Method | Service UUID | Event Characteristic | Protocol Style | Time Source |
|---|---|---|---|---|---|
| SG Timer | Advertised service UUID | `7520FFFF-14D2-4CDA-8B6B-697C554C9311` | `75200001-14D2-4CDA-8B6B-697C554C9311` | Length-prefixed event packet | Native milliseconds |
| Special Pie (MAC/name variant) | Name starts with `SP M1A2 Timer ` | `0000FFF0-0000-1000-8000-00805F9B34FB` | `0000FFF1-0000-1000-8000-00805F9B34FB` | Framed packets (`F8 F9 ... F9 F8`) | Seconds + centiseconds |
| Special Pie (UUID variant) | Advertised service UUID | `0000FFF0-0000-1000-8000-00805F9B34FB` | `0000FFF1-0000-1000-8000-00805F9B34FB` | Framed packets (`F8 F9 ... F9 F8`) | Seconds + centiseconds |
| ASN Tracker | Advertised service UUID | `E5A10001-F1A2-4B63-9F8C-D7B781E35E2A` | `E5A10002-F1A2-4B63-9F8C-D7B781E35E2A` | Framed packets (`F8 F9 ... F9 F8`) | Seconds + centiseconds |

## Event support by implementation

| Event / Capability | SG Timer | SpecialPieMac | SpecialPie(UUID) | ASN Tracker |
|---|---:|---:|---:|---:|
| Session started callback | ✅ | ✅ | ✅ | ✅ |
| Countdown complete callback | ✅ (when protocol event present) | ❌ | ✅ (triggered immediately after start) | ✅ (triggered immediately after start) |
| Session stopped callback | ✅ | ✅ | ✅ | ✅ |
| Session suspended callback | ✅ | ❌ | ❌ | ❌ |
| Session resumed callback | ✅ | ❌ | ❌ | ❌ |
| Shot detected callback | ✅ | ✅ | ✅ | ✅ |
| Remote start/stop methods | Interface exists; default `false` unless overridden | default `false` | default `false` | default `false` |
| Shot list support | Interface method exists; implementation-specific | default `false` | default `false` | default `false` |

## Time normalization rules (actual code behavior)

- Output type for all devices: `NormalizedShotData`
- `absoluteTimeMs` and `splitTimeMs` are always milliseconds
- SG Timer packets already carry millisecond-scale values
- Special Pie + ASN Tracker convert centiseconds to milliseconds (`cs * 10`)
- Special Pie + ASN Tracker split time is derived from current and previous shot timestamps

## Device-specific notes

### SG Timer (`SGTimer`)

- Detects by SG service UUID
- Parses SG BLE event IDs (`0x00`..`0x05`)
- Device model is refined from advertised name pattern (`SG-SST4...`)

### Special Pie name/MAC variant (`SpecialPieM1A2F`)

- Prioritized first during scanning
- Matches device names with prefix `SP M1A2 Timer `
- Can read optional firmware value from device info service and include it in display name

### Special Pie UUID variant (`SpecialPieM1A2Plus`)

- Fallback when name-based match does not trigger
- Uses same `FFF0/FFF1` event path

### ASN Tracker (`ASNTracker`)

- Uses ASN-specific UUID pair
- Protocol payload shape closely matches Special Pie framed events
- Shot numbers are treated as zero-indexed in implementation (`totalShots = shotNumber + 1`)

## MQTT republish path interaction

When MQTT republishing is enabled and broker connection is active, shot events from any connected device are queued in a ring buffer in `TimerApplication` and published asynchronously by `MqttManager`.

This queueing layer is independent of device type.

## Relevant source files

- `ESP32-S3-firmware/src/TimerApplication.cpp`
- `ESP32-S3-firmware/src/SGTimer.cpp`
- `ESP32-S3-firmware/src/SpecialPieM1A2F.cpp`
- `ESP32-S3-firmware/src/SpecialPieM1A2Plus.cpp`
- `ESP32-S3-firmware/src/ASNTracker.cpp`
- `ESP32-S3-firmware/include/ITimerDevice.h`
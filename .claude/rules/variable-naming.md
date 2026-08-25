---
paths:
  - "pwa-display-app/**/*.{ts,tsx}"
  - "mqtt-simulator/**/*.ts"
  - "score-keeping-app/**/*.{ts,tsx}"
  - "ESP32-S3-firmware/**/*.{cpp,h}"
  - "BLE-LoRa-Bridge/**/*.{cpp,h}"
---

# Naming Rules

These apply across the whole monorepo — the C++ firmware and the TypeScript
packages alike. Where a rule differs by language it says so.

For TypeScript/React-specific conventions built on top of this file, see
[`coding-conventions.md`](./coding-conventions.md). For firmware architecture and
conventions, see `CLAUDE.md`.

## Basics

- Use English
- No contractions: `onItemClick` not `onItmClk`; `shotNumber` not `shotNum`
- Avoid duplicating context: `MenuItem.handleClick()` not `handleMenuItemClick()`;
  inside `SGTimer`, prefer `parsePacket()` over `parseSGTimerPacket()`
- Match the name to the result: `isDisabled` not `isEnabled` when checking
  `disabled={isDisabled}`

### Case by language

| | TypeScript | C++ firmware |
|---|---|---|
| Variables, parameters, methods | `camelCase` | `camelCase` |
| Types, classes, components | `PascalCase` | `PascalCase` |
| Fixed literal constants | `SCREAMING_SNAKE_CASE` | `UPPER_SNAKE_CASE` |
| Private members | `camelCase` | `camelCase`, **no underscore prefix** |
| Files | `camelCase.ts`, `PascalCase.tsx` | `PascalCase.h` / `PascalCase.cpp` |

In C++, `#define` constants and compile-time configuration in `common.h` are
`UPPER_SNAKE_CASE` (`PANEL_WIDTH`, `BLE_RECONNECT_INTERVAL`). Static class
constants follow the same form (`SERVICE_UUID`, `LOG_TAG`).

## Variables

Prefix with boolean indicator:
- `is` + adjective: `isBlue`, `isPresent`, `isFirstShot`
- `has` + noun: `hasProducts`, `hasPreviousShot`
- `should` + verb: `shouldUpdate`, `shouldRedraw`

Boundaries/state:
- `min`, `max`, `prev`, `next`

Singular for single value, plural for collections:
- `friend = 'Bob'`
- `friends = ['Bob', 'Tony']`

### Units belong in the name

This project moves time values across four components, in two different units.
**Name the unit whenever a value carries one.**

- Milliseconds: suffix `Ms` — `absoluteTimeMs`, `splitTimeMs`, `timestampMs`
- Centiseconds: suffix `Centiseconds` — `previousTimeCentiseconds`
- Seconds: suffix `Seconds` — `previousTimeSeconds`, `startDelaySeconds`

Everything in `NormalizedShotData` is milliseconds by project invariant; the `Ms`
suffix is what makes a missing `* 10` conversion visible at the call site. An
unsuffixed time name is a defect waiting to happen.

**Field names that cross the MQTT contract are fixed by the firmware.** Do not
rename them on the TypeScript side to suit local style — the firmware is the
source of truth and a rename silently breaks the contract.

## Functions

Pattern: `<prefix?><action><context>`

### Actions

- `get`: access data (sync or async)
- `set`: assign value
- `reset`: restore to initial state
- `compose`: create new data from existing
- `build`: create from existing (used alongside `compose` in this codebase)
- `find`: search, may return nothing
- `handle`: respond to action/event
- `process`: parse or transform incoming data — the firmware convention for
  driver entry points (`processTimerData`, `processScanResults`)
- `remove`: delete from collection
- `delete`: erase completely

### Prefixes

- `should`: conditional statement before action
- `is`, `has`: boolean properties
- `matches`: predicate testing an external thing against criteria
  (`matchesDevice`)

### Firmware-specific conventions

- `show*` on `DisplayManager` sets state and marks the display dirty; it does not
  draw immediately. Keep that meaning — a `show*` that renders synchronously
  would break the dirty-flag pattern.
- `on*` names a registered callback (`onShotDetected`, `onSessionStarted`).
- `attempt*` signals an operation expected to fail routinely and returning
  `bool` rather than throwing (`attemptConnection`).
- A function named `get*` must not mutate. This holds in both languages.

## Short, Intuitive, Descriptive

- Choose readable names over brevity
- Use real words, avoid made-up verbs
- Reflect what it does/means
- Prefer a real name over a single letter: `error` beats `e`. Loop indices are fine.

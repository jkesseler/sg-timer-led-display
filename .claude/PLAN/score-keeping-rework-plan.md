# score-keeping-app rework: Electron desktop app, Redux-owned state, IndexedDB

## Context

`score-keeping-app` (Next 16 + Payload 3.89 + Postgres) was built on assumptions that turned out wrong:

- Discipline lives on the squad membership. It belongs to the **squad**.
- A server with a relational DB is the source of truth, and a server-side MQTT subscriber is the sole writer of results. The intent is the opposite: the **front-end Redux Toolkit state is authoritative**, and storage is only a backup of it.
- There is no score calculation at all (only times per round), and no `--:--`.
- Many safeguards make edge-case corrections impossible (board freezes while a session is pending, one-RS DB hook, no undo for DNF/DQ/absent/sign-off, unique indexes, `assertNotSessionActive` in 7 actions). **Range Office is always right: nothing may hard-block a correction.**

Decisions (confirmed with user):
- **Electron desktop app on one single laptop.** Next.js, Payload, Postgres/Mongo, Docker and login are dropped; the UI is client-side React built with Vite.
- **Storage is IndexedDB**, the storage engine built into Electron's Chromium (stored under the app's `userData` directory), via Dexie. No native database modules.
- **Redux state is leading.** IndexedDB holds the persisted copy; there is no server, no sync conflicts, no revision checks.
- **One active match, one laptop, one timer on the range.** Previous matches are not shown in the app (may be added later).
- Squads only group shooters. Devices are not tied to squads; the timer device belongs to the match.
- Setup data (shooters, devices, matches, squads) is edited in simple in-app setup screens that replace Payload's `/admin`, plus a CSV import for shooters.
- Keep DNF as a status; it is displayed and scored as `--:--` (uncountable).
- Discard existing Postgres data.
- **All identifiers are UUIDs** (`crypto.randomUUID()`).
- **Audit log is kept**, fully transparent: written automatically from the Redux actions, no UI, no reason prompts, no extra input from score keeper or Range Office. No history view for now.

## Domain rules to encode

- Squad: `startTime`, `endTime`, **`discipline`**; a match has 1..n squads; a shooter can be in 1..n squads (one *card* per shooter per squad).
- 5 rounds per card, shot round-robin (ABCDEF, ABCEDF...), never AAAAA. Existing `deriveCurrentRound`/queue logic already does this; the order stays a mutable queue.
- Round states: `pending`, `timed`, `rs`, `dnf` (`--:--`), `skipped` (absent/late, catch-up later).
- `rs`: shooter asked a reshoot (malfunction). Round shows "RS", shot again after the rest of the squad; the reshoot time **replaces** the round's time for scoring.
- DQ: on the card, invalidates the shooter's **entire match** score (all their cards in the match). Reversible.
- Final score = mean of the 3 fastest countable rounds (`3,2,2,1,1` -> `(2+1+1)/3 = 1.3333`). Countable = `timed`, or `rs` with a reshoot time. Fewer than 3 countable once all rounds are resolved -> `--:--`.
- Averages are **stored unrounded in ms and displayed rounded to 2 decimals**. This rule wins over the truncation in the current `formatRoundTimeMs`; round times keep their existing display.
- `--:--` covers DNF, and also "fewer than 3 countable rounds after all rounds resolved".
- One card per (shooter, squad); a shooter in two squads has two scores, and DQ voids all of them in that match. No match-level total across a shooter's squads.

## Architecture

```
Timer ──BLE──► renderer (Web Bluetooth) ─┐
ESP32 bridge ──MQTT──► aedes broker ─────┤
                                         ▼
                          renderer Redux (leading)
                           ├─► IndexedDB: match state + audit log
                           └─► aedes broker ──MQTT──► TV display (RPi browser), ESP32 LED display
```

- **Main process** (Node): window management, the Web Bluetooth device picker, an embedded MQTT broker (**aedes**: TCP `1883` for the ESP32, WebSocket `9001` for browsers), a static HTTP server that serves the display bundle to the TV (`http://<laptop>:8080/display`), backup files, and `powerSaveBlocker` so the laptop does not sleep mid-match.
- **Preload**: a narrow `contextBridge` API (backup export/import, broker status, OS username). `contextIsolation: true`, `nodeIntegration: false`, `sandbox: true`.
- **Renderer** (React + Redux): timekeeper board, setup screens, display view. A settings option points MQTT at an external broker instead of the embedded one.

## Timer input (BLE)

- **Web Bluetooth in the renderer.** Electron handles device selection in the main process (`select-bluetooth-device`), so the app picks the timer by name pattern / service UUID without Chrome's chooser, and reconnects on its own. `backgroundThrottling: false` keeps the connection alive when the window is not focused. Linux needs BlueZ.
- The **ESP32 bridge stays supported**: it publishes `timer/<deviceId>/<event>` to the embedded broker as today.
- Both sources produce the same `mqttSlice` actions (`sessionStarted`, `shotDetected`, `sessionStopped`), so the match listeners do not care where a time came from. Web Bluetooth events are also **republished to MQTT** under the same contract, so the TV display and the ESP32 LED display (MQTT mode) get the live feed.
- The protocol parsers for SG Timer and Special Pie are ported from the firmware device classes to TypeScript in `src/renderer/lib/timer/` (pure functions, bytes in, normalized event out, times in ms), with Vitest cases copied from `ESP32-S3-firmware/test/test_protocol_parsing/`.
- Fallback if Web Bluetooth proves unreliable on the laptop: `@abandonware/noble` in the main process, forwarding events to the renderer over IPC.

## Data model (IndexedDB via Dexie)

Setup stores (edited in the setup screens):
- `shooters`: `id`, `firstName`, `lastName`, `knsaNumber` (unique index).
- `devices`: `id`, `deviceId` (firmware ID, unique index), `label`.
- `matches`: `id`, `label`, `date`, `deviceId`, `active` (the app works on the active match; newest `date` if several).
- `squads`: `id`, `matchId`, `label`, `startTime`, `endTime`, `discipline` (enum from `src/lib/domain/disciplines.ts`).
- `squadMembers`: `id`, `squadId`, `shooterId`, `startingPosition`. No unique index (duplicates are harmless).

Runtime stores:
- `matchStates`: key `matchId`, `revision` (counter), `state` (the `MatchState` below), `updatedAt`.
- `matchAudit` (append-only): key `actionId` (the action's `meta.id`), `matchId` (index), `at`, `by`, `type` (Redux action type), `payload`. `by` = the operator name from settings, default the OS username from the preload; never asked for.

Redux `MatchState` (plain JSON):
```
{ matchId, deviceId, revision,
  squads: {id,label,start,end,discipline,status}[],
  cards: { id, squadId, shooterId, shooterName, knsaNumber, queuePosition, presence:'present'|'absent',
           rounds: [{n, status, timeMs|null, reshootTimeMs|null}] x5,
           dq: {reason, at}|null, signedOffAt|null }[],
  activeTurn: {cardId, round, phase:'armed'|'running'}|null,
  unassignedResults: {id, timeMs, at}[] }
```
`deviceId` is the firmware ID of the match's timer (the board calls `selectDevice` with it). No audit inside the snapshot.

Backups (IndexedDB lives on the same disk, and can be wiped by a profile reset):
- The main process writes a JSON dump of all stores to `userData/backups/` every 5 minutes during a match and on quit; keep the last 20.
- Manual **Export** / **Import** of the same JSON through a file dialog (e.g. to a USB stick).

## Implementation steps

0. ~~Commit everything~~ Done (`36b59c6`, `9105d96`, `5c1b2d2`).

1. **Scaffold**: replace the Next/Payload app in `score-keeping-app/` with an `electron-vite` project (`src/main`, `src/preload`, `src/renderer`), React 19, RTK, `react-redux`, `mqtt`, `dexie`, `aedes` + `websocket-stream`, `@dnd-kit/*`, Vitest, Playwright. Package with `electron-builder` (Linux AppImage first; Windows target later if needed). Delete `next.config.ts`, `payload.config.ts`, `payload-types.ts`, `src/app/`, `src/collections/`, `src/fields/`, `Dockerfile`, `docker-compose*.yml`, `mosquitto.conf`, `pg-data/`, `test.env`, `.env*`. Keep and move into `src/renderer/`: `src/store/*`, `src/lib/{display,domain,mqtt,scanner}`, `src/components/display/*`, `src/components/timekeeper/TimekeeperBoard.tsx`, the CSS.

2. **Pure domain logic** in `src/renderer/lib/match/` (no React, no Dexie): `types.ts`, `score.ts` (`countableTimeMs`, `finalScoreMs(card)`, `matchScoreForShooter` applying match-wide DQ), `derive.ts` (port `deriveCurrentRound`, `deriveUpcomingShooters`, `deriveOutstanding`, `isReadyForSignOff` from `matchState.ts`, `formatRoundTimeMs`; add `formatScore` (2 decimals, rounded), `--:--` formatting, `findCardByKnsa(state, knsa, squadId)`, `deriveRoster(state)` for the display). Vitest cases: the `3,2,2,1,1` example, RS replaced by reshoot, RS without reshoot, DNF, DQ across two squads, <3 countable, ties, score rounding, KNSA lookup for a shooter in two squads.

3. **Storage**: `src/renderer/db.ts` (Dexie schema above, versioned for later migrations); `loadActiveMatch()`, `startMatch(matchId)` (builds the initial `MatchState` from the setup stores), `saveMatchState()`, `appendAudit()`.

4. **Redux**: `makeStore()` in `src/renderer/store/store.ts`.
   - `matchSlice.ts` (RTK; plain arrays) with reducers: `hydrate`, `armTurn`, `cancelTurn`, `turnStarted`, `turnStopped({lastShotTimeMs})`, `setRoundTime`, `setRoundStatus`, `setReshootTime`, `flagRs`, `flagDnf`, `disqualify`/`reinstate`, `markAbsent`/`markPresent`, `reorderQueue`, `addCard` (late shooter), `signOff`/`unsign`, `setSquadStatus`, `addUnassignedResult`/`assignResult`/`discardUnassignedResult`. Every mutating action is created with a RTK `prepare` callback that attaches `meta: { id, at }` (id = UUID), so audit needs no per-call code. **No reducer validates rules against the user**; rule violations are computed by selectors as warnings (e.g. second RS, sign-off with pending rounds).
   - **Scan to arm**: the board's scan input (card scanner via `scanCapture.ts`, or a typed KNSA number) resolves the card with `findCardByKnsa` (the selected squad first) and dispatches `armTurn`. Unknown number -> inline message, nothing armed.
   - `matchListeners.ts` (`createListenerMiddleware`): on `mqttSlice` `sessionStarted`/`sessionStopped` dispatch `turnStarted`/`turnStopped`. No `lastShotTimeMs` -> turn discarded, round stays pending. A session with no armed turn becomes an `unassignedResults` entry with an "assign to..." action instead of being dropped.
   - `persistMiddleware.ts`: after every `matchSlice` action carrying `meta`, one Dexie transaction writes the new `matchStates` row and the `matchAudit` row. No debounce, no queue, no retries: a local write is fast, and one transaction keeps snapshot and log consistent. Exposes `saveStatus` (`saved | error`); an error shows a banner (e.g. disk full) but never blocks the board.
   - `rosterMiddleware.ts`: when the derived roster (current / next / on deck) changes, publish it **retained** to `timer/<deviceId>/roster`.
   - On start: `hydrate` from `matchStates` for the active match; none -> `startMatch`.

5. **Main process**: aedes broker (TCP + WebSocket), static server for the display bundle, `select-bluetooth-device` handler, `powerSaveBlocker`, backup timer + export/import dialogs, single-instance lock (`app.requestSingleInstanceLock()`), so two windows never write the same IndexedDB.

6. **Timer input**: `src/renderer/lib/timer/` parsers + tests; `bleMiddleware.ts` (Web Bluetooth connect/notify/reconnect, dispatches `mqttSlice` actions, republishes to MQTT); a "Connect timer" control on the board.

7. **Timekeeper UI** (`TimekeeperBoard.tsx`): read from Redux selectors instead of `SquadView` props + `router.refresh()` polling; replace server actions with dispatches. Remove `controlsDisabled`; nothing is frozen by a live/pending session. Add: squad tabs (grouping only), RS button (missing today), editable time per round (type a value, set status, clear), undo for DNF/DQ/absent/sign-off, "unsign", warnings shown inline, per-card **final score** column (`--:--` when not computable, `DQ` when disqualified), unassigned-results list, timer/broker connection status, save-status indicator. Replace `window.prompt`/`confirm` DNF/DQ flows with small inline dialogs.

8. **Setup screens** (replace Payload `/admin`): list + form for shooters (with CSV import: `firstName,lastName,knsaNumber`), devices, matches (with the `active` toggle), squads and their members (drag to order, reusing the existing `@dnd-kit` code). No validation beyond required fields.

9. **Display**: `DisplayApp` becomes its own Vite entry (`display.html`), served by the main process to the TV and also openable as a second window on the laptop (e.g. an HDMI screen). It gets the timer feed and the retained `roster` topic over MQTT WebSocket; the 3 s roster polling is removed.

10. **Cleanup**: delete `serverSubscriber.ts`, `instrumentation.ts`, `loadSquadView.ts`, old `matchState.ts`, `scripts/setup-*-fixtures.ts` (replace with a `seed` that fills IndexedDB for dev/tests), `tests/int/api.int.spec.ts`; rewrite README (run, build, package, backup/restore); update the root `CLAUDE.md` component table (score-keeping-app is an Electron app; `pwa-display-app/` status).

## Verification

1. `npm test` (Vitest): score/derive/parser unit tests, plus a reducer test that every "illegal" edit (second RS, edit after sign-off, un-DQ, change a timed value) is accepted and only produces warnings.
2. `npm run dev` starts the app; the broker listens on 1883/9001; `http://<laptop>:8080/display` loads on another machine.
3. Drive a squad end to end with `mqtt-simulator/` publishing `timer/<deviceId>/session/*` to the embedded broker: scan a KNSA number to arm, the result lands in Redux and in IndexedDB, the TV display shows the same current/next.
4. Real timer over Web Bluetooth: connect without a chooser, shoot a string, the result lands on the armed card, the TV display shows the live feed; switch the timer off and on -> the app reconnects on its own.
5. Playwright (`_electron.launch`): full squad of 5 rounds -> final score shown; RS + reshoot replaces time; DQ voids all the shooter's cards, then reinstate; edit a round after sign-off; stuck pending session does not freeze the board; result with nothing armed shows up as unassigned and can be assigned.
6. Audit: every action in the Playwright squad run has exactly one `matchAudit` row; the board shows no audit-related UI or prompts.
7. Resilience: kill the app mid-match -> restart restores the exact state; export, wipe `userData`, import -> state and audit restored; a second app launch focuses the existing window.
8. `electron-builder` AppImage installs and runs on a clean Linux machine with BLE working.

## Related firmware work (separate effort, not part of this rework)

Two firmware items surfaced while discussing this rework; tracked here so they aren't lost, but out of scope for the work above (`ESP32-S3-firmware/` stays untouched by it).

1. **MQTT-only firmware mode.** When `TIMER_TYPE != TIMER_TYPE_BLE`, the firmware acts as an MQTT client: Bluetooth is disabled and shot data is read from MQTT (`timer/<deviceId>/<event>`, e.g. published by the app's BLE connection) instead of from the timer. **Not implemented today.** What exists: `common.h` defines `TIMER_TYPE_MQTT`, and the WiFiManager portal has a `timer_type` field (`WiFiConfig::getTimerType()`, `WiFiConfig.cpp:295`) saved to NVS but never read; `TimerApplication.cpp` branches only on the compile-time `TIMER_TYPE` macro. Needs: read `getTimerType()` at startup; when it is not BLE, skip BLE init/scan/connect entirely, subscribe to the timer topics and feed the display from MQTT.
   - Tradeoff: in this mode the timer must be in BLE range of the laptop, and the app must be running for the display to update.

2. **Show SSID and IP on the physical display**, so the config portal address can be found without a serial monitor. Both go into `WiFiConfig::getStartupText()`; `DisplayManager` already renders that text (`DisplayManager.cpp:372`, `:470`) and marquee-scrolls it when it is too wide, so no new display state is needed.
   - `WiFiConfig` keeps a static composed buffer, e.g. `"<startup_text>  SSID: <ssid>  IP: <ip>"`, and `getStartupText()` returns it. Build it only when the WiFi state changes (the connect branch at `WiFiConfig.cpp:208-210`, which today logs only the IP), not per call: `WiFi.SSID()` / `localIP().toString()` allocate `String`s, and the getter runs in the render loop.
   - Not connected: show the plain startup text plus `"  WiFi: <AP_SSID>"` when the config portal is up, so the user knows which network to join.
   - `startupTextPixelWidth` is computed once in `showStartup()` (`DisplayManager.cpp:237`). Recompute it whenever the text changes (e.g. `WiFiConfig` sets a changed flag that `DisplayManager::update()` checks), else the marquee uses the old width.
   - Wi-Fi must be up at power-on. `TimerApplication::initialize()` already calls `WiFiConfig::initialize()` (non-blocking `autoConnect`) before the display and BLE start, so SSID/IP appear as soon as the connection is made. Verify this on hardware, and correct the stale "Wi-Fi is initialized lazily after the first BLE connection" line in `CLAUDE.md`.
   - Size the buffer for 40 (startup text) + 32 (SSID) + 15 (IP) + labels, ~110 bytes.

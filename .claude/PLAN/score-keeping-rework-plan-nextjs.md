# score-keeping-app rework (variant): Next.js + Payload, Node BLE bridge

Alternative to `score-keeping-rework-plan.md` (Electron). Same domain rules and Redux design; differs in runtime: the app stays a Next.js + Payload web app in the browser, and a Node process on the laptop connects to the timer over BLE.

## Context

`score-keeping-app` (Next 16 + Payload 3.89 + Postgres) was built on assumptions that turned out wrong:

- Discipline lives on the squad membership. It belongs to the **squad**.
- Postgres/relational is the source of truth, and a server-side MQTT subscriber is the sole writer of results. The intent is the opposite: the **front-end Redux Toolkit state is authoritative**, and the server is a **backup** of it (Mongo fits: one document per match).
- There is no score calculation at all (only times per round), and no `--:--`.
- Many safeguards make edge-case corrections impossible (board freezes while a session is pending, one-RS DB hook, no undo for DNF/DQ/absent/sign-off, unique indexes, `assertNotSessionActive` in 7 actions). **Range Office is always right: nothing may hard-block a correction.**

Decisions (confirmed with user):
- Keep Next.js + Payload; swap the adapter to `@payloadcms/db-mongodb`, transactions off. No Electron.
- **Everything runs on one single laptop, with one editor** (one timekeeper browser). One active match, one timer on the range. Previous matches are not shown in the app (may be added later).
- **The browser is leading.** Its Redux state is the truth; the server copy is a backup used only when the browser has nothing (cleared storage). No conflict detection, no revision compare-and-swap.
- **BLE: Timer -> Node BLE bridge (noble) -> MQTT -> browser.** The bridge only publishes to MQTT; it never writes match data.
- **The MQTT broker is an external process** (Mosquitto), not embedded in the app or the bridge. Its address is **configurable** for the server, the browser and the bridge (see "MQTT broker configuration"). In development it runs in `docker-compose.dev.yml`.
- Squads only group shooters. Devices are not tied to squads; the timer device belongs to the match.
- Sync = a Redux **middleware** that pushes the match state to the DB on change.
- Keep DNF as a status; it is displayed and scored as `--:--` (uncountable).
- Discard existing Postgres data. The server-side MQTT subscriber is removed.
- **All identifiers are UUIDs** (Payload documents and Redux entities alike).
- **Audit log is kept**, fully transparent: written automatically by the sync middleware from the Redux actions, using the logged-in user as `by`. No UI, no reason prompts, no extra input from score keeper or Range Office. No history view for now.
- Sync payload: snapshot per match (fast restore, read by `/display`) + append-only `match-audit`. Results flush immediately (no debounce) so a timed result is never lost in a debounce window.

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
Timer ──BLE──► ble-bridge (Node, noble) ─┐
ESP32 bridge ────────────────────────────┤──MQTT──► Mosquitto (external; Docker in dev)
                                         │              │
                                         │              ├──WS──► timekeeper browser: Redux (leading)
                                         │              │           └─► Next server actions ─► Mongo (snapshot + audit)
                                         │              ├──WS──► /display browser (TV)
                                         │              └──────► ESP32 LED display (MQTT mode)
```

The bridge is a **software ESP32**: it speaks exactly the firmware's MQTT contract, so the browser, `/display` and the simulator need no changes to accept it, and the ESP32 bridge stays a drop-in alternative.

## MQTT broker configuration

One set of environment variables, read at **runtime** (not `NEXT_PUBLIC_*`, which is baked in at build time, so one build works against any broker):

| Variable | Used by | Example (dev) |
|---|---|---|
| `MQTT_BROKER_URL` | BLE bridge (TCP) | `mqtt://localhost:1883` |
| `MQTT_WS_URL` | browser (WebSocket), handed over by the server | `ws://localhost:9001` |
| `MQTT_USERNAME` / `MQTT_PASSWORD` | both (optional) | `pewpew` / `timer` |

- **Browser**: the root layouts of `/timekeeper` and `/display` read `MQTT_WS_URL` and the credentials on the server (`src/lib/mqtt/config.ts`, `getMqttConfig()`) and pass them to `ReduxProvider`, which seeds `settingsSlice`. Precedence: a broker saved in the existing Settings panel (`localStorage`) > server config > fallback `ws://<window.location.hostname>:9001`. The fallback is what lets the TV's browser reach the laptop's broker without per-device setup. This replaces the hardcoded `DefaultMqttSettings.broker` (`ws://127.0.0.1:9001`) in `src/lib/display/constants.ts`.
- **Settings panel**: add a "Reset to server default" button that clears the saved override.
- **Bridge**: reads `MQTT_BROKER_URL` and the credentials from its own `.env`.
- **The Next server itself does not connect to MQTT** (the server subscriber is gone); it only hands the config to the browser.
- `.env.example` lists all four; README documents a production `mosquitto.conf` (listener `1883`, listener `9001` protocol `websockets`, password file).

## BLE bridge

Location: `score-keeping-app/ble-bridge/` (own `package.json`, TypeScript, run with `tsx`; `npm run bridge` from `score-keeping-app/`). A separate process, not inside Next: it keeps its BLE connection when the Next dev server restarts, and only it needs Bluetooth privileges.

- **Runs on the host, not in Docker.** `@abandonware/noble` uses raw HCI sockets on Linux: grant Node `cap_net_raw` (`sudo setcap cap_net_raw+eip $(readlink -f $(which node))`) instead of running as root. If noble fights the desktop's `bluetoothd` for the adapter, switch to `node-ble` (talks to BlueZ over D-Bus); the parsers and publisher stay the same.
- **Config** (env / `.env`): `MQTT_BROKER_URL` (default `mqtt://localhost:1883`), `MQTT_USERNAME`/`MQTT_PASSWORD` (optional), `BRIDGE_DEVICE_ID` (the ID it publishes under; register it in `devices` and select it on the match like an ESP32), optional timer name / address filter.
- **Discovery and connect**: scan for the supported timers in the firmware's priority order (SpecialPie M1A2F by name pattern -> SG Timer -> SpecialPie M1A2+ -> ASN tracker by service UUID); first match wins. Reconnect on disconnect with a rate-limited delay (as the firmware's `BLE_RECONNECT_INTERVAL_MS`).
- **Parsers**: port the device protocols from `ESP32-S3-firmware/src/<Device>.cpp` to pure TypeScript in `ble-bridge/src/protocols/` (bytes in, normalized events out, **times in ms**, centiseconds x10 for Special Pie / ASN, `SHOT_INDEX_BASE` per device). Vitest cases copied from `ESP32-S3-firmware/test/test_protocol_parsing/`.
- **Publishes** under `timer/<BRIDGE_DEVICE_ID>/...`, same payloads as the firmware and `mqtt-simulator/`:
  - retained: `presence` (`online`, with an MQTT last-will `offline`), `device/info`, `connection/state` (scanning / connecting / connected / disconnected);
  - events: `session/started`, `session/stopped` (with `lastShotTimeMs`), `session/suspended`, `session/resumed`, `shot/detected`, `countdown/complete`.
  Reuse the publish/payload code from `mqtt-simulator/src/simulator.ts` rather than rewriting it.
- **Logging**: one line per connect/disconnect/session/shot to stdout; nothing else.
- Tradeoff (accepted): the timer must be within BLE range of the laptop, and the timekeeper browser must be open to record results.

## Data model (Payload collections on Mongo)

Every collection uses a UUID `id` (custom `id` text field, generated with `crypto.randomUUID()`; check that Payload fills it on create via `defaultValue`, else a `beforeValidate` hook). Redux entities (cards, audit `actionId`) use `crypto.randomUUID()` too.

Setup data (edited in `/admin`, read when a match is started):
- `users`, `shooters`, `devices`: unchanged (keep `knsaNumber` / `deviceId` uniqueness; these are identities, not match rules). The BLE bridge is a `devices` entry like any ESP32.
- `matches`: `label`, `date`, `device`, **`active`** (checkbox; the app works on the active match, newest `date` if several are ticked).
- `squads`: `match`, `label`, `startTime`, `endTime`, **`discipline`** (enum from `src/lib/domain/disciplines.ts`). Drop `status` (runtime state moves to Redux).
- `squad-members` (was `squad-memberships`): `squad`, `shooter`, `startingPosition`. Drop discipline, queuePosition, status, DQ fields, reshootTimeMs, signedOffAt and the afterChange hook that seeds rounds. No unique index (duplicates are harmless).

Runtime backup:
- `match-states` (new): `match` (relationship, unique), `revision` (number, informational counter set by the browser), `state` (json). Payload `versions` off (audit collection is the history).
- `match-audit` (new, append-only, no admin UI needed): `actionId` (unique, client-generated, makes retries idempotent), `match`, `at`, `by` (user, set in a `beforeChange` hook from `req.user`; any client value is ignored), `type` (Redux action type), `payload` (json). Access: create by authenticated users only; no update/delete.

Removed: `round-results`, `match-sessions`, their hooks, and `src/lib/mqtt/serverSubscriber.ts` + `src/instrumentation.ts`.

Redux `MatchState` (plain JSON, shared with server via pure functions):
```
{ matchId, deviceId, revision,
  squads: {id,label,start,end,discipline,status}[],
  cards: { id, squadId, shooterId, shooterName, knsaNumber, queuePosition, presence:'present'|'absent',
           rounds: [{n, status, timeMs|null, reshootTimeMs|null}] x5,
           dq: {reason, at}|null, signedOffAt|null }[],
  activeTurn: {cardId, round, phase:'armed'|'running'}|null,
  unassignedResults: {id, timeMs, at}[] }
```
`deviceId` is the firmware/bridge ID of the match's device (the board calls `selectDevice` with it). `unassignedResults` lives in the synced state, so a reload keeps them. No audit inside the snapshot; it lives in `match-audit`.

## Implementation steps

0. ~~Commit everything~~ Done (`36b59c6`, `9105d96`, `5c1b2d2`).

1. **Infra**: `package.json` replace `@payloadcms/db-postgres` with `@payloadcms/db-mongodb`; `src/payload.config.ts` use `mongooseAdapter({ url: process.env.DATABASE_URI, transactionOptions: false })` (standalone Mongo, no replica set). `docker-compose.dev.yml`: replace the `postgres` service with `mongo:8` modelled on `gs-cms/gs-cms.code/docker-compose.dev.yml` (named volume `mongo-data`, `--storageEngine=wiredTiger`, `27017:27017`, healthcheck via `mongosh --eval "db.adminCommand('ping')"`, `scoring-app` `depends_on: mongo: condition: service_healthy`, `DATABASE_URI=mongodb://mongo:27017/score-keeping-app`). Keep the Debian `node:24-bookworm-slim` image (not alpine, see glibc memory). No traefik. **Mosquitto stays in the dev compose** as its own service (`eclipse-mosquitto:2`, ports `1883` for the bridge/ESP32 and `9001` WebSocket for browsers, `./mosquitto.conf` mounted read-only); the app finds it only through the configured URLs, so outside development any external broker works. Set `MQTT_WS_URL=ws://localhost:9001` on `scoring-app` (the browser runs on the host, so the URL is the host's view, not `mosquitto:9001`). The host-side bridge uses `mqtt://localhost:1883`. **Replace the absolute `/home/jorgen/...` bind-mount paths with relative ones (`.:/home/node/app`, `./mosquitto.conf`)**; use a named volume for `node_modules` (as in the gs-cms example) or run `npm install` in the command. Use `DATABASE_URI` everywhere (`DATABASE_URL` is gone): compose, `.env.example`, `test.env` (`mongodb://localhost:27017/score-keeping-app-test` for `test:int`), README. Drop `pg-data` from `.gitignore` and delete the directory. Run `generate:types`; IDs become `string` in all generated types.

2. **Pure domain logic** in `src/lib/match/` (no React, no Payload; runs in reducers and on the server): `types.ts`, `score.ts` (`countableTimeMs`, `finalScoreMs(card)`, `matchScoreForShooter` applying match-wide DQ), `derive.ts` (port `deriveCurrentRound`, `deriveUpcomingShooters`, `deriveOutstanding`, `isReadyForSignOff` from `src/lib/match/matchState.ts`, `formatRoundTimeMs`; add `formatScore` (2 decimals, rounded), `--:--` formatting, `findCardByKnsa(state, knsa, squadId)`). Vitest cases: the `3,2,2,1,1` example, RS replaced by reshoot, RS without reshoot, DNF, DQ across two squads, <3 countable, ties, score rounding, KNSA lookup for a shooter in two squads.

3. **Collections** as above (`src/collections/Matches.ts` + `active`, `Squads.ts`, `SquadMemberships.ts` -> `SquadMembers.ts`, new `MatchStates.ts`, `MatchAudit.ts`; UUID `id` on all; delete `RoundResults.ts`, `MatchSessions.ts`).

4. **Redux**: replace the module-level `store` in `src/store/store.ts` with a `makeStore()` factory; `ReduxProvider` creates one per mount in a `useRef` (the Next.js RTK pattern), so server rendering never shares a store between requests. The timekeeper provider adds the match slice + sync middleware; `/display` keeps its current slices only.
   - `src/store/matchSlice.ts` (RTK; plain arrays) with reducers: `hydrate`, `armTurn`, `cancelTurn`, `turnStarted`, `turnStopped({lastShotTimeMs})`, `setRoundTime`, `setRoundStatus`, `setReshootTime`, `flagRs`, `flagDnf`, `disqualify`/`reinstate`, `markAbsent`/`markPresent`, `reorderQueue`, `addCard` (late shooter), `signOff`/`unsign`, `setSquadStatus`, `addUnassignedResult`/`assignResult`/`discardUnassignedResult`. Every mutating action is created with a RTK `prepare` callback that attaches `meta: { id, at }` (id = UUID), so audit needs no per-call code. **No reducer validates rules against the user**; rule violations are computed by selectors as warnings (e.g. second RS, sign-off with pending rounds).
   - **Scan to arm**: the board's scan input (card scanner or typed KNSA number, as today via `scanCapture.ts`) resolves the card with `findCardByKnsa` (the selected squad first) and dispatches `armTurn`. Unknown number -> inline message, nothing armed.
   - `src/store/matchListeners.ts` (RTK `createListenerMiddleware`): on `mqttSlice` `sessionStarted`/`sessionStopped` dispatch `turnStarted`/`turnStopped`. No `lastShotTimeMs` -> turn discarded, round stays pending. A session with no armed turn becomes an `unassignedResults` entry with an "assign to..." action instead of being dropped.
   - `src/store/syncMiddleware.ts`: after every `matchSlice` action it (a) writes the state to `localStorage` synchronously, (b) pushes the snapshot to the server: result-bearing actions (`turnStopped`, `setRoundTime`, `setRoundStatus`, `setReshootTime`, `addUnassignedResult`, `assignResult`) immediately, others debounced ~500 ms, and (c) queues a `match-audit` entry for every action carrying `meta`. The queue retries with backoff, drains in order and dedupes by `actionId`; the server treats a duplicate `actionId` as success. The audit queue itself is mirrored to `localStorage`, so entries not yet sent survive a reload. Exposes `syncStatus` (`synced | dirty | error`). Last write wins; no conflict handling.
   - **Hydrate precedence (browser is leading)**: on load, use the `localStorage` copy for the active match if there is one and push it to the server; use the server's `match-states` doc only when there is no local copy; otherwise build a fresh state with `startMatch`.

5. **BLE bridge** (see "BLE bridge"): `ble-bridge/` package, protocol parsers + tests, scan/connect/reconnect, MQTT publisher with last-will; `npm run bridge` script; README section (setcap, env, registering the bridge as a device).

6. **Server glue**: server actions (auth via `payload.auth` in each):
   - `getActiveMatch()`: the active match with its device, squads, members, shooters.
   - `startMatch(matchId)`: builds the initial `MatchState` if no `match-states` doc exists.
   - `saveMatchState(state)`: upsert by `match` (find, then create or update). A server action, not Payload REST, so the upsert and auth stay in one place.
   - `appendAudit(entries[])`: creates `match-audit` docs; ignores duplicate `actionId`.
   Replace `(protected)/actions.ts` and `loadSquadView.ts` (delete). The timekeeper page loads the active match directly (no squad picker; squad selection is local UI state on the board). Remove all `Number(...)` ID parsing.

7. **Timekeeper UI** (`src/components/timekeeper/TimekeeperBoard.tsx`): read from Redux selectors instead of `SquadView` props + `router.refresh()` polling. Remove `controlsDisabled`; nothing is frozen by a live/pending session. Add: squad tabs (grouping only), RS button (missing today), editable time per round (type a value, set status, clear), undo for DNF/DQ/absent/sign-off, "unsign", warnings shown inline, per-card **final score** column (`--:--` when not computable, `DQ` when disqualified), unassigned-results list, timer status (from the bridge's retained `presence` / `connection/state`), sync-status indicator. Replace `window.prompt`/`confirm` DNF/DQ flows with small inline dialogs.

8. **/display**: `getRosterForDevice` in `src/app/display/actions.ts` reads the active match's `match-states` snapshot (poll stays ~3 s) and reuses the `derive.ts` selectors, so display and board agree. The timer feed stays browser-side MQTT as today.

9. **Cleanup**: delete `serverSubscriber.ts`, `instrumentation.ts`, old `matchState.ts`, obsolete fixtures; rewrite `scripts/setup-*-fixtures.ts` for the new model; update `tests/int/*`, README (Mongo, broker configuration, bridge), and the root `CLAUDE.md` component table.

## Files most affected

`score-keeping-app/`: `package.json`, `src/payload.config.ts`, `docker-compose.dev.yml`, `test.env`, `.env.example`, `src/collections/*`, `src/lib/match/*` (new pure logic), `src/store/*` (store factory, new slice/listeners/sync), `src/components/display/ReduxProvider.tsx`, `src/components/timekeeper/TimekeeperBoard.tsx`, `src/app/timekeeper/(protected)/*`, `src/app/display/actions.ts`, `ble-bridge/*` (new), `scripts/*`, `tests/*`. `pwa-display-app/`, firmware and `mqtt-simulator/` untouched (the bridge copies, not imports, the simulator's publish code).

## Verification

1. `npm run test:int` (against the Mongo from `test.env`): score/derive unit tests above, plus a reducer test that every "illegal" edit (second RS, edit after sign-off, un-DQ, change a timed value) is accepted and only produces warnings. `ble-bridge` Vitest: parser cases for every supported timer.
2. `docker compose -f docker-compose.dev.yml up`: Mongo + Mosquitto + app start clean; `generate:types` passes; all IDs in Mongo are UUIDs.
3. Broker config: the board and `/display` connect to the broker from `MQTT_WS_URL`; with it unset, `/display` opened from another machine connects to `ws://<laptop>:9001`; a broker saved in Settings wins, and "Reset to server default" restores the server value; pointing all variables at a different broker (e.g. the system Mosquitto) works without a rebuild.
4. Drive a squad end to end with `mqtt-simulator/` publishing `timer/<deviceId>/session/*`: scan a KNSA number to arm, result lands in Redux, appears in `match-states` in Mongo within ~1 s, `/display` shows the same current/next.
5. Real timer through the bridge: `npm run bridge` finds and connects to the timer, the board shows it online; shoot a string -> the result lands on the armed card and `/display` shows the live feed; switch the timer off -> `connection/state` goes to disconnected and the bridge reconnects when it is back; kill the bridge -> `presence` goes `offline` (last will).
6. Playwright: full squad of 5 rounds -> final score shown; RS + reshoot replaces time; DQ voids all the shooter's cards, then reinstate; edit a round after sign-off; stuck pending session does not freeze the board; result with nothing armed shows up as unassigned and can be assigned.
7. Audit: every action in the Playwright squad run has exactly one `match-audit` row (also after forced retries: no duplicates, none missing); the board shows no audit-related UI or prompts.
8. Resilience: stop the app/Mongo mid-match -> indicator shows unsynced, edits continue; restart -> pushes; hard-reload browser -> state restored from `localStorage`; clear `localStorage` and reload -> state restored from the server; restart the Next server -> the bridge keeps its BLE connection.

## Related firmware work (separate effort, not part of this rework)

Same as in `score-keeping-rework-plan.md`: MQTT-only firmware mode (`TIMER_TYPE != TIMER_TYPE_BLE`: BLE off, shot data from MQTT, e.g. published by this plan's BLE bridge) and SSID + IP in `WiFiConfig::getStartupText()`.

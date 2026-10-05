# score-keeping-app

Match score-keeping for shooting matches timed with the BLE shot timer
described in the repository root [`CLAUDE.md`](../CLAUDE.md). Built on
[PayloadCMS](https://payloadcms.com) 3 + Next.js (App Router) with MongoDB.
It absorbs [`pwa-display-app/`](../pwa-display-app) as the `/display` route.

The design is in
[`.claude/PLAN/score-keeping-rework-plan-nextjs.md`](../.claude/PLAN/score-keeping-rework-plan-nextjs.md).

## How it fits together

- **The timekeeper's browser is leading.** The match lives in Redux
  (`src/store/matchSlice.ts`). Every change is written to `localStorage` at
  once and pushed to the server as a backup (`src/store/syncMiddleware.ts`):
  results immediately, everything else after ~500 ms, with retries until the
  server accepts it.
- **Nothing blocks a correction.** Every edit is accepted; rule breaches
  (a second RS, signing with open rounds) only show as warnings.
- **Timer results arrive over MQTT** as `timer/<deviceId>/<event>`, from the
  ESP32 bridge or from the Node BLE bridge in [`ble-bridge/`](ble-bridge).
  The browser binds a session to whichever shooter is armed; a result with
  nobody armed is kept as "unassigned".
- **Audit log:** every change is also appended to `match-audit` with the
  logged-in user. It has no UI on the board.

## Local setup

### 1. Environment

```bash
cp .env.example .env   # then set PAYLOAD_SECRET
```

| Variable | Used by | Example |
|---|---|---|
| `DATABASE_URI` | Payload | `mongodb://127.0.0.1:27017/score-keeping-app` |
| `PAYLOAD_SECRET` | Payload | any long random string |
| `MQTT_WS_URL` | browser (WebSocket), handed over by the server at request time; leave unset unless the broker is on another host | `ws://broker.example:9001` |
| `MQTT_USERNAME` / `MQTT_PASSWORD` | browser (optional) | |
| `MQTT_BROKER_URL` | `ble-bridge` (TCP), in `ble-bridge/.env` | `mqtt://localhost:1883` |

When `MQTT_WS_URL` is unset, the browser uses `ws://<host it loaded the page
from>:9001`, so a TV opening `http://<laptop>:3000/display` finds the
laptop's broker without setup. A broker saved in the `/display` Settings
panel overrides both; "Reset to server default" clears it.

Open `/timekeeper` on the laptop itself (`http://localhost:3000`): the board
needs a secure context (`localhost` or HTTPS) for `crypto.randomUUID()`.

### 2. Run with Docker (development)

```bash
docker compose -f docker-compose.dev.yml up
```

Starts the app on `3000`, MongoDB on `27017` and Mosquitto on `1883` (MQTT)
and `9001` (WebSocket). `node_modules` lives in a Docker volume, so the
container installs its own dependencies.

### 3. Or run on the host

Start MongoDB and an MQTT broker yourself, then:

```bash
npm install
npm run dev
```

### 4. Seed a match

```bash
npm run seed
```

Creates an admin (`admin@timer.tsd` / `qazwsx123`), timer device `TKUI01` and
an active match with two squads. Open `http://localhost:3000/timekeeper`.

## MQTT broker outside development

The broker is an external process. A production `mosquitto.conf` for the
laptop:

```
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous false
password_file /etc/mosquitto/passwd
```

Create the password file with `mosquitto_passwd -c /etc/mosquitto/passwd <user>`
and set the same user in `MQTT_USERNAME` / `MQTT_PASSWORD` (app) and
`ble-bridge/.env`.

## Collections

- **`users`**: auth for `/admin` and `/timekeeper`, with an `admin` /
  `timekeeper` role.
- **`shooters`**: names, optional ASN and KNSA numbers. `knsaNumber` is the
  barcode scan key.
- **`devices`**: timer device IDs (ESP32 or `ble-bridge`) with a label.
- **`matches`**: label, date, timer device, and `active`. The app works on
  the active match (the newest date if several are ticked).
- **`squads`**: time block and discipline within a match.
- **`squad-members`**: shooter + starting position in a squad. One card per
  shooter per squad.
- **`match-states`**: the backup of the browser's match state, one per match.
- **`match-audit`**: append-only log of every match change. No update or
  delete.

All document IDs are UUIDs.

## Routes

- **`/timekeeper`**: runs the active match: scan or click to arm a shooter,
  edit any round, RS/DNF/DQ with undo, sign-off, late shooters, unassigned
  results, final scores.
- **`/display`**: public scoreboard. Live timer feed over MQTT; the
  `Next:`/`On deck:` names come from the saved match state (polled every 3 s).
- **`/admin`**: Payload admin for setup data.

## Printing score sheets

Signing off a card prints its score sheet (A6: match, squad, shooter, KNSA
number, discipline, the five rounds with the three counted ones marked, the
score, a DQ notice and signature lines). Signed cards have a **Reprint**
button. The page calls the browser's print, so any printer with a driver works;
the sheet is meant for a 4x6" / A6 direct-thermal label printer.

To print without a dialog in Firefox, use a separate profile for the
timekeeper (`about:profiles`, or `firefox -P timekeeper`), because the setting
applies to everything printed from that profile:

1. Make the label printer the default printer, print one sheet by hand with
   the label paper size and scale 100%, and under "More settings" untick
   "Print headers and footers" (otherwise Firefox prints the URL and page
   title on the sheet). Firefox keeps these settings per printer.
2. In `about:config`, set `print.always_print_silent` to `true` (and
   optionally `print.show_print_progress` to `false`).

In Chrome or Edge, start the browser with `--kiosk-printing` instead. The page
size is set in `src/app/timekeeper/timekeeper.css` (`@page`, 105 x 148 mm);
change it if the labels are another size, e.g. 101.6 x 152.4 mm for 4x6".

## Scripts

- `npm run dev`: Next.js dev server.
- `npm run seed`: fill the database with a test match.
- `npm run bridge`: start the BLE bridge (see [`ble-bridge/README.md`](ble-bridge/README.md)).
- `npm run generate:types`: regenerate `src/payload-types.ts`.
- `npm run test:int`: Vitest tests in `tests/int/`. The Payload tests need
  MongoDB at `DATABASE_URI` from `test.env`.
- `npm run test:e2e`: Playwright tests in `tests/e2e/`, against a running app
  with a freshly seeded database and a broker (`PLAYWRIGHT_BASE_URL`,
  `MQTT_BROKER_URL`).

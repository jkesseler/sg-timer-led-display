# score-keeping-app

Match score-keeping application for shooting matches timed with the BLE shot
timer described in the repository root [`CLAUDE.md`](../CLAUDE.md). Built on
[PayloadCMS](https://payloadcms.com) 3 + Next.js (App Router), with Postgres
as the database. It absorbs [`pwa-display-app/`](../pwa-display-app) as the
`/display` route rather than running it as a separate app.

See [`.claude/PLAN/score-keeping-plan-prompt.md`](../.claude/PLAN/score-keeping-plan-prompt.md)
for the full requirements this app implements.

## Local setup

### 1. Database

This app needs a local Postgres instance. Either:

- **Docker** — `docker-compose up -d` (starts Postgres on `5432`, matching
  the default `.env`).
- **A local Postgres install** — create a database and point `DATABASE_URL`
  in `.env` at it.

### 2. Install and run

```bash
npm install
npm run dev
```

Open `http://localhost:3000/admin` and follow the on-screen instructions to
create your first admin user.

## Collections

- **`users`** — Payload's auth-enabled collection, gates both `/admin` and
  `/timekeeper`. Has an `admin` / `timekeeper` `role` field.
- **`shooters`** — first/last name, optional ASN and KNSA membership numbers.
  `knsaNumber` is the barcode scan lookup key; it's optional because not
  every shooter has a scannable card.
- **`devices`** — registry of known timer `deviceId`s (the firmware's 6-char
  ID) with a friendly label, used to bind a timer device to a squad.
- **`squads`** — a time block with an ordered list of shooters (via
  `squad-memberships`) and the timer device bound to it.
- **`squad-memberships`** — one row per (squad, shooter, discipline). Owns
  the live queue position, presence status, and the one allowed reshoot.
  Auto-seeds 5 `round-results` rows on create.
- **`round-results`** — one row per (membership, round 1-5): `pending`,
  `timed`, `rs` (malfunction), or `skipped`.
- **`match-sessions`** — the MQTT session-binding ledger: which timer
  session became which round-result (or reshoot), written by the
  server-side MQTT subscriber (`src/lib/mqtt/serverSubscriber.ts`,
  started once from `src/instrumentation.ts`).

## Routes

- **`/display`** — the public, unauthenticated scoreboard (absorbs
  [`pwa-display-app/`](../pwa-display-app), which this route now supersedes).
  Subscribes to MQTT directly from the browser (`src/store/`, ported
  near-verbatim) and polls the live squad queue for the `Next:`/`On deck:`
  callouts (`src/app/display/actions.ts`).
- **`/timekeeper`** — auth-gated squad-running screen: scan-or-manual
  shooter activation, queue mutations, reshoot/catch-up handling.
- **`/admin`** — Payload's own admin UI, for shooter/device/squad CRUD.

## Scripts

- `npm run dev` — Next.js dev server.
- `npm run build` — production build.
- `npm run generate:types` — regenerate `src/payload-types.ts` from the
  current collection config.
- `npm run test:int` — Vitest integration tests (`tests/int/`).
- `npm run test:e2e` — Playwright end-to-end tests (`tests/e2e/`).
- `npx tsx scripts/verify-mqtt-binding.ts` — headless verification of the
  MQTT session-binding logic against a running dev server + real broker.

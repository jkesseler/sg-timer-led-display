# score-keeping-app rework: Mongo backup + Redux-owned state

## Context

`score-keeping-app` (Next 16 + Payload 3.89 + Postgres) was built on assumptions that turned out wrong:

- Discipline lives on the squad membership. It belongs to the **squad**.
- Postgres/relational is the source of truth, and a server-side MQTT subscriber is the sole writer of results. The intent is the opposite: the **front-end Redux Toolkit state is authoritative**, and the server is a **version backup** of it (Mongo fits: one document per match).
- There is no score calculation at all (only times per round), and no `--:--`.
- Many safeguards make edge-case corrections impossible (board freezes while a session is pending, one-RS DB hook, no undo for DNF/DQ/absent/sign-off, unique indexes, `assertNotSessionActive` in 7 actions). **Range Office is always right: nothing may hard-block a correction.**

Decisions (confirmed with user):
- Keep Payload, swap adapter to `@payloadcms/db-mongodb`.
- Sync = a Redux **middleware** that pushes state to the DB (debounced snapshot per match).
- Keep DNF as a status; it is displayed and scored as `--:--` (uncountable).
- Discard existing Postgres data. The **browser** owns MQTT -> results; the server-side MQTT subscriber is removed.
- Audit log is built in but **fully transparent**: written automatically by the sync middleware from the Redux actions, using the logged-in user as `by`. No UI, no reason prompts, no extra input from score keeper or Range Office. No history view for now.
- Sync payload: snapshot per match (fast restore, read by `/display`) + append-only `match-audit`. Results flush immediately (no debounce) so a timed result is never lost in a debounce window.

## Domain rules to encode

- Squad: `startTime`, `endTime`, **`discipline`**; a match has 1..n squads; a shooter can be in 1..n squads (one *card* per shooter per squad).
- 5 rounds per card, shot round-robin (ABCDEF, ABCEDF...), never AAAAA. Existing `deriveCurrentRound`/queue logic already does this; the order stays a mutable queue.
- Round states: `pending`, `timed`, `rs`, `dnf` (`--:--`), `skipped` (absent/late, catch-up later).
- `rs`: shooter asked a reshoot (malfunction). Round shows "RS", shot again after the rest of the squad; the reshoot time **replaces** the round's time for scoring.
- DQ: on the card, invalidates the shooter's **entire match** score (all their cards in the match). Reversible.
- Final score = mean of the 3 fastest countable rounds (`3,2,2,1,1` -> `(2+1+1)/3 = 1.3333`). Countable = `timed`, or `rs` with a reshoot time. Fewer than 3 countable once all rounds are resolved -> `--:--`.

## Data model (Payload collections on Mongo)

Setup data (edited in `/admin`, read when a match is started):
- `users`, `shooters`, `devices`: unchanged (keep `knsaNumber` / `deviceId` uniqueness; these are identities, not match rules).
- `matches`: `label`, `date`, `device`.
- `squads`: `match`, `label`, `startTime`, `endTime`, **`discipline`** (enum from `src/lib/domain/disciplines.ts`). Drop `status` (runtime state moves to Redux).
- `squad-members` (was `squad-memberships`): `squad`, `shooter`, `startingPosition`. Drop discipline, queuePosition, status, DQ fields, reshootTimeMs, signedOffAt and the afterChange hook that seeds rounds. No unique index (duplicates are harmless).

Runtime backup:
- `match-states` (new): `match` (relationship, unique), `revision` (number), `state` (json). Payload `versions` off (audit collection is the history).
- `match-audit` (new, append-only, no admin UI needed): `actionId` (unique, client-generated, makes retries idempotent), `match`, `at`, `by` (user), `type` (Redux action type), `payload` (json). Access: create by authenticated users only; no update/delete.

Removed: `round-results`, `match-sessions`, their hooks, and `src/lib/mqtt/serverSubscriber.ts` + `src/instrumentation.ts`.

Redux `MatchState` (plain JSON, shared with server via pure functions):
```
{ matchId, revision, squads: {id,label,start,end,discipline,status}[],
  cards: { id, squadId, shooterId, shooterName, queuePosition, presence:'present'|'absent',
           rounds: [{n, status, timeMs|null, reshootTimeMs|null}] x5,
           dq: {reason, at}|null, signedOffAt|null }[],
  activeTurn: {cardId, round, phase:'armed'|'running'}|null }
```
No audit inside the snapshot; it lives in `match-audit`.

## Implementation steps

0. **Commit everything** (first action after approval, per user, so work continues on another box): copy this plan to `.claude/PLAN/score-keeping-rework-plan.md`, then `git add -A` and commit the whole working tree on `feature/score-keeping` (includes the deleted `.claude/PLAN/*prompt.md` files, pwa `package.json`, score-keeping `.gitignore`/`package*.json`, `docker-compose.dev.yml`, `mosquitto.conf`, root `package-lock.json`). Check `git status` first and flag anything surprising (e.g. root `package-lock.json`). Message `docs(score-keeping): add rework plan` with a bullet body noting the bundled WIP; push if a remote branch exists so the other box can pull.

1. **Infra**: `package.json` replace `@payloadcms/db-postgres` with `@payloadcms/db-mongodb`; `src/payload.config.ts` use `mongooseAdapter({ url: process.env.DATABASE_URI })`; `docker-compose.dev.yml`: replace the `postgres` service with `mongo:8` modelled on `gs-cms/gs-cms.code/docker-compose.dev.yml` (named volume `mongo-data`, `--storageEngine=wiredTiger`, `27017:27017`, healthcheck via `mongosh --eval "db.adminCommand('ping')"`, `scoring-app` `depends_on: mongo: condition: service_healthy`, `DATABASE_URI=mongodb://mongo:27017/score-keeping-app`). Keep the Debian `node:24-bookworm-slim` image (not alpine, see glibc memory) and mosquitto. No traefik. **Replace the absolute `/home/jorgen/...` bind-mount paths with relative ones (`.:/home/node/app`, `./mosquitto.conf`)** so it works on the other dev box; use a named volume for `node_modules` (as in the gs-cms example) or run `npm install` in the command. Drop `pg-data` from `.gitignore`. Update `.env.example` (`DATABASE_URI`), README. Run `generate:types`.

2. **Pure domain logic** in `src/lib/match/` (no React, no Payload; runs in reducers and on the server): `types.ts`, `score.ts` (`countableTimeMs`, `finalScoreMs(card)`, `matchScoreForShooter` applying match-wide DQ), `derive.ts` (port `deriveCurrentRound`, `deriveUpcomingShooters`, `deriveOutstanding`, `isReadyForSignOff` from `src/lib/match/matchState.ts`, `formatRoundTimeMs`; add `formatScore`, `--:--` formatting). Vitest cases: the `3,2,2,1,1` example, RS replaced by reshoot, RS without reshoot, DNF, DQ across two squads, <3 countable, ties.

3. **Collections** as above (`src/collections/Squads.ts`, `SquadMemberships.ts` -> `SquadMembers.ts`, new `MatchStates.ts`; delete `RoundResults.ts`, `MatchSessions.ts`).

4. **Redux**: `src/store/matchSlice.ts` (RTK; `createEntityAdapter` or plain arrays) with reducers: `hydrate`, `armTurn`, `cancelTurn`, `turnStarted`, `turnStopped({lastShotTimeMs})`, `setRoundTime`, `setRoundStatus`, `setReshootTime`, `flagRs`, `flagDnf`, `disqualify`/`reinstate`, `markAbsent`/`markPresent`, `reorderQueue`, `addCard` (late shooter), `signOff`/`unsign`, `setSquadStatus`. Every mutating action is created with a RTK `prepare` callback that attaches `meta: { id, at }` (id = uuid), so audit needs no per-call code. **No reducer validates rules against the user**; rule violations are computed by selectors as warnings (e.g. second RS, sign-off with pending rounds).
   - `src/store/matchListeners.ts` (RTK `createListenerMiddleware`): on existing `mqttSlice` session started/stopped actions (`src/store/mqttMiddleware.ts`) dispatch `turnStarted`/`turnStopped`. No `lastShotTimeMs` -> turn discarded, round stays pending. A session with no armed turn is shown as "unassigned result" with an "assign to..." action instead of being dropped.
   - `src/store/syncMiddleware.ts` (the sync middleware): for every `matchSlice` action carrying `meta` it enqueues a `match-audit` POST (`by` resolved from the session server-side, so the client never supplies it); the queue retries with backoff and is drained in order, deduped by `actionId`. Result-bearing actions (`turnStopped`, `setRoundTime`, `setRoundStatus`, `setReshootTime`) flush snapshot + audit immediately; all other actions debounce ~500 ms. The snapshot is PUT `{revision, state}` to `match-states` via Payload REST; on success stores the new revision; retries with backoff when offline; exposes `syncStatus` (`synced | dirty | error`). On revision mismatch never discard silently: show a banner with "load server version" / "overwrite server". Also mirror state to `localStorage` so a browser refresh does not lose unsynced work.
   - Register in `src/store/store.ts`; `ReduxProvider` hydrates from the server-loaded state.

5. **Server glue**: server action `startMatch(matchId)` builds initial `MatchState` from matches/squads/squad-members/shooters if no `match-states` doc exists; timekeeper page loads the existing doc otherwise. Replace `(protected)/actions.ts` and `loadSquadView.ts` (delete). Re-check auth (`payload.auth`) in every remaining action and on the sync endpoint.

6. **Timekeeper UI** (`src/components/timekeeper/TimekeeperBoard.tsx`): read from Redux selectors instead of `SquadView` props + `router.refresh()` polling. Remove `controlsDisabled`; nothing is frozen by a live/pending session. Add: RS button (missing today), editable time per round (type a value, set status, clear), undo for DNF/DQ/absent/sign-off, "unsign", warnings shown inline, per-card **final score** column (`--:--` when not computable, `DQ` when disqualified), sync-status indicator. Replace `window.prompt`/`confirm` DNF/DQ flows with small inline dialogs.

7. **/display**: `getRosterForDevice` in `src/app/display/actions.ts` reads the `match-states` snapshot (poll stays ~3 s) and reuses the `derive.ts` selectors, so display and board agree. The timer feed stays browser-side MQTT as today.

8. **Cleanup**: delete `serverSubscriber.ts`, `instrumentation.ts`, old `matchState.ts`, obsolete fixtures; rewrite `scripts/setup-*-fixtures.ts` for the new model; update `tests/int/*`, README, and the auto-memory note that says "server subscriber is sole DB writer" (now wrong).

## Files most affected

`score-keeping-app/`: `package.json`, `src/payload.config.ts`, `docker-compose.dev.yml`, `src/collections/*`, `src/lib/match/*` (new pure logic), `src/store/*` (new slice/listeners/sync), `src/components/timekeeper/TimekeeperBoard.tsx`, `src/app/timekeeper/(protected)/*`, `src/app/display/actions.ts`, `scripts/*`, `tests/*`. `pwa-display-app/`, firmware and `mqtt-simulator/` untouched.

## Verification

1. `npm run test:int`: score/derive unit tests above, plus a reducer test that every "illegal" edit (second RS, edit after sign-off, un-DQ, change a timed value) is accepted and only produces warnings.
2. `docker compose -f docker-compose.dev.yml up`: Mongo + Mosquitto + app start clean; `generate:types` passes.
3. Drive a squad end to end with `mqtt-simulator/` publishing `timer/<deviceId>/session/*`: arm a shooter, result lands in Redux, appears in `match-states` in Mongo within ~1 s, `/display` shows the same current/next.
4. Playwright: full squad of 5 rounds -> final score shown; RS + reshoot replaces time; DQ voids all the shooter's cards, then reinstate; edit a round after sign-off; stuck pending session does not freeze the board.
5. Audit: every action in the Playwright squad run has exactly one `match-audit` row (also after forced retries: no duplicates, none missing); the board shows no audit-related UI or prompts.
6. Resilience: stop the app/Mongo mid-match -> indicator shows unsynced, edits continue; restart -> pushes; hard-reload browser -> state restored from server; open a second browser with an older revision -> conflict banner (no silent loss).

## Assumptions to confirm during review

- Averages are displayed to 2 decimals (rounded), stored unrounded in ms.
- `--:--` covers DNF, and also "fewer than 3 countable rounds after all rounds resolved".
- One card per (shooter, squad); a shooter in two squads has two scores, and DQ voids all of them in that match.
- Match-level total across a shooter's several squads is not computed (each card has its own score).

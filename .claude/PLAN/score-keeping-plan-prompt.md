# Plan-mode prompt: match score-keeping app (PayloadCMS + Next.js)

> Source notes: [.claude/PLAN/score-keeping-prompt.md](.claude/PLAN/score-keeping-prompt.md)
> Paste everything below into plan mode.

---

Produce an implementation plan for a match score-keeping web application built on **PayloadCMS + Next.js**, into which the existing PWA display app is absorbed as a route.

Do not write implementation code. I want a plan: data model, routes, state machine, integration points, and a staged build order. Ask me the open questions listed at the end before finalising — several of them change the shape of the whole design.

## Goal

Today a shooting match is timed with a BLE shot timer that bridges shot events to MQTT, and those events are shown on an LED matrix panel and in a browser PWA. Times are written down on paper, signed by the shooter, and typed into an external system afterwards.

I want an application that captures those times digitally against the right shooter, squad, and discipline, driven by a timekeeper sitting at a screen, with shooters identified by scanning a barcode card.

## Current state of this repo

This is a monorepo — see [CLAUDE.md](CLAUDE.md) for the full component map and conventions. Relevant parts:

- [pwa-display-app/](pwa-display-app/) — React 18 + Vite + Redux Toolkit PWA that subscribes to MQTT over WebSockets and renders a simulated 128×32 LED matrix. This becomes a route in the new Next.js app.
  - [src/types.ts](pwa-display-app/src/types.ts) — the shared event/message types
  - [src/constants.ts](pwa-display-app/src/constants.ts) — topic constants plus `buildDeviceTopic()` / `parseDeviceTopic()`
  - [src/hooks/useMqtt.ts](pwa-display-app/src/hooks/useMqtt.ts) — connection, device discovery, per-device message routing
  - Store lives in [src/store/](pwa-display-app/src/store/) with MQTT and beep middleware
- [ESP32-S3-firmware/](ESP32-S3-firmware/) — the firmware that publishes the events. **Out of scope for changes** unless the plan proves a firmware change is unavoidable; if so, call it out explicitly rather than assuming it.
- [mqtt-simulator/](mqtt-simulator/) — emulates firmware MQTT output. Use it as the development and test driver so no hardware is needed.

### The MQTT contract (already exists — consume it, do not redesign it)

Topics follow `timer/<deviceId>/<event>`, where `deviceId` is a 6-char id from the firmware. The PWA subscribes with the `+` wildcard to see all devices at once.

Retained: `presence`, `connection/state`, `device/info`.
Ephemeral: `session/started`, `session/stopped`, `session/suspended`, `session/resumed`, `shot/detected`, `countdown/complete`.

`shot/detected` carries `sessionId`, `shotNumber`, `absoluteTimeMs`, `splitTimeMs`, `deviceModel`, `isFirstShot`, `timestamp`. `session/stopped` carries `sessionId`, `totalShots`, `lastShotTimeMs`, `timestamp`. All times are milliseconds.

A round therefore maps onto one timer session: `session/started` → n × `shot/detected` → `session/stopped`. The plan needs to say how a session gets bound to (shooter, discipline, round) and what happens to a session that arrives with no active shooter selected.

## Actors

- **Shooter / competitor** — shoots the rounds. Carries a card with a barcode encoding their KNSA membership number.
- **Range officer** — holds the timer device, presses start and stop on it. Does not touch the application.
- **Timekeeper** — sits at the timekeeper screen, advances the match, corrects the active shooter when the scanner fails.
- **Match director** — receives the signed paper sheets at the end and enters results into an external system. **Out of scope.**

## Domain

### Disciplines

A fixed set, defined as an enum in code:

| Code | Meaning |
|---|---|
| OKP | Open groot kaliber pistool |
| OKKP | Open klein kaliber pistool |
| SKP | Standaard groot kaliber pistool |
| SKKP | Standaard klein kaliber pistool |
| PCC 9mm | PCC 9mm |
| PCC .22 | PCC .22 |
| OKR | Open groot kaliber revolver |
| OKKR | Open klein kaliber revolver |
| SKR | Standaard revolver |
| SKKR | Standaard klein kaliber revolver |

### Shooter

At minimum: first name, last name, ASN membership number, KNSA membership number. The KNSA number is what the barcode encodes, so it is the scan lookup key.

### Squad

A group of shooters on the range during one time block (for example 08:00–09:00). One squad can have several disciplines running at once. A squad has a start/end time and an ordered list of shooters.

**Important modelling constraint:** a shooter can appear in multiple squads, and competes in multiple disciplines. So the discipline and the position number (`#1`, `#2`, …) are properties of the **squad membership**, not of the shooter. Recorded times attach to that membership row (or to a round belonging to it) — not directly to the shooter. Design the collections accordingly.

### Real squad schedule shape

I extracted the reference schedule PDF (`~/Downloads/squad_schedule.pdf`, outside the repo, so plan mode cannot read it — everything below is what it contains). Columns are `# | Naam | Klasse | Lid ASN | KNSA nr.`, grouped under time-block headings:

```
8:00 - 9:00
1  Patrick Vlaar       SKP   -   -
2  Vincent Vlaar       OKP   -   -
...
7  Arjan Dekker        SKP   -   -
9:00 - 10:00
1  Jeffrey Evers       OKP   -   -
...
```

Facts worth designing against:

- **8 time blocks** across 2 pages: 08:00–09:00 through 11:00–12:00, then 13:00–14:00 through 16:00–17:00 — note the **gap over 12:00–13:00**, so blocks are not a contiguous run and squads should carry explicit start/end times rather than a slot index.
- **6 or 7 shooters per squad**, position-numbered `1..n` within the block. The footer totals **54 series** — i.e. one entry per shooter per squad, which is the membership row.
- Every row carries exactly one Klasse. Only **OKP and SKP** appear in this export, but the enum must still cover all ten disciplines.
- **8 shooters appear in exactly two squads each** (Patrick Vlaar, Sven Burgering, Cris Rutgers, Leon Keijzer, Kadir Demirci, Jan-Maarten van Osch, Harry Kostwinder, Cor Woordman) — and in each case the two entries carry **different disciplines**. This is direct evidence for the membership model above: the same person, two squads, two disciplines, two independent sets of times.
- One name contains a diacritic (Fred Mangé), so handle non-ASCII names.

Assume schedules arrive as this kind of export; whether to build an importer is an open question below.

## How a match runs

**The squad rotates as a whole, one round at a time.** Shooter 1 round 1, shooter 2 round 1, shooter 3 round 1, and so on to the end of the squad — then back to shooter 1 for round 2. After taking a turn a shooter waits for every other shooter in the squad before their next turn. A squad of 7 therefore runs 5 passes of 7 turns, 35 turns in total, and the round number only increments when the squad wraps around.

This matters for the data model and the UI: the active position advances within a round, and the round advances only on wrap. Do **not** design it as one shooter completing all five rounds before the next steps up.

1. The active squad is on the range.
2. The next shooter in squad order scans their barcode card and becomes the active shooter.
3. The range officer presses start on the timer device.
4. The shooter shoots the round.
5. The range officer presses stop. The turn ends.
6. The timekeeper advances to the next shooter in the squad. Back to step 2.
7. When the last shooter in the squad has finished, the round number increments and the rotation returns to the first shooter — until all 5 rounds are done.
8. When every squad member has completed 5 rounds, any outstanding reshoots are shot off (see below).
9. Each shooter checks their recorded times with the timekeeper and signs the paper sheet.
10. Once every round has a time and every sheet is signed, the match is over for that squad. A new squad arrives — back to step 1.

### Reshoots

A shooter is allowed **one** reshoot if they have a malfunction during a round. A reshoot is **deferred, not taken immediately** — it does not interrupt the rotation:

1. The shooter has a malfunction in, say, round 3.
2. That round's result is recorded as **`RS`** rather than a time. It is a marker, not a number — round 3 has no time until the reshoot is taken.
3. The rotation carries on untouched. The rest of the squad completes all 5 rounds.
4. Once the squad has finished, that shooter takes their reshoot for round 3.
5. The time from the reshoot becomes the round 3 result, replacing the `RS` marker.

So a round result is either a recorded time or the `RS` marker awaiting a reshoot, and a squad is not finished while any `RS` is outstanding. Model the round result as a small state — pending, timed, or `RS`-awaiting-reshoot — rather than a nullable time field, and make sure the reshoot writes back to the *original* round number rather than appending a sixth round.

The plan must cover:

- Where deferred reshoots are queued and how the timekeeper sees which ones are outstanding, given they may be requested many turns before they are taken.
- Whether more than one shooter in a squad can have an outstanding `RS`, and in what order those are shot off at the end.
- What the timekeeper screen shows during the reshoot phase, once the normal 5-round rotation is over but the squad is not yet complete.
- How `RS` appears on the printed/reviewed sheet the shooter signs, since sign-off happens after their rounds are done.

## The barcode scanner

A NETUM NT-EM61 2D CMOS scanner in HID mode — the OS sees it as a USB keyboard. Scans arrive as fast keystrokes ending in Enter, into whatever has focus.

The plan needs a capture strategy that does not depend on an input being focused, distinguishes a scan burst from human typing (inter-keystroke timing plus the terminating Enter), and handles unknown or unmatched codes. It must also honour the enable/disable rule below.

## Timekeeper screen

Behind a login. Shows:

- The current shooter's name and their time.
- **No split times on this screen.**
- The squad list in rotation order, with the active shooter highlighted, and the current round number.
- Clickable names, so the timekeeper can set the active shooter by hand when the scanner fails.
- A button to advance to the next turn. Because the squad rotates as a whole, this normally moves to the next shooter in the squad and only rolls the round number over when it passes the last shooter — one button, not separate next-shooter and next-round controls.

Since a shooter's five times are collected across five separate passes, the squad list should make each shooter's progress visible — which rounds they have completed so far, not just the current one.

**Hard rule:** manual shooter selection is only possible when **no session is active**. The barcode scanner is likewise active only when no session is active. Both are gated by the same condition — treat it as one guard in the state machine, and make the plan explicit about what the UI does if a scan or click arrives while a session is running.

## Requirements

### Must

- PayloadCMS + Next.js application; the existing PWA display becomes a route inside it.
- Shooter administration: create and edit shooters with first name, last name, ASN number, KNSA number.
- Squads with a time block, an ordered shooter list, and a per-membership discipline.
- Disciplines as an enum in code, all ten above.
- Barcode scan sets the active shooter, via KNSA number.
- Timekeeper screen as specified, behind authentication.
- Times captured per shooter per round from the existing MQTT event stream, five rounds per shooter.
- One reshoot per shooter on malfunction, deferred until the squad has finished its 5 rounds, with the affected round marked `RS` until the reshoot replaces it (see the open question on how the limit is scoped).
- Scanner and manual selection enabled only when no session is active.

### Should

- Sensible handling of the failure cases: an unknown barcode, a session that starts with no active shooter, a session that stops with zero shots, a timer disconnecting mid-round.
- Ability to review and correct recorded times before the squad is signed off.

### Out of scope

- The paper sign-off flow and the shooter's signature.
- The match director's entry into the external results system.
- Any change to the firmware or the MQTT contract, unless proven unavoidable.
- Ranking, scoring formulas, or published results — unless the answer to the "what is a score" question below says otherwise.

## Open questions — ask me these before finalising the plan

Give a recommended default for each so the plan is not blocked, but do not silently bury them as assumptions.

1. **Where MQTT is consumed.** Does the Next.js server subscribe to `timer/<deviceId>/…` and persist to Payload, or does the timekeeper's browser subscribe and POST results? Only server-side capture survives the timekeeper closing their laptop — recommend accordingly and let me confirm.
2. **The `/admin` collision.** Payload already owns `/admin`. My notes say the timekeeper screen lives under `/admin` with a login. Is it a Payload custom admin view, or a separate route such as `/timekeeper` using Payload auth? Flag the trade-off.
3. **What "score" means here.** The system records *times*; paper stays authoritative and the match director enters results elsewhere. So is this a capture aid that prints a sheet, or is it becoming the results system? This decides whether export and sign-off features are in scope at all.
4. **Timer-to-squad binding.** How does a `deviceId` map to a squad, range, or lane — configured once, chosen by the timekeeper, or auto-bound to the first online device?
5. **Placement and stack.** New app directory alongside `pwa-display-app/`, or convert that app in place? npm workspaces or standalone? Which database does Payload use — Postgres or Mongo? These are unstated.
6. **PWA migration specifics.** How the Vite app moves to Next.js: the MQTT hook becomes a client-only component, `vite-plugin-pwa` is replaced by a Next PWA setup, and the Redux store carries over. Confirm the PWA route keeps working standalone on a tablet.
7. **Schedule import.** Should squad schedules be imported from an export like the PDF above (or a CSV of it), or entered by hand in the Payload admin?
8. **Missing KNSA numbers.** In the reference export the ASN and KNSA columns are dashes — I can't tell whether that is placeholder data or genuinely absent. If some shooters have no KNSA number they cannot be scanned at all. Does manual selection need to be a permanent first-class path rather than a scanner fallback, and can a shooter be entered without a KNSA number?
9. **Reshoot scope and edge cases.** The reshoot mechanic itself is settled above, but the limit is not: is it one reshoot per match, per squad entry, or per discipline? A shooter entered in two disciplines makes this materially different. Also: what happens if the reshoot itself malfunctions, and can a shooter with an outstanding `RS` sign off before it is taken?

## Constraints

- Follow the conventions in [CLAUDE.md](CLAUDE.md), including conventional commits.
- Do not use the `void` operator in click handlers or callbacks.
- Reuse the existing MQTT types and topic helpers rather than restating them.
- Use [mqtt-simulator/](mqtt-simulator/) to develop and test without hardware.

## Deliverable

A staged plan covering:

- Payload collections and their relationships, with the membership-owns-discipline-and-times constraint respected.
- The match/round state machine, including the no-active-session guard, reshoots, and the failure cases.
- Route structure, including where the timekeeper screen and the migrated PWA display live, and the auth boundary.
- The MQTT ingestion path and how a timer session is bound to (shooter, discipline, round).
- Barcode capture strategy.
- Build order in phases that each leave the app working, with the riskiest unknowns resolved first.

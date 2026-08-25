# Plan-mode prompt: match score-keeping app (PayloadCMS + Next.js)

> Source notes: [.claude/PLAN/score-keeping-prompt.md](.claude/PLAN/score-keeping-prompt.md)
> Paste everything below into plan mode.

---

Produce an implementation plan for a match score-keeping web application built on **PayloadCMS + Next.js**, into which the existing PWA display app is absorbed as a route.

Do not write implementation code. I want a plan: data model, routes, state machine, integration points, a staged build order, and an honest assessment of whether the proposed technologies fit the problem. Ask me the open questions listed at the end before finalising — several of them change the shape of the whole design.

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
- [ESP32-S3-firmware/](ESP32-S3-firmware/) — the firmware that publishes the events, and the HUB75 LED panel it drives. **Strictly off limits.** Do not plan, propose, or design any change here — not to the firmware, not to the matrix rendering code, not to the MQTT contract it publishes. Treat the event stream as a fixed external input. If something appears to require a firmware change, redesign around it or flag it as a blocker; do not plan the change.
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

**The squad rotates as a whole, one round at a time.** Everyone shoots round 1 before anyone starts round 2. After taking a turn a shooter waits for every other shooter in the squad before their next turn. A squad of 7 therefore runs 5 passes of 7 turns, 35 turns in total, and the round number only increments once the whole squad has finished the current round.

The order within a round is a queue that the timekeeper can rearrange mid-match (see below), so treat "whose turn is next" as a property of that live queue rather than a fixed index into the squad list.

This matters for the data model and the UI: the active position advances within a round, and the round advances only on wrap. Do **not** design it as one shooter completing all five rounds before the next steps up.

1. The active squad is on the range.
2. The next shooter in the queue scans their barcode card and becomes the active shooter.
3. The range officer presses start on the timer device.
4. The shooter shoots the round.
5. The range officer presses stop. The turn ends.
6. The timekeeper closes off the turn, which re-enables the scanner and lets the *next* shooter scan in. Back to step 2.
7. Once every active shooter has a result for the current round, the round number increments and the queue starts again from the front — until all 5 rounds are done.
8. When every squad member has completed 5 rounds, any outstanding reshoots are shot off (see below).
9. Each shooter checks their recorded times with the timekeeper and signs the paper sheet.
10. Once every round has a time and every sheet is signed, the match is over for that squad. A new squad arrives — back to step 1.

**Who chooses the active shooter.** Step 6 is a release, not a selection. The timekeeper does not pick who shoots next — they end the current turn and re-arm the scanner, and the next shooter makes themselves active by scanning their own card. The queue tells everyone whose turn it is, but the scan is what sets the active shooter. Manual selection by clicking a name exists only as a fallback for a failed scan (see the timekeeper screen below). Do not design step 6 as "timekeeper selects next shooter".

### The shooting order is a mutable queue, not a fixed list

The squad's position numbers (`#1`…`#7`) give the *starting* order, but the real order changes during a match and the system must allow it. Real cases the timekeeper deals with:

- A shooter arrives late — they are not there for round 1 but join later.
- A shooter steps away right before their turn (the toilet, a gun problem, fetching ammo). Physically the timekeeper moves their scoring paper to the bottom of the stack, and they shoot later in that same round.
- A shooter does not show up at all and is skipped entirely.

So model the shooting order as an **ordered queue that can be rearranged mid-match**, not as a fixed list iterated by index. Note the paper metaphor exactly matches a queue operation: moving the card to the bottom of the stack is "send to back of the current round".

The plan must cover:

- Reordering the queue during a match: at minimum, send a shooter to the back of the current round; ideally drag-to-reorder. Only allowed when no session is active, like the other queue mutations.
- Marking a shooter as absent/skipped, and letting them rejoin later at a chosen position — a late arrival should be able to shoot their missed rounds if the match allows, so say what happens to rounds they were absent for.
- The consequence for round-advance: if the order is mutable, "the round advances when the last shooter finishes" cannot mean a fixed final index. The round advances when **every active shooter in the squad has a result for the current round** — derive it from completion, not position.
- What a rearrangement means for a shooter mid-way through their 5 rounds: their already-recorded rounds are untouched, only their queue position changes.
- Whether a skipped shooter blocks squad completion, given a squad is otherwise not finished until everyone has 5 results.

### Reshoots

A shooter is allowed **one** reshoot if they have a malfunction during a round. A reshoot is **deferred, not taken immediately** — it does not interrupt the rotation:

1. The shooter has a malfunction in, say, round 3.
2. That round's result is recorded as **`RS`** rather than a time. It is a marker, not a number — round 3 never gets a time.
3. The rotation carries on untouched. The rest of the squad completes all 5 rounds.
4. Once the squad has finished, that shooter takes their reshoot.
5. The reshoot time is recorded in its **own separate field**, not written back into round 3. The `RS` marker stays.

**The `RS` marker is permanent.** See the score card examples below: round 3 still reads `RS` on the finished card, and the reshoot time sits in a dedicated `Reshoot:` field alongside the five rounds. Do not model the reshoot as overwriting the failed round — the card deliberately preserves both, so it stays visible that round 3 malfunctioned and what was shot in its place.

So a round result is either a recorded time or the permanent `RS` marker, and the reshoot is a sixth, separately-labelled value that exists only when some round is `RS`. A squad is not finished while an `RS` has no matching reshoot time. Model the round result as a small state — pending, timed, or `RS` — plus one optional reshoot time on the shooter's card, rather than a nullable time field per round.

Since the card has a single `Reshoot:` field and the allowance is one reshoot, at most one round per card can be `RS`.

The plan must cover:

- Where deferred reshoots are queued and how the timekeeper sees which ones are outstanding, given they may be requested many turns before they are taken.
- Whether more than one shooter in a squad can have an outstanding `RS`, and in what order those are shot off at the end.
- What the timekeeper screen shows during the reshoot phase, once the normal 5-round rotation is over but the squad is not yet complete.
- Sign-off timing. A shooter with an outstanding `RS` and no reshoot time yet **waits** — they do not sign until the reshoot has been taken. They then sign a card that still shows `RS` for that round plus the reshoot time. There is no partial or repeated sign-off.

## The score card

This is the artefact the shooter checks and signs, and what the match director carries away. Two real examples — the first a clean card, the second with a reshoot:

```
# Score Card

Name: John Piper
ANS Number: 1122334455
KNSA Number: 5544332211
Disicipline: OKP

Round times
1: 11.45
2: 10.64
3: 10.12
4: 09.99
5: 09.76

Reshoot:


Signature:


------------------------------
# Score Card

Name: Peter Wik
ASN Number: 123456
KNSA Number:  654321
Disicipline: OKP

Round times
1: 11.45
2: 10.64
3: RS
4: 09.99
5: 09.76

Reshoot:  09.95


Signature:
```

What this tells you about the model:

- **One card per shooter per discipline** — the card carries a single `Disicipline` field, so a shooter entered in two disciplines gets two cards. This matches the squad-membership constraint above: the card *is* the membership row rendered.
- Times are shown as **`SS.CC`** — seconds and centiseconds, two decimals, zero-padded (`09.99`, not `9.99`). The MQTT feed carries milliseconds, so the card rounds or truncates on render; the plan should say which, and keep full millisecond precision in storage.
- `Reshoot:` is a first-class field on every card, left blank when unused — not an appended note.
- The card shows **no split times**, consistent with the timekeeper screen.
- The heading, field labels and layout above are reproduced verbatim from the real cards, typos included (`ANS Number` on one, `ASN Number` on the other; `Disicipline` on both). Treat the *structure* as authoritative, not the spelling — use correct spelling in code and in any generated card, but keep the same fields in the same order.

Generating this card is likely the concrete output of the whole capture flow — see the open question below on whether printing it is in scope.

## The barcode scanner

A NETUM NT-EM61 2D CMOS scanner in HID mode — the OS sees it as a USB keyboard. Scans arrive as fast keystrokes into whatever has focus. String is terminated with TAB character.

The plan needs a capture strategy that does not depend on an input being focused, distinguishes a scan burst from human typing (inter-keystroke timing plus the terminating Enter), and handles unknown or unmatched codes. It must also honour the enable/disable rule below.

## Timekeeper screen

Behind a login. Shows:

- The current shooter's name and their time.
- **No split times on this screen.**
- The squad queue in current shooting order, with the active shooter highlighted, and the current round number. It should also make clear who is **next** and who is **on deck** (see the display section below for what those mean).
- Clickable names, so the timekeeper can set the active shooter by hand when the scanner fails. This is a fallback for a broken scan, not the normal way a shooter becomes active.
- Controls to rearrange the queue — send a shooter to the back of the current round, mark them absent, or reorder them — per the mutable-queue section above.
- A button to end the current turn, which re-arms the scanner for the next shooter. Because the squad rotates as a whole, the round number rolls over on its own once every active shooter has a result for the current round — one button, not separate next-shooter and next-round controls.

Since a shooter's five times are collected across five separate passes, the squad list should make each shooter's progress visible — which rounds they have completed so far, not just the current one.

**Hard rule:** manual shooter selection is only possible when **no session is active**. The barcode scanner is likewise active only when no session is active. Both are gated by the same condition — treat it as one guard in the state machine, and make the plan explicit about what the UI does if a scan or click arrives while a session is running. Queue rearrangement is gated the same way.

## The display app: queue callouts

**Scope: this is the PWA app only.** The callouts below are a change to [pwa-display-app/](pwa-display-app/) — the browser view — and nothing else. The physical HUB75 LED panel and the firmware that drives it are **not** part of this work: do not plan changes to the matrix rendering for the hardware, do not propose firmware changes to carry queue data, and do not treat the panel's 128×32 geometry as a constraint on the browser layout. The browser is a screen and can be laid out freely.

The PWA display that shooters and the range officer look at must also announce the queue, using the standard steel-plate range commands:

- **"Next shooter"** — the competitor who will shoot after the shooter currently being called. Shown in **large text at the bottom of the screen**: `Next: <name>`.
- **"Shooter on deck"** — the competitor after that, who should be getting ready at the shooting position. Shown in **smaller text**: `On deck: <name>`.

These are established range roles, not invented labels: when the range officer calls "shooter on deck", they are telling that competitor to get prepared and step up. Keep the wording and the relative ordering exact — on-deck is one further out than next.

Design notes for the plan:

- Both names come from the mutable queue above, so they must update live when the queue is rearranged, when a shooter is marked absent, or when a turn ends — not be computed once from the starting order.
- **There is already enough room on the PWA screen** — this is not a layout problem to solve. The callouts go in the page below the simulated matrix view. Do not treat available space as a constraint, do not propose scrolling or a taller canvas to make them fit, and leave the matrix rendering itself alone.
- The PWA renders a simulated **128×32** matrix (see [pwa-display-app/src/constants.ts](pwa-display-app/src/constants.ts)) to mimic the panel. The callouts sit outside it, as ordinary page text, so the matrix geometry does not apply to them.
- Say what is shown when there is no next or on-deck shooter — end of round, end of squad, or during the reshoot phase.

## Requirements

### Must

- PayloadCMS + Next.js application; the existing PWA display becomes a route inside it.
- Shooter administration: create and edit shooters with first name, last name, ASN number, KNSA number.
- Squads with a time block, an ordered shooter list, and a per-membership discipline.
- Disciplines as an enum in code, all ten above.
- Barcode scan sets the active shooter, via KNSA number. The shooter makes themselves active by scanning; the timekeeper only releases the scanner between turns.
- A shooting order that can be rearranged mid-match: send to back of round, mark absent, reorder — to handle late arrivals, shooters who step away, and no-shows.
- `Next:` in large text and `On deck:` in smaller text on the display app, driven by the live queue.
- Timekeeper screen as specified, behind authentication.
- Times captured per shooter per round from the existing MQTT event stream, five rounds per shooter.
- One reshoot per shooter on malfunction, deferred until the squad has finished its 5 rounds. The affected round stays permanently marked `RS` and the reshoot time is stored in its own field (see the open question on how the limit is scoped).
- A score card per shooter per discipline, in the format above, showing five round results, the reshoot field, and space for a signature.
- Scanner and manual selection enabled only when no session is active.

### Should

- Sensible handling of the failure cases: an unknown barcode, a session that starts with no active shooter, a session that stops with zero shots, a timer disconnecting mid-round.
- Ability to review and correct recorded times before the squad is signed off.

### Out of scope

- The paper sign-off flow and the shooter's signature.
- The match director's entry into the external results system.
- **The ESP32-S3 firmware, the HUB75 LED panel, and the matrix rendering code — off limits entirely, no exceptions.** The MQTT contract they publish is fixed input, not something to extend.
- The [BLE-LoRa-Bridge/](BLE-LoRa-Bridge/) firmware, for the same reason.
- Ranking, scoring formulas, or published results — unless the answer to the "what is a score" question below says otherwise.

## Open questions — ask me these before finalising the plan

Give a recommended default for each so the plan is not blocked, but do not silently bury them as assumptions.

1. **Where MQTT is consumed.** I want the browser's responsiveness *and* durable storage, and I have not decided how to get both. My instinct is that these are not exclusive: **both** subscribe to the same broker. The timekeeper's browser subscribes directly so the live time updates with no server round-trip, and the Next.js server subscribes independently and persists every event to Payload as the system of record. The browser then renders from its own live feed but treats the server's stored data as authoritative on reload or reconnect.

   Evaluate that against the alternatives (server-only with push to the browser over SSE/WebSocket; browser-only with POST) and tell me if the dual-subscriber approach is wrong. The specific things I care about: the recorded time must survive the timekeeper closing their laptop or losing Wi-Fi mid-round; and the live display must not feel laggy. Note that both subscribers see the *same* `sessionId`, so the plan needs to say how a browser-side view and a server-side record of the same session are reconciled without double-counting, and which side owns binding a session to (shooter, discipline, round).
2. **The `/admin` collision.** Payload already owns `/admin`. My notes say the timekeeper screen lives under `/admin` with a login. Is it a Payload custom admin view, or a separate route such as `/timekeeper` using Payload auth? Flag the trade-off — and answer it together with the Payload fitness question in the technology review, since they are the same decision seen from two sides.
3. **What "score" means here, and whether the card is printed.** The system records *times*; paper stays authoritative and the match director enters results elsewhere. So is this a capture aid, or is it becoming the results system? Now that the score card format is known, the sharper version of this question is: should the app **generate and print the filled-in card** for the shooter to sign — replacing the hand-written sheet with a printed one — or does the card stay hand-written and the app merely mirrors it on screen? Printing is the obvious win, since the times are already captured and transcription is where errors creep in. Recommend accordingly.
4. **Timer-to-squad binding.** How does a `deviceId` map to a squad, range, or lane — configured once, chosen by the timekeeper, or auto-bound to the first online device?
5. **Placement and stack.** New app directory alongside `pwa-display-app/`, or convert that app in place? npm workspaces or standalone? Which database does Payload use — Postgres or Mongo? These are unstated.
6. **PWA migration specifics.** How the Vite app moves to Next.js: the MQTT hook becomes a client-only component, `vite-plugin-pwa` is replaced by a Next PWA setup, and the Redux store carries over. Confirm the PWA route keeps working standalone on a tablet.
7. **Schedule import.** Should squad schedules be imported from an export like the PDF above (or a CSV of it), or entered by hand in the Payload admin?
8. **Missing KNSA numbers.** In the reference export the ASN and KNSA columns are dashes — I can't tell whether that is placeholder data or genuinely absent. If some shooters have no KNSA number they cannot be scanned at all. Does manual selection need to be a permanent first-class path rather than a scanner fallback, and can a shooter be entered without a KNSA number?
9. **Reshoot scope.** The reshoot mechanic and sign-off ordering are settled above. The card's single `Reshoot:` field implies one reshoot **per card** — i.e. per shooter per discipline — so a shooter in two disciplines would get one reshoot in each. Confirm that reading. Also: what happens if the reshoot itself malfunctions, given there is nowhere on the card to record a second one?

## Constraints

- Follow the conventions in [CLAUDE.md](CLAUDE.md), including conventional commits.
- Do not use the `void` operator in click handlers or callbacks.
- Reuse the existing MQTT types and topic helpers rather than restating them.
- Use [mqtt-simulator/](mqtt-simulator/) to develop and test without hardware.
- **Client application state should be handled with Redux Toolkit.** The existing PWA already uses it — store in [pwa-display-app/src/store/](pwa-display-app/src/store/), with MQTT and beep middleware — so this carries the pattern forward rather than introducing one. The live match state (active shooter, current round, the mutable queue, incoming shot events) is exactly the kind of shared cross-component state it suits. This is a preference, not an absolute: if some part of the app is better served otherwise, say so and justify it in the technology review below.

## Technology fitness review

Part of the plan is to **examine whether the chosen technologies actually fit this problem**, rather than assuming the stack and designing inside it. Treat this as real analysis with a recommendation, not a formality — and say plainly where a choice is a poor fit.

Cover at least:

- **PayloadCMS** — it is a content management system being used here for match operations, not content. Assess how well its collections, access control, admin UI and hooks fit a live, stateful, real-time workflow. Where does it help (auth, CRUD, admin scaffolding for shooters and squads), and where does it fight the problem (a live shooting queue, session state, event ingestion)? Is a Payload custom admin view the right home for the timekeeper screen, or should that be an ordinary Next.js route using Payload only for auth and data?
- **Redux Toolkit** — confirm it fits the live match state, and say how it interacts with server state. Be explicit about the boundary between RTK and whatever fetches from Payload, and whether RTK Query or the Payload client should own server data. Flag it if two state systems would end up overlapping.
- **Next.js** — App Router vs Pages, server vs client components given a persistent MQTT subscription, and whether SSR earns its place for a screen that is almost entirely live client state.
- **The MQTT client in the browser**, and how a long-lived subscription coexists with the Next.js lifecycle and React strict mode.
- **Database choice** for Payload (see the open question), judged against this workload rather than by general preference.
- **The barcode scanner as HID keyboard input** — whether global key capture is robust enough, or whether the scanner should be configured differently.

For each: does it fit, what does it cost, and what is the alternative if it does not. If the honest answer is that the stack is a poor fit for part of this, say so — I would rather hear it now than discover it mid-build.

## Deliverable

A staged plan covering:

- Payload collections and their relationships, with the membership-owns-discipline-and-times constraint respected.
- The match/round state machine, including the no-active-session guard, the mutable shooting queue, the deferred-reshoot phase, and the failure cases.
- How the display app derives the next / on-deck callouts from the live queue and keeps them in sync as it changes.
- How a score card is produced from the stored data, matching the format above.
- Route structure, including where the timekeeper screen and the migrated PWA display live, and the auth boundary.
- The MQTT ingestion path and how a timer session is bound to (shooter, discipline, round).
- Barcode capture strategy.
- The technology fitness review above, with a clear recommendation on each choice and on the RTK / server-state boundary.
- Build order in phases that each leave the app working, with the riskiest unknowns resolved first.

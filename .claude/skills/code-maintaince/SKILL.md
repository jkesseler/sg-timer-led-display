---
name: code-maintaince
description: Codebase Maintenance Skill
---

# Codebase Maintenance Skill

## Purpose

Maintain this codebase with the smallest reasonable change while producing readable, understandable, maintainable, production-quality code.

Optimize for:

1. Readability
2. Understandability
3. Low cognitive load
4. Consistency
5. Maintainability
6. Minimal change

Prefer explicit and predictable code over clever, compressed, or overly abstract code.


## What this repository is

A monorepo with four components. **Most of it is embedded C++**; the TypeScript is the smaller half.

| Directory | Language | Toolchain |
|---|---|---|
| `ESP32-S3-firmware/` | C++ | PlatformIO |
| `BLE-LoRa-Bridge/` | C++ (reuses `ESP32-S3-firmware/` sources) | PlatformIO |
| `mqtt-simulator/` | TypeScript | npm / tsx |
| `pwa-display-app/` | TypeScript + React | npm / Vite |

All four agree on the MQTT topic contract `timer/<deviceId>/<event>`. A change on one side of that contract usually implies a change on the other.

Identify which tree you are in before you start — the rules differ.


## Before Changing Code

Before modifying code:

1. Read `CLAUDE.md`. It is the primary source for architecture, build commands, and firmware constraints.
2. **For C++ changes** (`ESP32-S3-firmware/`, `BLE-LoRa-Bridge/`): read the Architecture, Embedded constraints, BLE implementation patterns, and Code conventions sections of `CLAUDE.md`, plus the Firmware section below.
3. **For TypeScript changes** (`pwa-display-app/`, `mqtt-simulator/`): read `.claude/rules/coding-conventions.md` and `.claude/rules/variable-naming.md`.
4. Read other relevant rules in `.claude/rules/`.
5. Inspect the target file.
6. Inspect related code when the change crosses module or architectural boundaries — in particular anything touching the MQTT contract or `NormalizedShotData`.
7. Inspect nearby code for established patterns only if the conventions do not apply.

Do not guess about project conventions or architecture when the codebase can answer the question.

Prefer established project patterns over introducing new ones.


## Code Style

`CLAUDE.md` is the source of truth for firmware conventions and architecture.
`.claude/rules/coding-conventions.md` is the source of truth for TypeScript and React conventions.
`.claude/rules/variable-naming.md` is the source of truth for naming variables and functions in TypeScript.

Follow them when:

- writing new code,
- modifying existing code,
- refactoring,
- naming variables or functions,
- choosing TypeScript patterns,
- structuring React components,
- adding or changing a firmware device driver.

When a code-style decision is ambiguous, prefer the option with lower cognitive load.

Do not choose a shorter or more "idiomatic" implementation merely because it uses fewer lines.

For example, prefer explicit control flow when it is easier to understand than a compact operator or expression.

Do not introduce advanced language features merely to make code shorter.


## Existing Code

Match nearby code when it is consistent with the project conventions.

Do not blindly copy existing code if it:

- clearly violates the coding conventions,
- increases unnecessary complexity,
- appears to be legacy code,
- is inconsistent with surrounding code.

When modifying existing code, avoid unnecessary stylistic changes.

Do not reformat or refactor unrelated code.


## Comments

Default: do not add comments.

Prefer clear names and structure over explanatory comments.

Add comments only when they explain something that cannot reasonably be understood from the code itself, such as:

- business rules,
- non-obvious constraints,
- workarounds,
- external-system behavior,
- surprising behavior,
- important implementation decisions.

Comments should explain **why**, not **what**.

Firmware earns comments more often than the TypeScript does, because the constraint is usually invisible in the code: a protocol quirk, a unit conversion, a BLE address-type subtlety, a timing requirement. Document protocol quirks inline in the device `.cpp` that handles them.

Avoid comments that merely describe obvious code.

Avoid JSDoc or Doxygen blocks unless they provide meaningful value, particularly for public APIs and device driver headers.


## Firmware (C++)

Applies to `ESP32-S3-firmware/` and `BLE-LoRa-Bridge/`. `CLAUDE.md` holds the full detail; this is the short list that matters most when editing.

**Units**

- All time values in `NormalizedShotData` are **milliseconds**, always. Devices reporting centiseconds multiply by 10 inside the device implementation. This invariant is load-bearing across firmware, MQTT, simulator and PWA.

**Real-time constraints**

- No heap allocation in notification callbacks or in `publishQueuedEvents()`.
- No `delay()` inside device driver classes — use `millis()` deltas and return promptly.
- Avoid floating-point math in performance-sensitive paths (BLE callbacks, display render loop).
- No heavy work inside a BLE notification callback: parse the bytes, fire the registered `std::function`, return. Never call display or MQTT code from within a callback.
- Do not call `u8g2_for_adafruit_gfx.getUTF8Width()` — use `DisplayManager`'s `textPixelWidth` estimate.

**BLE**

- Connection sequence: scan → connect → get service → get characteristic → register notify.
- Use full 128-bit UUID strings.
- Null-check every BLE object (`pClient`, `pService`, `pChar`) before use, and validate payload length before parsing.
- The static-instance + C-style `notifyCallback` pattern is required by the BLE library. Keep it; it is not accidental duplication.
- Never reconnect without the rate-limiting delay (`BLE_RECONNECT_INTERVAL`, `common.h`).

**Structure and conventions**

- Classes PascalCase; methods camelCase; constants `UPPER_SNAKE_CASE`; private members camelCase with no underscore prefix.
- `#pragma once` in every header.
- `std::unique_ptr` for component ownership in `TimerApplication`; raw `new` for BLE library objects.
- Keep device-specific protocol logic inside the device class. Nothing device-specific belongs in `TimerApplication`.
- Devices sharing a wire protocol share a base (`FrameProtocolTimerDevice`). Separate classes for separate hardware models are intentional even when they look near-identical — do not collapse them.

**Logging**

- Use the component-tagged macros (`LOG_SYSTEM`, `LOG_BLE`, `LOG_TIMER`, `LOG_DISPLAY`, `LOG_ERROR`, `LOG_DEBUG`). No bare `Serial.print` in production code.

**Duplicated logic to keep in sync**

- `DisplayManager::formatTime` and `DisplayManager::formatSplitTime` are replicated in `test/test_time_formatting/test_time_formatting.cpp`. Change one, change the other.


## TypeScript and React

Applies to `pwa-display-app/` and `mqtt-simulator/`. Follow `.claude/rules/coding-conventions.md`; that file carries the detail.

In general:

- Prefer type inference when the type is obvious.
- Prefer explicit types when they communicate important intent or prevent ambiguity.
- Prefer type-safe solutions over `any`, and avoid `unknown` — model the shape you actually use.
- Avoid unnecessary type assertions and unnecessarily complex type definitions.
- Keep types close to where they are used unless they are genuinely shared. Do not create a new `types.ts` to relocate a local type.
- Match nearby component structure; do not introduce `React.FC`.
- Prefer early returns over deeply nested JSX; avoid unnecessary effects and abstractions.
- Every subscription effect returns a cleanup that unsubscribes.
- `pwa-display-app` is a Vite SPA, not Next.js. `'use client'` does not belong here.

Note that there is **no ESLint config and no test framework** in either package, so nothing is machine-checked. Do not add either as a side effect of an unrelated change.


## Changes

Make the smallest change that completely solves the problem.

Before changing code, understand the requested behavior.

After changing code:

- Preserve existing behavior unless the change intentionally modifies it.
- Avoid unrelated refactoring.
- Avoid opportunistic cleanup.
- Avoid renaming unrelated code.
- Avoid introducing abstractions without a clear benefit.
- Remove complexity when doing so is directly related to the change.
- Do not rewrite working code merely to make it more idiomatic.

A small, clear change is preferable to a broad "improvement".

**Cross-component changes.** A change to the MQTT payload shape, topic structure, or field units is a change to a contract with three other components. Update every side or state explicitly which sides you did not update and why.


## Error Handling

Preserve existing error-handling conventions.

When adding error handling:

- Do not swallow errors without a reason.
- Do not expose sensitive information.
- Prefer actionable error messages.
- Handle expected failure modes explicitly.
- Avoid catching errors merely to rethrow them unchanged.

In firmware, degradation is the norm rather than the exception: MQTT is non-fatal and the device continues in display-only mode when the broker is unreachable. Preserve that. Do not make a transport failure fatal to the main loop.

Inspect nearby code before introducing a new error-handling pattern.


## Testing

After making changes, run the most relevant available checks.

Prefer focused verification over running unrelated tooling.

**Firmware** — all `pio` commands run from the repository root:

    pio test -e native-tests                          # host-only GoogleTest suites
    pio test -e native-tests --filter test_protocol_parsing
    pio run -e main-firmware                          # confirm it still compiles

Native tests cover protocol parsing, the ring buffer, and time formatting. `DisplayManager` and `MqttManager` are hardware-dependent and are not covered — changes there cannot be verified without a board, so say so.

When adding a timer device, add protocol tests under `ESP32-S3-firmware/test/test_protocol_parsing/` following the `ProtocolTestBase` pattern, and run the suite before touching hardware.

**TypeScript** — per package:

    cd pwa-display-app && npm run type-check          # note the hyphen
    cd pwa-display-app && npm run build
    cd mqtt-simulator  && npm run simulate            # no check scripts exist

Do not use auto-fix as a substitute for reviewing the resulting changes.

If a command modifies files, inspect the changes before considering the task complete.

Add or update tests when behavior changes and a test suite exists for that area.

Do not add a test framework to the TypeScript packages, or integration/end-to-end tests anywhere, unless explicitly requested.


## Verification

Before reporting completion:

1. Review the final diff.
2. Confirm that the requested behavior is implemented.
3. Confirm that unrelated files were not changed unnecessarily.
4. Run the relevant checks for the tree you changed (see Testing).
5. Inspect the output of commands that modify files.
6. Mention what was actually verified.

Never claim that a change works solely because it compiles or type-checks.

Firmware changes that depend on hardware cannot be fully verified from a host build. State that limit rather than implying the change is proven.

If something could not be verified, say so explicitly.


## LLM Behavior

When generating or modifying code:

- Prefer readability over brevity.
- Prefer explicit control flow when it reduces cognitive load.
- Do not use clever syntax merely because it is shorter.
- Do not introduce unnecessary abstractions.
- Do not introduce new patterns without first checking existing code.
- Do not refactor unrelated code.
- Do not silently change behavior.
- Do not make speculative improvements.
- Do not over-engineer the solution.
- Do not add defensive layers nobody asked for — in firmware, an extra null check in a BLE callback is not free.
- Preserve useful existing structure.
- Follow project rules before general language preferences.

When multiple valid implementations exist, choose the simplest implementation that is easy for another developer to understand.


## Communication

Keep responses concise.

When reporting completed work, explain:

- what changed,
- why it changed,
- what was tested.

Mention relevant limitations or verification failures.

Do not provide lengthy explanations unless requested.

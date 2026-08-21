---
paths:
  - "pwa-display-app/**/*.{ts,tsx}"
  - "mqtt-simulator/**/*.ts"
---

# TypeScript & React coding conventions

**Scope: the TypeScript packages only** — `pwa-display-app/` (React + Vite PWA) and
`mqtt-simulator/` (Node/tsx tool). The firmware trees (`ESP32-S3-firmware/`,
`BLE-LoRa-Bridge/`) are C++ and are covered by `CLAUDE.md` plus the firmware
section of the `code-maintaince` skill — nothing in this file applies to them.

## Purpose

Code is read far more often than it is written, and most of it is read by someone
who did not write it — including reviewers and LLMs. Optimise for the reader.

The guiding question for every decision below is: **how much does a reader have to
hold in their head to be sure what this does?** Fewer characters is not the goal.
Fewer things to work out is.

### How to read the rule levels

| Level | Meaning |
| --- | --- |
| **[Convention]** | Dominant existing pattern. Follow it; deviating needs a reason. |
| **[Recommended]** | The preferred direction. Existing code is not wrong. |

**Nothing here is machine-enforced.** `pwa-display-app` lists ESLint as a
dev-dependency and has a `lint` script, but **there is no ESLint config file in
the repository**, so `npm run lint` does not currently run. `mqtt-simulator` has
no lint or typecheck script at all. Treat every rule below as a convention a
human or an LLM has to uphold — no tool will catch a violation for you.

Where sources conflict, resolve in this order: explicit project requirements →
established codebase patterns → team conventions → general best practice.
Readability and maintainability outrank all of them; if a rule below ever makes
code harder to understand, the rule is wrong for that case.

---

## 1. Readability over brevity

When two versions do the same thing, prefer the one that needs less interpretation.

```ts
// GOOD — the control flow is a list you can read top to bottom:
// check, start if missing, return.
// Share an in-flight connection so concurrent callers don't dial the broker twice.
if (!state.inFlight) {
  state.inFlight = connectToBroker(brokerUrl)
    .then((client) => {
      state.client = client;

      return client;
    })
    .finally(() => {
      state.inFlight = undefined;
    });
}

return state.inFlight;
```

```ts
// BAD — same behaviour, but the reader must first recall that ??= assigns only
// when nullish, then infer the caching intent from the operator.
state.inFlight ??= connectToBroker(brokerUrl).then(/* ... */);
```

**Do not prefer a language feature because it is newer or shorter.**
`??`, `??=`, `||=`, optional chaining, ternaries, destructuring, implicit returns,
chained calls and advanced generics are all allowed — when they make the intent
*more* obvious. When the compact form hides a decision the reader needs to see,
write the decision out.

Practical tests:

- If you have to mentally evaluate two expressions at once, split them.
- If a line reads left-to-right as a sentence, chaining is fine. If you have to
  scan back and forth, break it up.
- Do not add a temporary variable just to lengthen a line — and do not remove a
  well-named one just to shorten it. Name a value when the name explains something
  the expression doesn't.

### Traps to avoid

Nested ternaries · nested destructuring · one-liners that pack a decision and an
action together · helper functions generic enough to serve two callers and clear
to neither · abstractions introduced before the second use case exists · functions
that mutate arguments or module state as a side effect of returning something ·
optimisation without a measurement.

---

## 2. Formatting  [Convention]

There is no formatter and no linter config in this repository — no Prettier, no
ESLint rules, no `.editorconfig`. Formatting is therefore held by convention and
by matching the file you are editing. Do not add a formatter or a lint config as
a side effect of an unrelated change; that is its own decision.

Match what the existing TypeScript already does:

- 2-space indent, semicolons.
- Single quotes in TS; double quotes in JSX attributes, with no braces around
  literal JSX props (`title="x"`, not `title={"x"}`).
- Trailing commas on multi-line literals.
- 1TBS braces, never on a single line. `if`/`else`/`for`/`while` always take
  braces, even for one statement.
- **Blank line before every `return`** that follows another statement.
- Arrow parentheses as-needed, but present when the body is a block:
  `items.map(item => item.id)` and `items.forEach((item) => { … })`.
- Line breaks go *before* operators; multi-line ternaries break consistently.
- `;` separates members in `type` and `interface` bodies.
- Quote object keys only when the key requires it.
- `const` by default, `let` when reassigned, never `var`.

**Always use `===` and `!==`.**

**Also expected** — template literals over string concatenation, and no
reassignment of function parameters.

**Escape hatches.** `@ts-expect-error` is for real constraints — a wrong
third-party type, a known library bug. Each needs a comment saying why. Do not
reach for one to silence a type error you could fix properly.

**There is no line-length limit.** That is not licence to write long lines. Break
wherever it aids scanning; a 200-character expression is still bad code.

---

## 3. Naming

Names carry most of the readability budget. Spend it here first.

Base naming rules — English, `camelCase`, boolean prefixes, verb prefixes,
singular/plural, no contractions, don't repeat context, match the name to the
result — are defined in [`.claude/rules/variable-naming.md`](./variable-naming.md).
Apply those first. Everything below is additive: TypeScript/React-specific
conventions that file does not cover.

| Kind | Convention | Example |
| --- | --- | --- |
| Components, types | `PascalCase` | `LEDMatrix`, `ShotEvent` |
| Hooks | `use` + what it gives you | `useMqttConnection` |
| Props type | `<Component>Props` | `LEDMatrixProps` |
| Event handler props | `on<Event>` | `onSettingsSave` |
| Local handlers | `handle<Event>` | `handleSubmit` |
| Fixed literal module constants | `SCREAMING_SNAKE_CASE` | `DEFAULT_BROKER_URL` |
| Everything else at module scope | `camelCase` | `mqttClient`, `displayConfig` |

### Rules that matter

- **No `I` or `T` prefixes on types.** TypeScript does not need them and they
  carry no information. `ShotEvent`, not `IShotEvent`.
- **`SCREAMING_SNAKE_CASE` is narrow.** Use it only for fixed, literal,
  module-level values that act as configuration — topic prefixes, timeouts,
  storage keys, regexes. A `const` holding a derived value, a client, or anything
  computed stays `camelCase`.
- **`handle*` vs `on*` is a real distinction**, not a synonym. `on*` is the prop
  the parent passes in; `handle*` is the local function that implements it.
- **`next`, `prev`, `min`, `max`** (base list in `variable-naming.md`): the
  replacement for something is `nextVariant`, not `newVariant` or `variant2`.
- **Prefer a real name over a single letter**: `error` beats `e`. Loop indices are
  fine.
- **Unused values are prefixed `_`** — parameters, variables and destructured
  array slots.
- **Verb prefixes**, beyond the base set in `variable-naming.md`: `build`
  alongside `compose` (create from existing), and `find` (search, may return
  nothing).

### MQTT contract names

Both TypeScript packages speak the firmware's MQTT contract, `timer/<deviceId>/<event>`.
**Field names that cross that wire are fixed by the firmware** — they mirror
`NormalizedShotData` (`sessionId`, `shotNumber`, `absoluteTimeMs`, `splitTimeMs`,
`timestampMs`, `deviceModel`, `isFirstShot`). Do not rename them on the TypeScript
side to suit a local style preference; the firmware is the source of truth and a
rename silently breaks the contract. The `Ms` suffix is load-bearing — all
firmware times are milliseconds.

### Files and folders

- **Components: `PascalCase.tsx`**, with a matching `PascalCase.css` beside them
  where they carry styles — `LEDMatrix.tsx` / `LEDMatrix.css`, `Settings.tsx` /
  `Settings.css`.
- **Utilities, services and plain modules: `camelCase.ts`** — `utils.ts`,
  `constants.ts`. Not kebab-case.
- Role folders — `components`, `hooks`, `store` — are lowercase.

---

## 4. Variables and destructuring

Destructure when it removes repetition. Do not destructure to save characters.

```ts
// GOOD — several values from one object, read once, named clearly.
const { brokerUrl, reconnectPeriodMs } = displayConfig.mqtt;
```

```ts
// GOOD — one value from a deep path: direct access says exactly where it lives.
const panelWidth = displayConfig.layout.panelWidth;
```

```ts
// BAD — the reader has to unpick three levels of braces to learn one name,
// and the shape of displayConfig is now scattered across the pattern.
const {
  layout: {
    panel: { width },
  },
} = displayConfig;
```

Rules of thumb:

- Multiple keys from the same object → destructure.
- One value, especially from a nested path → access it directly.
- Never nest destructuring more than one level.
- Do not reassign function parameters. Derive a new value instead.

---

## 5. TypeScript

Both packages run in `strict` mode. Types exist to make wrong code fail early, not
to decorate it.

**Keep the visual weight of types as low as you can.** Annotations compete with
the logic for the reader's attention. The best type is the one you never had to
write because inference already got it right. When you do write one, it should be
because it tells the reader something the code does not.

- **Use `interface` for object shapes.** Reserve `type` for unions, aliases,
  mapped and conditional types — the things `interface` cannot express.
  Do not mix both styles for object shapes within one file.
- **Let inference do the work, including return types.** Do not annotate a return
  type that TypeScript already knows. Add one only when inference produces
  something wrong, unreadably wide, or when you deliberately want a narrower
  public contract than the body implies.
- **Annotate a local only when the inferred type is wrong or genuinely unclear.**
- **`export function` and `export const … = () =>` are both fine for module-level
  functions.** Be consistent within a file rather than converting either way.
- **Name non-trivial types instead of inlining them.** A one-field inline type in
  a signature is fine; a multi-field object literal in a parameter position is not.
- **Keep a type next to its use.** `pwa-display-app/src/types.ts` exists and holds
  the shared MQTT event shapes — that is the right place for types genuinely used
  across components. Do not create a *new* `types.ts` just to relocate a type that
  one module uses.
- **Pass an options object once a function takes more than two parameters**, and
  give it a named `interface`. Five positional arguments do not read at the call
  site.
- **Type-only imports go in their own `import type` statement**, grouped by
  source. Do not mix values and types in one specifier list:

  ```ts
  // GOOD — one statement per concern. The type import is visibly a type import.
  import { connectToBroker } from './mqttClient';
  import type { ShotEvent, SessionEvent } from './types';

  // BAD — `type` buried mid-list. The reader has to parse each specifier
  // separately to know what is a value and what vanishes at build time.
  import { connectToBroker, type ShotEvent, type SessionEvent } from './mqttClient';
  ```

- **Nullable values:** prefer `undefined` for "not provided" and optional
  properties over `| null`, unless an external contract gives you `null`. Handle
  absence explicitly at the top of a function rather than optional-chaining the
  same value repeatedly downstream.
- **Type assertions (`as`) are a claim the compiler cannot check.** Use one only
  at a boundary whose shape you have just verified, adjacent to the check that
  proves it. Never chain assertions to force an unrelated type.
- **Generics only when a real relationship exists** between an input and an output
  type. A generic with one call site is indirection.

### Untyped and unknown values

**Model the shape you actually use.** When data arrives from outside the type
system — an MQTT payload, a parsed JSON document, a loosely typed library — write
an `interface` for the fields you read and move on. A small honest interface beats
both a wide `any` and an `unknown` you then have to unwrap.

MQTT payloads are the main case here: they arrive as `Buffer`/`string`, and
whatever you parse out of them is unverified at runtime. Parse once at the
boundary, into a named `interface` that matches the firmware contract, and let the
rest of the app work with a real type.

- **Avoid `unknown`.** It does not describe anything; it defers the description
  and forces narrowing ceremony at every use site. The guard clauses it demands
  are noise unless the value's shape is genuinely in doubt at runtime.
- **`any` is acceptable where a type is genuinely not worth expressing** — a
  third-party shape you touch once. Keep it contained: name the fields you use
  rather than letting `any` spread through call chains and swallow real errors
  downstream.
- **`never` is for exhaustiveness checks** in discriminated unions — useful for a
  `switch` over event types, and cheap to read.

**Caught errors are the one place narrowing pays for itself.** `strict` mode types
`catch` as `unknown`, and a logger needs a real `Error` or the stack is lost.
Convert once, at the catch:

```ts
} catch (error) {
  console.error('[mqttClient] subscribe failed — display will stay on last state',
    error instanceof Error ? error : new Error(String(error)));
}
```

---

## 6. Functions and control flow

The target is code you can read once, top to bottom, without backtracking.

- **Guard clauses first.** Validate, bail out, then do the work. Prefer returning
  early over an `else` branch.
- **Keep nesting shallow.** Three levels is a practical ceiling. Deeper is usually
  a missing guard clause or a function that should be two functions.
- **A function should do one thing you can name.** If naming it needs "and", split
  it. There is no line limit, but a function that no longer fits on a screen is
  usually holding several ideas.
- **`async`/`await` over `.then()` chains** **[Convention]** — `await` reads as a
  sequence of steps. `.then()` still earns its place when you need the promise
  itself rather than its value: sharing an in-flight request, attaching cleanup
  with `.finally()`, storing a pending operation (see §1).
- **Use `Promise.allSettled` when independent operations should not cancel each
  other**, and handle each rejection explicitly.
- **Handle errors where you can act on them.** Wrap the narrowest operation that
  can fail. An empty `catch` is a bug. A `catch` that logs and continues must make
  the fallback obvious — log *what* it fell back to, not just that it failed.
  This matters most around the MQTT connection: a dropped broker is expected, and
  the display should degrade visibly rather than silently freeze.
- **Do not catch merely to rethrow unchanged.**
- **No hidden side effects.** A function named `get*` must not mutate. If it does
  both, name it for the mutation or split it.

### Conditionals

- Ternaries are good for choosing between two simple values.
- **Never nest ternaries.** Use `if`/`return`, or extract a small named function.
- Avoid `condition ? true : false`.
- Prefer `if` over `&&`/`||` used purely for side effects. `cond && doThing()`
  states a boolean expression while meaning "call it if present". Write what you
  mean.
- Extract a compound condition into a named boolean when it encodes a rule:
  `const shouldRedraw = isConnected && shots.length > 0;`

---

## 7. React

Applies to `pwa-display-app/` only. This is a **Vite** SPA, not Next.js — there
are no server components, and `'use client'` has no meaning here. Do not add it.

- **Function components, arrow style** **[Convention]**:
  `export const Settings = () => { … }`. Module-level helpers in the same file are
  plain `function` declarations.
- **Do not use `React.FC`.** It adds nothing over typing the props parameter and
  obscures the actual signature.
- **Destructure props in the signature** and use them consistently.

```tsx
// GOOD — the component's inputs are visible in one line.
export const LEDMatrix = ({ shots, isConnected, onReset }: LEDMatrixProps) => {
```

```tsx
// BAD — three access styles for one object. A reader cannot tell whether
// `props?.` guards a real case or is defensive noise.
export const LEDMatrix = (props: LEDMatrixProps) => {
  const { shots } = props;
  const label = buildLabel(shots, props?.isConnected);
  if (!props.onReset) { /* ... */ }
```

- **Props type lives next to the component**, named `<Component>Props`. Export it
  only if another module needs it.
- **Component body order:** hooks → derived values → effects → handlers → early
  returns → JSX. Consistent ordering means a reader always knows where to look.
- **Derive during render; do not mirror props into state.** Compute plain values
  inline. Reach for `useMemo`/`useCallback` only for a measured cost or a
  genuinely required stable identity.
- **Effects synchronise with something outside React** (MQTT subscriptions,
  timers, the DOM). An effect that only computes a value from props should be a
  derived value instead. **Every subscription effect must return a cleanup** that
  unsubscribes — a leaked MQTT handler keeps firing into an unmounted component.
- **Never suppress a dependency warning to make an effect behave.** The usual
  temptation is an effect that writes state it also depends on, with the
  dependency omitted to stop the loop. That works by accident and breaks under
  edits. Fix the data flow — derive rather than store, move the logic into the
  event handler that caused the change, or use a ref for a value you deliberately
  do not want to react to.
- **Conditional rendering:** early-return for whole-component states (loading,
  disconnected, empty). Inside JSX use `{condition && <X />}` or a single ternary
  — if you need more branches, extract a component or compute the element above
  the return.
- **Redux Toolkit holds shared app state** (`src/store`). Keep transient,
  component-local state in `useState`; put in the store what more than one
  component genuinely needs. Do not duplicate a store value into local state.
- Extract pure display logic (time formatting, text selection) into module-level
  functions above the component. It keeps the component body about behaviour and
  makes the logic testable without rendering.

---

## 8. Imports

There is no import-order linter, so this is convention. Order by path group:

1. Node builtins
2. External packages
3. Parent (`../`)
4. Sibling (`./`)

**There are no path aliases configured** in either package — no `@/*`. Use
relative imports. If a relative chain gets deep enough to be unreadable, that is a
signal the module is in the wrong place, not a reason to add an alias mid-change.

**[Recommended]** Put type-only imports at the end of the block, and keep them in
their own `import type` statements (see §5).

---

## 9. Comments

Write code that does not need explaining — good names, small functions, explicit
control flow. Then comment what the code cannot say. Most lines need no comment;
the ones that do usually encode a decision someone would otherwise undo.

**Comment when it explains a decision or a constraint:**

- business rules, and why a threshold or special case exists
- non-obvious external-system behaviour — especially firmware/MQTT quirks
- workarounds, with a link or ticket
- why a surprising implementation was chosen over the obvious one
- why a `@ts-expect-error` is justified

**Do not narrate the code:**

```ts
// GOOD — explains why the value is what it is. Without this, someone "tidies"
// the units away and the display silently reads 100× fast.
// Firmware publishes every time in milliseconds; the panel renders centiseconds.
const MS_PER_CENTISECOND = 10;

// BAD — the line below already says this.
// Set milliseconds per centisecond to 10.
const MS_PER_CENTISECOND = 10;
```

JSDoc is worth writing for exported functions whose contract is not obvious from
the signature — ownership, error behaviour, side effects, units. It is not
required on every export, and a block that restates the parameter names is worse
than none.

Mark unfinished work as `// TODO:` with enough context to act on it.

---

## 10. Errors and logging

- **`console` is the logger here.** There is no logging framework in either
  package. Use `console.error` / `console.warn` for real problems;
  `console.log` should not reach `master`.
- **Prefix the message with the originating module in brackets** so a line is
  greppable — `'[mqttClient] subscribe failed'`. This mirrors the firmware's
  `LOG_*` component tags and makes the two sides of the contract read alike.
- **Always log a real `Error`** — see the narrowing idiom in §5. A string loses
  the stack trace, which is the reason to log at all.
- Never log secrets, tokens or credentials. Broker URLs with embedded credentials
  count.
- Degrade gracefully: a caught error that leaves the app usable should still be
  visible to the user — a disconnected display that looks connected is worse than
  one that says it is offline.

---

## 11. For LLMs generating or modifying code

- **Match nearby code** when it does not conflict with these conventions.
  Consistency within a file beats global consistency. Where this document
  conflicts with surrounding code, follow it for new code and leave the rest alone.
- **Change only what was asked.** No unrelated refactoring, no renaming, no
  reformatting untouched lines, no "while I was here" improvements. Preserve
  existing behaviour — including edge cases that look like bugs. Report them
  instead of fixing them silently.
- **Make the smallest change that completely solves the problem.**
- **Do not modernise working code.** A `.then()` chain or a `function` declaration
  that works is not a defect.
- **Do not rename anything that crosses the MQTT contract** (see §3) without
  changing the firmware to match. The two sides must agree.
- **Prefer readability over brevity** every time. Do not compress readable
  multi-line code into one-liners, and do not reach for an advanced language
  feature without a readability gain you could defend in review.
- **Do not abstract speculatively.** Write the second use case before extracting
  the shared helper.
- **Do not add defensive layers nobody asked for** — redundant null checks,
  try/catch around code that cannot throw, validation the type system already
  guarantees. Generated code drifts toward this; resist it.
- **Keep types quiet.** Do not add return-type annotations inference already
  provides, do not reach for `unknown`, and do not generate narrowing guards for
  values whose shape you already know. Write an `interface` and move on.
- **Do not add a linter, formatter or test framework as a side effect** of an
  unrelated change. Their absence is the current state of the project; adding one
  is a decision to raise separately.
- **Verify before reporting done.** In `pwa-display-app`, run
  `npm run type-check` (note the hyphen — it is not `typecheck`) and
  `npm run build`. `mqtt-simulator` has no check scripts; run it with
  `npm run simulate` if the change is behavioural. Say what you actually ran.
  Type-checking alone is not evidence that a change works.

---
applyTo: "ESP32-firmware/**/*"
---

# GitHub Copilot Code Review Instructions (Path-Scoped)
**Scope:** This file applies only to code inside `ESP32-firmware/`
**Platform:** ESP32-S3 (Arduino + PlatformIO)
**Language:** Embedded C++

These rules define how GitHub Copilot should review pull requests that modify firmware code.

---

# 1. Review Priorities

## 1.1 Correctness & Safety
- Verify **all time values** are converted to **milliseconds** before being placed in `NormalizedShotData`.
- Ensure no parsing occurs without validating BLE payload length and field boundaries.
- Flag unsafe memory patterns:
  - Missing null checks on BLE objects (`pService`, `pChar`, `pClient`).
  - Stack-heavy arrays or large temporary buffers.
  - Raw pointers used where `std::unique_ptr` ownership is expected.
- Highlight heavy or blocking operations inside:
  - BLE notification callbacks
  - Main update loop
  - Device-specific parsing routines

## 1.2 Interface Compliance
All timer devices **must** fully implement `ITimerDevice`.

Flag PRs where:
- A method is missing or incorrectly implemented.
- Device-specific code leaks into `TimerApplication`.
- `update()` is not used according to the architecture (must be called only from main loop).

## 1.3 BLE Protocol Checks
Copilot should validate:
- Correct use of full 128-bit UUID strings.
- Connection pattern follows:
  `scan → connect → get service → get characteristic → register notify`.
- No reconnection spam or reconnect loops without delays.
- Notification handlers forward work to processing functions instead of performing heavy tasks inline.

---

# 2. Style & Organization Checks

## 2.1 Naming Rules
- **Classes:** PascalCase
- **Methods:** camelCase
- **Constants:** UPPER_SNAKE_CASE
- **Private members:** camelCase
- **Static strings:** `static const char*` or `constexpr`

Flag inconsistent naming.

## 2.2 Directory & File Structure
Ensure PRs follow structure:
```
ESP32-firmware/
├── include/ # .h files only
└── src/ # .cpp implementations only
```

- No mixed header/implementation code.
- New devices must include both `.h` and `.cpp` in their proper directories.

## 2.3 Logging Guidelines
Copilot must check:
- Errors use `LOG_ERROR("<COMPONENT>", ...)`
- BLE operations use the `BLE` tag
- No stray `Serial.print` in production firmware
- Debug logging uses appropriate level

---

# 3. Embedded Constraints to Enforce

## 3.1 Performance
Highlight:
- Unnecessary heap allocations inside fast loops.
- Large stack allocations in callbacks.
- Use of floating-point math in performance-sensitive locations.
- Misuse of `delay()` in device drivers.

## 3.2 Memory Provisions
Flag when:
- Data could be static instead of dynamically allocated.
- PSRAM is incorrectly assumed for buffers that must reside in internal RAM.
- BLE objects leak ownership or are freed incorrectly.

---

# 4. Display & State Machine Rules

Copilot should enforce that PRs:
- Modify display logic only through `DisplayManager`.
- Use correct RGB565 conversion helpers.
- Maintain valid display states:
  `STARTUP, DISCONNECTED, SCANNING, CONNECTED, WAITING_FOR_SHOTS, SHOWING_SHOT, SESSION_ENDED`
- Avoid unnecessary clearing/redrawing that can cause flicker.
Do not suggest the usage of `1u8g2_for_adafruit_gfx.getUTF8Width()`

---

# 5. Testing Requirements

For PRs that modify:
- BLE parsing
- Device logic
- Timing conversions
- State transitions

Copilot must require:
- Updated or new tests under `ESP32-firmware/test/`
- Tests clearly validating parsing and time normalization
- Use of proper PlatformIO test environments

---

# 6. Copilot PR Review Checklist

Copilot should evaluate each PR against this checklist:

### Architecture
- [ ] Follows facade pattern
- [ ] No device-specific logic outside device classes
- [ ] All `ITimerDevice` methods are implemented

### Safety
- [ ] BLE pointers validated before use
- [ ] BLE payload lengths checked
- [ ] No heavy work inside notification callbacks
- [ ] Time normalization correct (always ms)

### Style
- [ ] Naming rules followed
- [ ] Logging consistent with component tags
- [ ] File structure correct

### Testing
- [ ] Relevant unit tests updated/added
- [ ] Parsing and timing logic covered

---

# 7. Review Tone Guidance

Copilot should:
- Provide concrete, technical comments
- Highlight architecture or safety violations first
- Suggest small targeted fixes, not full rewrites
- Emphasize embedded constraints (performance, memory, timing)
- **ONLY review and provide feedback** - never implement changes

Copilot should avoid:
- Vague or generic feedback
- Overly broad refactoring suggestions
- Ignoring embedded-specific concerns
- **NEVER** create new pull requests or issues
- **NEVER** implement suggestions or fixes itself
- **NEVER** push commits or make code changes on behalf of developers
- **NEVER** modify files without explicit user request in the conversation

---

# 8. Implementation Boundaries

**Critical Rule:** Copilot is a **review and advisory tool only** for this project.

Copilot **MUST NOT:**
- Automatically fix flagged issues
- Create commits or push changes
- Open pull requests (even suggested ones)
- Modify files as part of review feedback
- Take autonomous actions beyond providing code review comments

**Copilot Role:**
- Analyze PR code against the checklist
- Provide specific, actionable feedback
- Identify violations with line references
- Explain why something violates project standards
- Wait for developer to implement any changes
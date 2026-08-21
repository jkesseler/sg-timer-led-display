---
paths:
  - "*, **"
---

# Branch Naming

Format: `<type>/<description>`

Types: `feat`, `fix`, `bugfix`, `hotfix`, `release`, `chore`, `ai`, `copilot`, `cursor`, `claude`, `codex`

Examples:
- `feat/asn-tracker-support`
- `bugfix/ble-reconnect-loop`
- `chore/update-platformio-deps`

# Commit Messages

Format: `<type>[(<scope>)]: <description>`

Types: `feat`, `fix`, `chore`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `revert`

Subject in imperative mood, <= 50 characters.

Scopes match the component being changed - e.g. `lora-bridge`, `display`,
`mqtt`, `pwa`, `simulator`, or a device class name. Omit the scope when a change
spans the repository.

Examples:
- `feat(lora-bridge): add BLE-LoRa bridge firmware`
- `fix(display): prevent flicker on session end`
- `chore(deps): update PlatformIO libraries`

Use a body with `*` bullet points for additional detail when needed.

Squash-merging a pull request appends its number to the subject automatically
(`... (#13)`). Do not type one by hand.
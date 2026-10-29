# SENTINEL-X

**Project:** Design and Development of a Zero-Trust IoT Security Gateway Based on STM32F411RE with Wi-Fi and Edge Monitoring.

> Repository bootstrap: the folders and policies are scaffolding only. A folder existing does not mean its module is implemented or tested.

## Project architecture

- **STM32F411RE** — security enforcement point: validates messages and policy, maintains anti-replay state, and controls the local/device-side action.
- **ESP32-S3** — Wi-Fi and message transport bridge between MQTT and the STM32 UART link.
- **Raspberry Pi 4 / Edge Backend** — API, MQTT broker integration, persistence, identity/access control and audit services.
- **PC Control Center** — Python/PySide6 operator application.
- **Protocol contracts** — versioned interface definitions shared across components.

See `docs/architecture/` for architecture decisions and diagrams, `protocol/` for interface contracts, and `docs/setup/` for setup instructions.

## Repository map

```text
firmware/
  stm32/                 STM32CubeIDE firmware and MCU tests
  esp32/                 ESP32-S3 firmware
edge/backend/            Raspberry Pi edge services and migrations
pc-app/                  Desktop Control Center
protocol/                Shared message, UART, MQTT, REST and schema contracts
tests/
  unit/                  Unit tests
  integration/           Cross-module tests
  system/                End-to-end and fault tests
  fixtures/              Non-secret test data
configs/examples/         Safe example configuration only
docs/
  architecture/           System architecture and diagrams
  decisions/              Architecture Decision Records (ADRs)
  planning/               Roadmap, backlog and meeting decisions
  testing/                Test strategy and evidence
  setup/                  Development environment setup
tools/scripts/            Reproducible helper scripts
```

## Current status

- [x] Repository scaffold defined
- [ ] Team members added and access verified
- [ ] `develop` branch created
- [ ] Pull Request rules enabled
- [ ] Interface contracts reviewed and frozen
- [ ] Component skeletons build/run on their target environments
- [ ] First end-to-end message path demonstrated

Do not mark an item complete until there is evidence in the repository or a recorded test result.

## Branching and Pull Requests

- `main`: stable, reviewable baseline; no direct pushes.
- `develop`: shared integration branch; feature work targets this branch.
- `feature/<short-name>`: new work.
- `fix/<short-name>`: bug fixes.
- `docs/<short-name>`: documentation-only changes.
- `test/<short-name>`: tests and test infrastructure.

Open PRs into `develop`, include a clear summary and test evidence, and request at least one teammate review. Merge `develop` into `main` only for a reviewed milestone/release. Never force-push shared branches.

## Security rules — mandatory

1. Never commit real passwords, API/access tokens, Wi-Fi credentials, private keys, HMAC keys, certificates with private keys, or production data.
2. Commit safe templates such as `.env.example`; keep real values in a local `.env` or another approved secret store. `.env` is ignored by Git.
3. Never put secrets in source code, issue descriptions, screenshots, test logs, commit messages, or PR comments.
4. If a secret is committed, treat it as compromised: revoke/rotate it immediately. Removing it in a later commit is not enough.
5. Use synthetic data in tests. Never use real user credentials or real device secrets as fixtures.
6. HMAC is for message authenticity/integrity, not encryption. Do not claim the system is “secure” solely because it uses HMAC or a Zero-Trust-inspired design.

## First-time contributors

1. Read `CONTRIBUTING.md`.
2. Clone the repository and check out `develop` for feature work.
3. Create a task branch from the latest `develop`.
4. Make a focused change and test it where the relevant environment is available.
5. Open a PR into `develop`; do not push directly to `main`.

Component-specific setup guides will be added under `docs/setup/` before the corresponding component is implemented.

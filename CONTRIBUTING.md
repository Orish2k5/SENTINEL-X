# Contributing to SENTINEL-X

## Before starting

- Read the current Blueprint and the relevant interface contract in `protocol/`.
- Confirm the task ID, owner, acceptance criteria and dependencies with the team.
- Do not implement an interface by guessing fields, byte order, limits or error codes. Propose the change and update the contract first.

## Branches

Start from the latest `develop` and use one branch per task:

- `feature/<short-name>`
- `fix/<short-name>`
- `docs/<short-name>`
- `test/<short-name>`

Examples: `feature/envelope-parser`, `fix/uart-length-check`, `docs/setup-windows`.

## Commit messages

Use a short imperative summary and an optional task ID:

- `feat(stm32): add envelope parser (TASK-012)`
- `fix(protocol): reject invalid message length`
- `docs: document local setup`
- `test(edge): cover viewer authorization`

Keep each commit focused. Do not include generated build outputs or local IDE state unless a file is intentionally required by the project.

## Pull Request checklist

- [ ] PR targets `develop` (unless it is a release PR to `main`).
- [ ] Scope and task ID are stated.
- [ ] Relevant tests/build checks were run, or the reason they could not be run is stated.
- [ ] Protocol/interface changes update the relevant contract and compatibility notes.
- [ ] No secrets, personal data, database files, or build artifacts are included.
- [ ] Failure behavior and safe-state behavior are documented where relevant.
- [ ] At least one teammate reviewed the change.

## Reviews and merges

- PR author must not approve their own PR.
- Request at least one review from another team member.
- Resolve review comments and ensure the target branch is up to date before merging.
- Prefer squash merge for feature branches to keep history readable.
- Never force-push `main` or `develop`.
- If a change affects another component, notify its owner and update shared contracts first.

## Security-sensitive changes

Changes to authentication, authorization, HMAC, key handling, anti-replay, policy enforcement, persistence of security state, or safe-state behavior require explicit review by another teammate. Tests must include rejection/failure cases, not only success cases.

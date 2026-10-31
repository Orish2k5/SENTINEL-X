# Command and Policy Lifecycle v1

Command baseline: `QUEUED -> PUBLISHED -> ACKNOWLEDGED -> COMPLETED`; exception outcomes include `REJECTED`, `TIMEOUT`, and `INDETERMINATE`.

`TIMEOUT` does not prove the physical action failed. Query state before any retry; do not blindly replay commands.

A policy is not `ACTIVE` just because the backend stored or published it. It becomes active only after STM32 validates and durably stores it and the required confirmation is received. Until then report a pending state such as `POLICY_PENDING`.

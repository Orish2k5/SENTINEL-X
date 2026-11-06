#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/tests/c/test_protocol_core"
cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/protocol/c" -I"$ROOT/firmware/stm32/Core/Inc" \
  "$ROOT/firmware/stm32/Core/Src/sx_crc16.c" \
  "$ROOT/firmware/stm32/Core/Src/sx_uart_frame.c" \
  "$ROOT/firmware/stm32/Core/Src/sx_envelope.c" \
  "$ROOT/firmware/stm32/Core/Src/sx_policy.c" \
  "$ROOT/tests/c/test_protocol_core.c" -o "$OUT"
"$OUT"

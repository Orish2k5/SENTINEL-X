#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/.build"
mkdir -p "${BUILD_DIR}"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"${REPO_ROOT}/protocol/c" \
  -I"${SCRIPT_DIR}/../Core/Inc" \
  "${SCRIPT_DIR}/../Core/Src/sx_uart_frame.c" \
  "${SCRIPT_DIR}/../Core/Src/sx_envelope.c" \
  "${SCRIPT_DIR}/../Core/Src/sx_replay_guard.c" \
  "${SCRIPT_DIR}/../Core/Src/sx_app_core.c" \
  "${SCRIPT_DIR}/../Core/Src/sx_ring_buffer.c" \
  "${SCRIPT_DIR}/test_stm32_core.c" \
  -o "${BUILD_DIR}/test_stm32_core"

"${BUILD_DIR}/test_stm32_core"

# Compile-check the HAL adapter against a small declaration-only HAL shim.
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only \
  -I"${SCRIPT_DIR}/mock_hal" \
  -I"${REPO_ROOT}/protocol/c" \
  -I"${SCRIPT_DIR}/../Core/Inc" \
  "${SCRIPT_DIR}/../Core/Src/sx_stm32_adapter.c"

# Syntax-check the Mbed TLS adapter against the API declarations; actual
# cryptographic correctness is separately checked with the published vector.
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only \
  -I"${SCRIPT_DIR}/mock_hal" \
  -I"${REPO_ROOT}/protocol/c" \
  -I"${SCRIPT_DIR}/../Core/Inc" \
  "${SCRIPT_DIR}/../Core/Src/sx_crypto_mbedtls.c" \
  "${SCRIPT_DIR}/../Core/Src/sx_platform_port_template.c"

python3 "${SCRIPT_DIR}/test_hmac_vector.py"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STM32_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${STM32_DIR}/../.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/.build"
mkdir -p "${BUILD_DIR}"

CORE="${STM32_DIR}/Core"
MOCK="${SCRIPT_DIR}/mock_hal"

PORTABLE_SOURCES=(
  "${CORE}/Src/sx_uart_frame.c"
  "${CORE}/Src/sx_envelope.c"
  "${CORE}/Src/sx_replay_guard.c"
  "${CORE}/Src/sx_app_core.c"
  "${CORE}/Src/sx_ring_buffer.c"
  "${CORE}/Src/sx_at24c256.c"
  "${CORE}/Src/sx_persistence_at24c256.c"
  "${CORE}/Src/sx_tb6612.c"
)

COMMON_FLAGS=(-std=c11 -Wall -Wextra -Werror -pedantic)
INCLUDES=("-I${CORE}/Inc" "-I${REPO_ROOT}/protocol/c")

cc "${COMMON_FLAGS[@]}" "${INCLUDES[@]}" \
  "${PORTABLE_SOURCES[@]}" \
  "${SCRIPT_DIR}/test_stm32_core.c" \
  -o "${BUILD_DIR}/test_stm32_core"
"${BUILD_DIR}/test_stm32_core"

cc "${COMMON_FLAGS[@]}" "${INCLUDES[@]}" \
  "${PORTABLE_SOURCES[@]}" \
  "${SCRIPT_DIR}/test_hardware_drivers.c" \
  -o "${BUILD_DIR}/test_hardware_drivers"
"${BUILD_DIR}/test_hardware_drivers"

# Syntax-only checks use declaration mocks. They do not emulate peripherals or crypto.
cc "${COMMON_FLAGS[@]}" -fsyntax-only \
  "-I${MOCK}" "${INCLUDES[@]}" \
  "${CORE}/Src/sx_stm32_adapter.c" \
  "${CORE}/Src/sx_at24c256_hal.c" \
  "${CORE}/Src/sx_platform_port_at24c256.c" \
  "${CORE}/Src/sx_crypto_mbedtls.c" \
  "${CORE}/Src/sx_platform_port_template.c"

python3 "${SCRIPT_DIR}/test_hmac_vector.py"
echo "All STM32 Core v0.2 host checks passed."

# Apply this STM32 core patch to SENTINEL-X

The patch targets the current `develop` version of `firmware/stm32/README.md` and adds the new STM32 source/docs/tests. The `protocol/c/sentinel_x_protocol_v1.h` and Envelope HMAC vector files already exist in the repository; copies included in this standalone archive are for running the host tests from the extracted archive and should not be treated as new protocol changes.

## Recommended Git workflow

From a local clone of `Orish2k5/SENTINEL-X`:

```bash
git checkout develop
git pull --ff-only origin develop
git switch -c feature/stm32-firmware-core
git apply --check SENTINEL-X_STM32_Core_v0.1.patch
git apply SENTINEL-X_STM32_Core_v0.1.patch
./firmware/stm32/tests/run_host_tests.sh
git status --short
git add firmware/stm32
git commit -m "feat(stm32): add secure UART and envelope core"
git push -u origin feature/stm32-firmware-core
```

Then open a Pull Request from `feature/stm32-firmware-core` into `develop`. Do not merge this into `main` until hardware integration and the acceptance tests are complete.

On Windows 11, use `firmware\stm32\tests\run_host_tests.ps1` from PowerShell with a native GCC (for example MinGW/MSYS2) and Python available on `PATH`. If you only have the ARM cross-compiler bundled with CubeIDE, use WSL or install a native host GCC for these executable tests.

## Patch constraints

- The patch does not include a generated CubeIDE `.ioc`, startup files or linker script.
- It does not implement an FRAM-specific driver because the FRAM part is not selected in the current Blueprint.
- It does not include any device HMAC key.
- The HAL adapter expects the generated project to provide `main.h`, `huart1` and `htim3`.
- Follow `firmware/stm32/docs/CUBEIDE_SETUP.md` and `firmware/stm32/docs/INTEGRATION_NOTES.md` before wiring hardware.

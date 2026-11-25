# Apply STM32 Core v0.2 to SENTINEL-X

## 1. Prepare a feature branch

Run in PowerShell at the repository root:

```powershell
git checkout develop
git pull --ff-only origin develop
git switch -c feature/stm32-at24c256-tb6612
```

Do not apply this package on `main`.

## 2. Extract this ZIP

Extract `SENTINEL-X_STM32_Core_v0.2.zip` to a temporary location. Keep `apply_to_repo.ps1` at the package root.

## 3. Copy files without replacing CubeMX-generated files

From PowerShell, run:

```powershell
.\apply_to_repo.ps1 -RepositoryPath "C:\path\to\SENTINEL-X"
```

The script copies only:

- `sx_*.h` into `firmware/stm32/Core/Inc/`;
- `sx_*.c` into `firmware/stm32/Core/Src/`;
- host test sources/scripts and `tests/mock_hal/` into `firmware/stm32/tests/`;
- documentation (including the validation record), the STM32 README and `.gitignore`.

It does **not** copy the package's convenience copies under `protocol/`, so the canonical repository files `protocol/c/sentinel_x_protocol_v1.h` and `protocol/envelope_v1/test-vectors/envelope_v1_hmac.json` are not overwritten. It also does not overwrite `main.c`, `main.h`, the `.ioc` file or CubeMX-generated HAL files.

`tests/mock_hal/` is for host compilation only. Do not add it to the STM32CubeIDE target include paths.

## 4. Run host tests

```powershell
.\firmware\stm32\tests\run_host_tests.ps1
```

Expected result in the tested package revision:

- STM32 core: 73 checks, 0 failures;
- EEPROM/persistence/TB6612: 111 checks, 0 failures;
- Python HMAC vector: PASS;
- syntax checks against declaration-only mocks: PASS.

## 5. Review and commit

```powershell
git diff --check
git status --short
git add firmware/stm32
git commit -m "feat(stm32): add AT24C256 persistence and TB6612 motor core"
git push -u origin feature/stm32-at24c256-tb6612
```

Open a Pull Request into `develop`. Do not merge into `main` until review and hardware validation are complete.

## Important integration gates

- The PC Edge change does not require a change to STM32 Core or EnvelopeV1.
- EEPROM blank provisioning is a factory/service operation, never a normal boot operation.
- Device-key provisioning and rate limiting intentionally deny commands until implemented.
- The current motor mode is forward-only. Reverse/brake and Modbus relay functionality are not implemented.
- Host tests are not a substitute for building against the real STM32CubeF4 HAL/Mbed TLS and testing the actual NUCLEO, EEPROM, TB6612 and motor.

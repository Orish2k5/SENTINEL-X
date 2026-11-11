$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir '../../../')).Path
$buildDir = Join-Path $scriptDir '.build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$protocolInclude = Join-Path $repoRoot 'protocol/c'
$coreInclude = Join-Path $scriptDir '../Core/Inc'
$mockInclude = Join-Path $scriptDir 'mock_hal'
$exePath = Join-Path $buildDir 'test_stm32_core.exe'

$portableSources = @(
    (Join-Path $scriptDir '../Core/Src/sx_uart_frame.c'),
    (Join-Path $scriptDir '../Core/Src/sx_envelope.c'),
    (Join-Path $scriptDir '../Core/Src/sx_replay_guard.c'),
    (Join-Path $scriptDir '../Core/Src/sx_app_core.c'),
    (Join-Path $scriptDir '../Core/Src/sx_ring_buffer.c'),
    (Join-Path $scriptDir 'test_stm32_core.c')
)

$gccArgs = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
             "-I$protocolInclude", "-I$coreInclude") + $portableSources + @('-o', $exePath)
& gcc @gccArgs
if ($LASTEXITCODE -ne 0) { throw "GCC test build failed: $LASTEXITCODE" }

& $exePath
if ($LASTEXITCODE -ne 0) { throw "STM32 core tests failed: $LASTEXITCODE" }

& gcc -std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only `
    "-I$mockInclude" "-I$protocolInclude" "-I$coreInclude" `
    (Join-Path $scriptDir '../Core/Src/sx_stm32_adapter.c')
if ($LASTEXITCODE -ne 0) { throw "STM32 HAL adapter syntax check failed: $LASTEXITCODE" }

& gcc -std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only `
    "-I$mockInclude" "-I$protocolInclude" "-I$coreInclude" `
    (Join-Path $scriptDir '../Core/Src/sx_crypto_mbedtls.c') `
    (Join-Path $scriptDir '../Core/Src/sx_platform_port_template.c')
if ($LASTEXITCODE -ne 0) { throw "Crypto/platform syntax check failed: $LASTEXITCODE" }

& python (Join-Path $scriptDir 'test_hmac_vector.py')
if ($LASTEXITCODE -ne 0) { throw "HMAC vector test failed: $LASTEXITCODE" }

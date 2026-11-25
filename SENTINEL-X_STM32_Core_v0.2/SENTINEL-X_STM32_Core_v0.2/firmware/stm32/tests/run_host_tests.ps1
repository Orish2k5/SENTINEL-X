$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$stm32Dir = (Resolve-Path (Join-Path $scriptDir '..')).Path
$repoRoot = (Resolve-Path (Join-Path $stm32Dir '../..')).Path
$buildDir = Join-Path $scriptDir '.build'
$core = Join-Path $stm32Dir 'Core'
$mock = Join-Path $scriptDir 'mock_hal'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$includes = @("-I$(Join-Path $core 'Inc')", "-I$(Join-Path $repoRoot 'protocol/c')")
$portableSources = @(
    (Join-Path $core 'Src/sx_uart_frame.c'),
    (Join-Path $core 'Src/sx_envelope.c'),
    (Join-Path $core 'Src/sx_replay_guard.c'),
    (Join-Path $core 'Src/sx_app_core.c'),
    (Join-Path $core 'Src/sx_ring_buffer.c'),
    (Join-Path $core 'Src/sx_at24c256.c'),
    (Join-Path $core 'Src/sx_persistence_at24c256.c'),
    (Join-Path $core 'Src/sx_tb6612.c')
)
$flags = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic')

$testCore = Join-Path $buildDir 'test_stm32_core.exe'
& gcc @flags @includes @portableSources (Join-Path $scriptDir 'test_stm32_core.c') -o $testCore
if ($LASTEXITCODE -ne 0) { throw "STM32 core host build failed: $LASTEXITCODE" }
& $testCore
if ($LASTEXITCODE -ne 0) { throw "STM32 core host tests failed: $LASTEXITCODE" }

$testDrivers = Join-Path $buildDir 'test_hardware_drivers.exe'
& gcc @flags @includes @portableSources (Join-Path $scriptDir 'test_hardware_drivers.c') -o $testDrivers
if ($LASTEXITCODE -ne 0) { throw "Hardware driver host build failed: $LASTEXITCODE" }
& $testDrivers
if ($LASTEXITCODE -ne 0) { throw "Hardware driver host tests failed: $LASTEXITCODE" }

$mockIncludes = @("-I$mock") + $includes
& gcc @flags -fsyntax-only @mockIncludes `
    (Join-Path $core 'Src/sx_stm32_adapter.c') `
    (Join-Path $core 'Src/sx_at24c256_hal.c') `
    (Join-Path $core 'Src/sx_platform_port_at24c256.c') `
    (Join-Path $core 'Src/sx_crypto_mbedtls.c') `
    (Join-Path $core 'Src/sx_platform_port_template.c')
if ($LASTEXITCODE -ne 0) { throw "HAL / Mbed TLS syntax checks failed: $LASTEXITCODE" }

& python (Join-Path $scriptDir 'test_hmac_vector.py')
if ($LASTEXITCODE -ne 0) { throw "HMAC vector test failed: $LASTEXITCODE" }

Write-Host 'All STM32 Core v0.2 host checks passed.' -ForegroundColor Green

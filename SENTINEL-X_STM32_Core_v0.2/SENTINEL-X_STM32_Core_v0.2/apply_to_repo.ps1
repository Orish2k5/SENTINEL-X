[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryPath
)

$ErrorActionPreference = 'Stop'

$packageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceStm32 = Join-Path $packageRoot 'firmware/stm32'
$repoPath = (Resolve-Path -LiteralPath $RepositoryPath).Path
$destinationStm32 = Join-Path $repoPath 'firmware/stm32'

$gitRoot = (& git -C $repoPath rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace(($gitRoot -join ''))) {
    throw "RepositoryPath is not inside a Git worktree: $repoPath"
}
$gitRoot = (Resolve-Path -LiteralPath ($gitRoot -join '')).Path
if ($gitRoot -ne $repoPath) {
    throw "Pass the repository root, not a subdirectory. Git root is: $gitRoot"
}

$branch = (& git -C $repoPath branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
    throw 'Could not determine the current Git branch.'
}
if ($branch -eq 'main') {
    throw 'Refusing to copy STM32 Core v0.2 onto main. Switch to a feature branch created from develop first.'
}
if ($branch -eq 'develop') {
    Write-Warning 'You are on develop. Recommended: create feature/stm32-at24c256-tb6612 before applying this package.'
}

$canonicalProtocolHeader = Join-Path $repoPath 'protocol/c/sentinel_x_protocol_v1.h'
if (-not (Test-Path -LiteralPath $canonicalProtocolHeader -PathType Leaf)) {
    throw "Canonical protocol header was not found: $canonicalProtocolHeader"
}

$destinationInc = Join-Path $destinationStm32 'Core/Inc'
$destinationSrc = Join-Path $destinationStm32 'Core/Src'
$destinationTests = Join-Path $destinationStm32 'tests'
$destinationDocs = Join-Path $destinationStm32 'docs'

New-Item -ItemType Directory -Force -Path $destinationInc, $destinationSrc, $destinationTests, $destinationDocs | Out-Null

# Copy only SENTINEL-X-owned source/header files. Do not overwrite CubeMX main.c/main.h/.ioc.
Get-ChildItem -LiteralPath (Join-Path $sourceStm32 'Core/Inc') -Filter 'sx_*.h' -File |
    Copy-Item -Destination $destinationInc -Force
Get-ChildItem -LiteralPath (Join-Path $sourceStm32 'Core/Src') -Filter 'sx_*.c' -File |
    Copy-Item -Destination $destinationSrc -Force

# Copy host tests and declaration mocks. These stay outside the CubeIDE target source folders.
$sourceTests = Join-Path $sourceStm32 'tests'
Get-ChildItem -LiteralPath $sourceTests -File |
    Where-Object { $_.Extension -in @('.c', '.py', '.ps1', '.sh') } |
    Copy-Item -Destination $destinationTests -Force
Copy-Item -LiteralPath (Join-Path $sourceTests 'mock_hal') `
    -Destination (Join-Path $destinationTests 'mock_hal') -Recurse -Force

# Copy documentation and the STM32-specific README.
Get-ChildItem -LiteralPath (Join-Path $sourceStm32 'docs') -File -Filter '*.md' |
    Copy-Item -Destination $destinationDocs -Force
Copy-Item -LiteralPath (Join-Path $sourceStm32 'README.md') -Destination (Join-Path $destinationStm32 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $packageRoot 'VALIDATION.md') -Destination (Join-Path $destinationDocs 'VALIDATION_v0.2.md') -Force
Copy-Item -LiteralPath (Join-Path $sourceStm32 '.gitignore') -Destination (Join-Path $destinationStm32 '.gitignore') -Force

Write-Host "STM32 Core v0.2 copied to: $destinationStm32" -ForegroundColor Green
Write-Host 'The repository protocol/c header was preserved; no CubeMX-generated files were overwritten.' -ForegroundColor Green
Write-Host 'Next: run .\firmware\stm32\tests\run_host_tests.ps1, inspect git diff, then build with STM32CubeIDE.' -ForegroundColor Yellow
& git -C $repoPath status --short -- firmware/stm32

# Bear2Wave automated smoke (CLI portion of tests/SMOKE_CHECKLIST.md).
# Usage (repo root):
#   powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1
#   powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1 -Configuration Release

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$SkipBuild,
    [switch]$IncludeLarge,
    # CI / headless: only TraceTools CLI tests (no wx Bear2Wave.exe required)
    [switch]$TraceToolsOnly
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$TraceDir = Join-Path $RepoRoot 'tests\traces'
$OutDir = Join-Path $RepoRoot "out\x64\$Configuration"
$BearExe = Join-Path $OutDir 'Bear2Wave.exe'
$TraceToolsVcx = Join-Path $RepoRoot 'tools\TraceTools.vcxproj'
$AppVcx = Join-Path $RepoRoot 'TEST1\TEST1.vcxproj'

Set-Location $RepoRoot
New-Item -ItemType Directory -Force -Path $TraceDir | Out-Null

$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild `
    -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msb) {
    Write-Error 'MSBuild not found. Install VS 2022 Build Tools or open Developer PowerShell.'
}

function Invoke-MsBuild([string]$Project, [string]$Cfg) {
    Write-Host ">> msbuild $([IO.Path]::GetFileName($Project)) ($Cfg|x64)"
    & $msb $Project /p:Configuration=$Cfg /p:Platform=x64 /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "MSBuild failed: $Project" }
}

if (-not $SkipBuild) {
    if (-not $TraceToolsOnly) {
        Invoke-MsBuild $AppVcx $Configuration
    }
    Invoke-MsBuild $TraceToolsVcx 'Debug'
}

if (-not $TraceToolsOnly -and -not (Test-Path $BearExe)) {
    Write-Error "Bear2Wave.exe not found: $BearExe"
}

$TraceTools = Join-Path $RepoRoot 'tools\x64\Debug\TraceTools.exe'
if (-not (Test-Path $TraceTools)) {
    Write-Error "TraceTools.exe not found: $TraceTools"
}

$failures = @()
function Test-Trace([string]$relPath, [string]$label) {
    $path = Join-Path $RepoRoot $relPath
    if (-not (Test-Path $path)) {
        Write-Warning "[skip] $label — missing $relPath"
        return
    }
    Write-Host "--- test $label ---"
    & $TraceTools test $path
    if ($LASTEXITCODE -ne 0) {
        $script:failures += $label
    }
}

Write-Host ''
Write-Host '=== Bear2Wave smoke (CLI) ==='
if ($TraceToolsOnly) {
    Write-Host 'Mode: TraceToolsOnly (no GUI exe required)'
} else {
    Write-Host "App: $BearExe"
}
Write-Host ''

Test-Trace 'tests\traces\bear2wave_sample.fst' 'sample FST'
Test-Trace 'tests\traces\bear2wave_sample.vcd' 'sample VCD'
Test-Trace 'tests\traces\test2.vcd' 'test2 VCD'

Write-Host '--- context-snapshot (E1-6) ---'
$goldenDir = Join-Path $RepoRoot 'tests\fixtures'
New-Item -ItemType Directory -Force -Path $goldenDir | Out-Null
$goldenHash = Join-Path $goldenDir 'context_snapshot.hash'
& $TraceTools context-snapshot (Join-Path $RepoRoot 'tests\traces\bear2wave_sample.vcd') $goldenHash
if ($LASTEXITCODE -ne 0) { $script:failures += 'context-snapshot' }

Write-Host '--- cancel-smoke (E1-8) ---'
$lazyPath = Join-Path $RepoRoot 'tests\traces\large_test.fst'
if (Test-Path $lazyPath) {
    & $TraceTools cancel-smoke $lazyPath 10
    if ($LASTEXITCODE -ne 0) { $script:failures += 'cancel-smoke' }
} else {
    & $TraceTools cancel-smoke (Join-Path $RepoRoot 'tests\traces\bear2wave_sample.fst') 5
    if ($LASTEXITCODE -ne 0) { $script:failures += 'cancel-smoke' }
}

if ($IncludeLarge) {
    Test-Trace 'tests\traces\large_test.fst' 'large FST (~100MB)'
    Test-Trace 'tests\traces\large_test.vcd' 'large VCD (~100MB)'
} else {
    Write-Host '[info] Skipping large trace loader tests (use -IncludeLarge).'
}

Write-Host ''
if ($failures.Count -eq 0) {
    Write-Host 'SMOKE CLI: PASS'
    Write-Host 'Manual UI checks: tests/SMOKE_CHECKLIST.md sections 1-5'
    exit 0
}

Write-Host "SMOKE CLI: FAIL ($($failures.Count))"
$failures | ForEach-Object { Write-Host "  - $_" }
exit 1

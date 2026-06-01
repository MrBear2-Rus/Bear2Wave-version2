# Bear2Wave full-flow automated test runner (CLI + static checks).
# GUI sections: follow tests/FULL_FLOW_TEST.md after this script completes.
#
# Usage (repo root):
#   powershell -ExecutionPolicy Bypass -File tools\run_full_flow.ps1
#   powershell -ExecutionPolicy Bypass -File tools\run_full_flow.ps1 -Configuration Release -IncludeLarge

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [switch]$IncludeLarge
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ReportDir = Join-Path $RepoRoot 'tests\output'
$ReportPath = Join-Path $ReportDir 'full_flow_report.txt'
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

$lines = @(
    "Bear2Wave Full-Flow Test Report",
    "Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "Configuration: $Configuration",
    "IncludeLarge: $IncludeLarge",
    ""
)

function Add-Result([string]$name, [int]$exitCode, [string]$note = '') {
    $status = if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' }
    $script:lines += "$status`t$name`texit=$exitCode`t$note"
    Write-Host "[$status] $name" -ForegroundColor $(if ($status -eq 'PASS') { 'Green' } else { 'Red' })
}

function Invoke-Phase([scriptblock]$Block) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $Block } finally { $ErrorActionPreference = $prev }
}

Set-Location $RepoRoot

Write-Host '=== Phase 0: release_check ===' -ForegroundColor Cyan
Invoke-Phase { & "$PSScriptRoot\release_check.ps1" }
Add-Result 'release_check' $LASTEXITCODE

Write-Host '=== Phase 1: run_smoke ===' -ForegroundColor Cyan
$smokeParams = @{ Configuration = $Configuration }
if ($SkipBuild) { $smokeParams.SkipBuild = $true }
if ($IncludeLarge) { $smokeParams.IncludeLarge = $true }
Invoke-Phase { & "$PSScriptRoot\run_smoke.ps1" @smokeParams }
Add-Result 'run_smoke' $LASTEXITCODE

Write-Host '=== Phase 2: run_trace_tests ===' -ForegroundColor Cyan
Invoke-Phase { & "$PSScriptRoot\run_trace_tests.ps1" -Configuration Debug }
Add-Result 'run_trace_tests' $LASTEXITCODE

$TT = Join-Path $RepoRoot 'tools\x64\Debug\TraceTools.exe'
$Tr = Join-Path $RepoRoot 'tests\traces'

if (Test-Path $TT) {
    Write-Host '=== Phase 3: extended TraceTools ===' -ForegroundColor Cyan
    foreach ($cmd in @('test-fp0','test-fp1','test-fp2','test-sst-filter','test-pattern-search','test-sim-log','test-pow10-snap')) {
        Invoke-Phase {
            if ($cmd -match 'sim-log|pow10') {
                & $TT $cmd | Out-Null
            } else {
                & $TT $cmd $Tr | Out-Null
            }
        }
        Add-Result $cmd $LASTEXITCODE
    }

    $outFst = Join-Path $ReportDir 'full_flow_vcd2fst.fst'
    Invoke-Phase { & $TT vcd2fst (Join-Path $Tr 'test2.vcd') $outFst | Out-Null }
    $vcd2fstRc = $LASTEXITCODE
    if (Test-Path $outFst) {
        Invoke-Phase { & $TT test $outFst | Out-Null }
        $roundRc = $LASTEXITCODE
        Add-Result 'vcd2fst+roundtrip' $(if ($roundRc -eq 0) { 0 } else { 1 }) "cli_exit=$vcd2fstRc"
    } else {
        Add-Result 'vcd2fst+roundtrip' 1 'no output file'
    }
}

$lines += ''
$lines += '--- Manual GUI (tests/FULL_FLOW_TEST.md) ---'
$lines += 'Pending: sections A-G in Bear2Wave.exe GUI'

$lines | Set-Content -Path $ReportPath -Encoding UTF8
Write-Host ''
Write-Host "Report: $ReportPath" -ForegroundColor Cyan

$failCount = ($lines | Where-Object { $_ -match '^FAIL' }).Count
if ($failCount -gt 0) {
    Write-Host "FULL-FLOW CLI: FAIL ($failCount failures)" -ForegroundColor Red
    exit 1
}
Write-Host 'FULL-FLOW CLI: PASS (complete GUI checklist manually)' -ForegroundColor Green
exit 0

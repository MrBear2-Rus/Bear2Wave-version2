# Build trace_tools, generate samples, run loader tests for all formats.
# Usage (repo root): powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$TraceDir = Join-Path $RepoRoot "tests\traces"
$Vcxproj = Join-Path $RepoRoot "tools\TraceTools.vcxproj"

Set-Location $RepoRoot
New-Item -ItemType Directory -Force -Path $TraceDir | Out-Null

$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msb) {
    Write-Error "MSBuild not found. Open a VS Developer shell or install VS Build Tools."
}

Write-Host "Building TraceTools (x64 Debug)..."
& $msb $Vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Exe = Join-Path $RepoRoot "tools\x64\Debug\TraceTools.exe"
if (-not (Test-Path $Exe)) {
    Write-Error "TraceTools.exe not found at $Exe"
}

Write-Host "Generating test traces in $TraceDir ..."
& $Exe gen-all $TraceDir
if ($LASTEXITCODE -ne 0) {
    Write-Warning "gen-all returned $LASTEXITCODE (GHW/VZT may be skipped if loader macros off)"
}

Write-Host "Running loader tests (per file)..."
$files = @(
    "bear2wave_sample.vcd",
    "bear2wave_sample.fst",
    "bear2wave_sample.vzt",
    "bear2wave_sample.lxt2",
    "bear2wave_gtkwave_basic.vcd",
    "bear2wave_gtkwave_basic.fst",
    "bear2wave_test2.vcd"
)
$testRc = 0
foreach ($f in $files) {
    $p = Join-Path $TraceDir $f
    if (-not (Test-Path $p)) { continue }
    Write-Host "--- test $f ---"
    & $Exe test $p
    if ($LASTEXITCODE -ne 0) { $testRc = 1 }
}
# GHW loader not implemented yet — optional manual check
$ghw = Join-Path $TraceDir "bear2wave_sample.ghw"
if (Test-Path $ghw) {
    Write-Host "--- test bear2wave_sample.ghw (expected FAIL until ghw_loader done) ---"
    & $Exe test $ghw
}

Write-Host ""
Write-Host "Samples in: $TraceDir"
Get-ChildItem $TraceDir -File | Format-Table Name, Length -AutoSize

exit $testRc

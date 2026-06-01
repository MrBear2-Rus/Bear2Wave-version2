# Pre-release sanity checks (E2-6). Run from repo root.
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$fail = 0
function Check($name, $ok) {
    if ($ok) { Write-Host "[OK] $name" -ForegroundColor Green }
    else { Write-Host "[FAIL] $name" -ForegroundColor Red; $script:fail = 1 }
}

Check "TEST1.vcxproj in repo" (Test-Path "TEST1/TEST1.vcxproj")
Check "TraceTools.vcxproj in repo" (Test-Path "tools/TraceTools.vcxproj")
Check "vcpkg.json" (Test-Path "vcpkg.json")
Check "CI workflow" (Test-Path ".github/workflows/windows-ci.yml")
Check "LICENSE" (Test-Path "LICENSE")
Check "README.md" (Test-Path "README.md")
Check "VERSION.txt" (Test-Path "VERSION.txt")
Check "CHANGELOG.md" (Test-Path "CHANGELOG.md")
Check "CONTRIBUTING.md" (Test-Path "CONTRIBUTING.md")
Check "Bear2Wave.Build.props" (Test-Path "TEST1/Bear2Wave.Build.props")
Check "Bear2WaveWx.props.example" (Test-Path "TEST1/Bear2WaveWx.props.example")
Check "package_release.ps1" (Test-Path "tools/package_release.ps1")
Check "run_smoke.ps1" (Test-Path "tools/run_smoke.ps1")
Check "setup_local_props.ps1" (Test-Path "tools/setup_local_props.ps1")
Check "context_snapshot golden" (Test-Path "tests/fixtures/context_snapshot.hash")
Check "SMOKE_CHECKLIST.md" (Test-Path "tests/SMOKE_CHECKLIST.md")
Check "docs/QUICKSTART.md" (Test-Path "docs/QUICKSTART.md")
Check "docs/WEEK3_CLOSEOUT.md" (Test-Path "docs/WEEK3_CLOSEOUT.md")
Check "docs/images/README.md" (Test-Path "docs/images/README.md")
Check "docs/WX_RELEASE_BUILD.md" (Test-Path "docs/WX_RELEASE_BUILD.md")
Check "windows-ci-nightly.yml" (Test-Path ".github/workflows/windows-ci-nightly.yml")
Check "windows-ci-release.yml" (Test-Path ".github/workflows/windows-ci-release.yml")

$propsWx = Test-Path "TEST1/Bear2WaveWx.props"
$propsFmt = Test-Path "TEST1/Bear2WaveTraceFormats.props"
if ($propsWx -and $propsFmt) {
    Write-Host "[OK] local props (Bear2WaveWx + TraceFormats)" -ForegroundColor Green
} else {
    Write-Host "[WARN] run tools\setup_local_props.ps1 before Release build" -ForegroundColor Yellow
}

if ($propsWx) {
    $wxRoot = (Select-String -Path "TEST1/Bear2WaveWx.props" -Pattern '<WxWidgetsRoot>([^<]+)</WxWidgetsRoot>').Matches[0].Groups[1].Value.Trim()
    if (Test-Path "$wxRoot\lib\vc_x64_lib\mswu\wx\setup.h") {
        Write-Host "[OK] wx mswu (Release /MD libs)" -ForegroundColor Green
    } else {
        Write-Host "[WARN] wx mswu missing — Release will fall back to mswud (see docs/WX_RELEASE_BUILD.md)" -ForegroundColor Yellow
    }
}

if ($fail -ne 0) { exit 1 }
Write-Host "release_check: all static checks passed."

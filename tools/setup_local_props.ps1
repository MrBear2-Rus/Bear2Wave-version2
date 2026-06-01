# Create local MSBuild props from examples (E0-3). Run from repo root.
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Test1 = Join-Path $RepoRoot "TEST1"

function Find-VcpkgInstalled {
    $candidates = @()
    if ($env:VCPKG_ROOT) { $candidates += Join-Path $env:VCPKG_ROOT "installed\x64-windows" }
    $candidates += @(
        "E:\download\vcpkg-master\installed\x64-windows",
        "E:\vcpkg-master\installed\x64-windows",
        "C:\vcpkg\installed\x64-windows",
        (Join-Path $RepoRoot "vcpkg_installed\x64-windows")
    )
    foreach ($root in $candidates) {
        if ($root -and (Test-Path (Join-Path $root "include\zlib.h"))) { return $root }
    }
    return $null
}

function Ensure-Copy($example, $target, $hint) {
    $ex = Join-Path $Test1 $example
    $tg = Join-Path $Test1 $target
    if (Test-Path $tg) {
        Write-Host "exists: $target"
        return
    }
    if (-not (Test-Path $ex)) {
        Write-Error "Missing $example"
    }
    Copy-Item $ex $tg
    Write-Host "created: $target ($hint)"
}

Ensure-Copy "Bear2WaveTraceFormats.props.example" "Bear2WaveTraceFormats.props" "edit VcpkgRoot if needed"
Ensure-Copy "Bear2WaveWx.props.example" "Bear2WaveWx.props" "set WxWidgetsRoot or env WXWIN"

$vcpkg = Find-VcpkgInstalled
if ($vcpkg) {
    $traceProps = Join-Path $Test1 "Bear2WaveTraceFormats.props"
    if (Test-Path $traceProps) {
        $lines = Get-Content $traceProps
        $changed = $false
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^\s*<VcpkgRoot Condition="''\$\(VcpkgRoot\)''==''''">') {
                $newLine = "    <VcpkgRoot Condition=`"'`$(VcpkgRoot)'`==''`">$vcpkg</VcpkgRoot>"
                if ($lines[$i] -ne $newLine) {
                    $lines[$i] = $newLine
                    $changed = $true
                }
            }
        }
        if ($changed) {
            Set-Content -Path $traceProps -Value $lines -Encoding UTF8
            Write-Host "VcpkgRoot -> $vcpkg"
        }
    }
} else {
    Write-Warning "zlib.h not found. Run: vcpkg install zlib:x64-windows bzip2:x64-windows liblzma:x64-windows"
}

if ($env:WXWIN -and (Test-Path $env:WXWIN)) {
    Write-Host "WXWIN=$env:WXWIN"
}
if ($env:VCPKG_ROOT) {
    Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
}
Write-Host "Next: msbuild TEST1\TEST1.vcxproj /p:Configuration=Release /p:Platform=x64"

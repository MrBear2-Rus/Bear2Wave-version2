# Fetch GTKWave libvzt + liblxt2 into third_party/gtkwave for Bear2Wave.
# Usage (from repo root TEST1 - 1/TEST1):
#   powershell -ExecutionPolicy Bypass -File tools\fetch_gtkwave_libs.ps1

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$DestVzt  = Join-Path $RepoRoot "third_party\gtkwave\libvzt"
# LXT2 reader lives in gtkwave lib/liblxt (not liblxt2)
$DestLxt  = Join-Path $RepoRoot "third_party\gtkwave\liblxt"
$DestGhw  = Join-Path $RepoRoot "third_party\gtkwave\libghw"
$CloneDir = Join-Path $RepoRoot "third_party\gtkwave-src"
$RepoUrl  = "https://github.com/gtkwave/gtkwave.git"

function Copy-LibDir {
    param([string]$SrcSub, [string]$Dest)
    $src = Join-Path $CloneDir $SrcSub
    if (-not (Test-Path $src)) {
        Write-Warning "Source not found: $src (GTKWave layout may have changed)"
        return $false
    }
    if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
    New-Item -ItemType Directory -Force -Path (Split-Path $Dest) | Out-Null
    Copy-Item -Recurse -Force $src $Dest
    Write-Host "Copied $SrcSub -> $Dest"
    return $true
}

Write-Host "Repo root: $RepoRoot"

if (-not (Test-Path $CloneDir)) {
    Write-Host "Cloning GTKWave (shallow)..."
    git clone --depth 1 $RepoUrl $CloneDir
} else {
    Write-Host "Updating $CloneDir ..."
    Push-Location $CloneDir
    git pull --ff-only
    Pop-Location
}

# GTKWave layout: lib/libvzt, lib/liblxt (contains lxt2_read.c)
$okVzt = Copy-LibDir "lib\libvzt" $DestVzt
$okLxt = Copy-LibDir "lib\liblxt" $DestLxt
$okGhw = Copy-LibDir "lib\libghw" $DestGhw

if (-not $okVzt -and -not $okLxt -and -not $okGhw) {
    Write-Error "Could not find libvzt/liblxt under $CloneDir. Check gtkwave tree layout."
}

# Ensure MSVC config.h stub exists (libvzt/lxt2_read.c include <config.h>)
$ConfigStub = Join-Path $RepoRoot "third_party\gtkwave\config.h"
if (-not (Test-Path $ConfigStub)) {
    @'
#ifndef BEAR2WAVE_GTKWAVE_CONFIG_H
#define BEAR2WAVE_GTKWAVE_CONFIG_H
#define STDC_HEADERS 1
#define HAVE_INTTYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_FCNTL_H 1
#endif
'@ | Set-Content -Encoding UTF8 $ConfigStub
    Write-Host "Wrote $ConfigStub"
}

Write-Host ""
Write-Host "Done. Next steps:"
Write-Host "  1. vcpkg install zlib:x64-windows bzip2:x64-windows liblzma:x64-windows"
Write-Host "  2. Copy TEST1\Bear2WaveTraceFormats.props.example -> Bear2WaveTraceFormats.props"
Write-Host "  3. Import props in Visual Studio (x64) or add BEAR2WAVE_WITH_VZT / LXT2 to vcxproj"
Write-Host "  4. Rebuild TEST1 and open a .vzt or .lxt2 file"

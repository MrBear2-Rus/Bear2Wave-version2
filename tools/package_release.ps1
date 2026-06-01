# E2-3: Build Release x64 and pack dist/Bear2Wave-<version>-win64.zip
param(
    # Default Debug: many wx installs only have mswud static libs. Use -Configuration Release when mswu exists.
    [string]$Configuration = "Debug",
    [switch]$SkipBuild
)

function Get-WxWidgetsRootFromProps {
    $props = Join-Path $RepoRoot "TEST1\Bear2WaveWx.props"
    if (-not (Test-Path $props)) { return $null }
    $m = Select-String -Path $props -Pattern '<WxWidgetsRoot>([^<]+)</WxWidgetsRoot>' | Select-Object -First 1
    if ($m) { return $m.Matches.Groups[1].Value.Trim() }
    return $env:WXWIN
}

function Test-WxMswuReleaseLibs {
    param([string]$WxRoot)
    if (-not $WxRoot) { return $false }
    return Test-Path (Join-Path $WxRoot "lib\vc_x64_lib\mswu\wx\setup.h")
}
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$version = (Get-Content "VERSION.txt" -Raw).Trim()
if (-not $version) { $version = "0.0.0" }

& "$PSScriptRoot\setup_local_props.ps1"

$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msb) { throw "MSBuild not found" }

$vcx = Join-Path $RepoRoot "TEST1\TEST1.vcxproj"
if (-not $SkipBuild) {
    Write-Host "Building Bear2Wave $Configuration x64..."
    & $msb $vcx /p:Configuration=$Configuration /p:Platform=x64 /m /v:minimal /p:Bear2WaveVersion=$version
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$outDir = Join-Path $RepoRoot "out\x64\$Configuration"
$exe = Join-Path $outDir "Bear2Wave.exe"
if (-not (Test-Path $exe)) {
    throw "Missing $exe — build failed or wrong output path"
}

$distRoot = Join-Path $RepoRoot "dist\Bear2Wave-$version-win64"
if (Test-Path $distRoot) { Remove-Item $distRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null

Copy-Item $exe $distRoot
foreach ($dll in @("bz2.dll", "zlib1.dll")) {
    $src = Join-Path $outDir $dll
    if (Test-Path $src) { Copy-Item $src $distRoot }
    else {
        $props = Join-Path $RepoRoot "TEST1\Bear2WaveTraceFormats.props"
        if (Test-Path $props) {
            if ($dll -match "bz2" -and (Select-String -Path $props -Pattern "VcpkgRoot" -Quiet)) {
                $m = Select-String -Path $props -Pattern '<VcpkgRoot>([^<]+)</VcpkgRoot>' | Select-Object -First 1
                if ($m) {
                    $vcpkgBin = Join-Path $m.Matches.Groups[1].Value.Trim() "bin"
                    $cand = Join-Path $vcpkgBin "bz2.dll"
                    if (Test-Path $cand) { Copy-Item $cand $distRoot }
                }
            }
        }
    }
}

@("README.md", "LICENSE", "CHANGELOG.md", "VERSION.txt") | ForEach-Object {
    $p = Join-Path $RepoRoot $_
    if (Test-Path $p) { Copy-Item $p $distRoot }
}
$helpDir = Join-Path $distRoot "docs\help"
$docsDir = Join-Path $distRoot "docs"
New-Item -ItemType Directory -Force -Path $helpDir | Out-Null
New-Item -ItemType Directory -Force -Path $docsDir | Out-Null
$srcHelp = Join-Path $RepoRoot "docs\help"
if (Test-Path $srcHelp) {
    Copy-Item (Join-Path $srcHelp "*.html") $helpDir -Force
}
@(
    "USER_GUIDE.md", "QUICKSTART.md", "SHORTCUTS.md", "ENVIRONMENT.md",
    "LARGE_TRACE_PERFORMANCE.md", "AI_USAGE.md", "WX_RELEASE_BUILD.md", "WEEK3_CLOSEOUT.md"
) | ForEach-Object {
    $src = Join-Path $RepoRoot "docs\$_"
    if (Test-Path $src) { Copy-Item $src (Join-Path $distRoot "docs") -Force }
}
$imgReadme = Join-Path $RepoRoot "docs\images\README.md"
if (Test-Path $imgReadme) {
    $imgDst = Join-Path $distRoot "docs\images"
    New-Item -ItemType Directory -Force -Path $imgDst | Out-Null
    Copy-Item $imgReadme $imgDst -Force
}
$sampleDir = Join-Path $distRoot "samples"
New-Item -ItemType Directory -Force -Path $sampleDir | Out-Null
$tr = Join-Path $RepoRoot "tests\traces"
if (Test-Path $tr) {
    foreach ($pat in @("bear2wave_sample.fst", "bear2wave_sample.vcd", "test2.vcd")) {
        Get-ChildItem $tr -Filter $pat -ErrorAction SilentlyContinue | Copy-Item -Destination $sampleDir
    }
}

$zipPath = Join-Path $RepoRoot "dist\Bear2Wave-$version-win64.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $distRoot "*") -DestinationPath $zipPath -Force

$wxRoot = Get-WxWidgetsRootFromProps
$hasMswu = Test-WxMswuReleaseLibs $wxRoot
if ($Configuration -eq "Release" -and -not $hasMswu) {
    Write-Host ""
    Write-Host "[WARN] Release package built but wx mswu not found — exe may link mswud (debug CRT)." -ForegroundColor Yellow
    Write-Host "       See docs/WX_RELEASE_BUILD.md" -ForegroundColor Yellow
} elseif ($Configuration -eq "Release" -and $hasMswu) {
    Write-Host ""
    Write-Host "[OK] wx mswu detected — Release uses /MD wx libs." -ForegroundColor Green
} elseif ($Configuration -eq "Debug") {
    Write-Host ""
    Write-Host "[INFO] Debug package (default). For public Alpha Release: build mswu then -Configuration Release." -ForegroundColor Cyan
}

Write-Host ""
Write-Host "Package: $zipPath"
Write-Host "Folder:  $distRoot"
Get-ChildItem $distRoot | Format-Table Name, Length -AutoSize

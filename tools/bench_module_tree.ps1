# W3-11: Generate deep module-tree VCD and print size hint for manual open test.
param(
    [int]$Modules = 2000,
    [string]$OutPath = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$py = Join-Path $RepoRoot "tools\gen_module_tree_vcd.py"
if (-not $OutPath) {
    $OutPath = Join-Path $RepoRoot "tests\traces\module_tree_bench.vcd"
}

Set-Location $RepoRoot
python $py --modules $Modules --output $OutPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$item = Get-Item $OutPath
Write-Host ""
Write-Host "Module-tree bench VCD: $OutPath"
Write-Host ("Size: {0:N0} bytes" -f $item.Length)
Write-Host "Manual: open in Bear2Wave, expand module tree, note time-to-first-expand."

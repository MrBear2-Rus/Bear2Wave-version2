# Mock transaction filter decoder for Bear2Wave FP-2 tests.
# Reads minimal VCD from stdin; emits two synthetic decode traces.

$stdinText = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($stdinText)) { exit 1 }
if ($stdinText -notmatch '\$enddefinitions') { exit 1 }

$dataChanges = @()
foreach ($line in ($stdinText -split "`n")) {
    $t = $line.Trim()
    if ($t -match '^#(\d+)$') { $curT = [uint64]$Matches[1]; continue }
    if ($null -eq $curT) { continue }
    if ($t -match '^b([01]+)\s+b2w\d+$') {
        $bits = $Matches[1]
        if ($bits.Length -ge 1) {
            $dataChanges += [pscustomobject]@{ Time = $curT; Bits = $bits }
        }
    }
    if ($t -match '^([01])\s+b2w\d+$') {
        $dataChanges += [pscustomobject]@{ Time = $curT; Bits = $Matches[1] }
    }
}

Write-Output '$name txn.data_nonzero'
$emitted = $false
foreach ($ch in $dataChanges) {
    if ($ch.Bits -match '1') {
        Write-Output ('#' + $ch.Time + ' 1')
        $emitted = $true
    } else {
        Write-Output ('#' + $ch.Time + ' 0')
    }
}
if (-not $emitted) {
    Write-Output '#0 0'
}

Write-Output '$name txn.last_data'
if ($dataChanges.Count -gt 0) {
    $last = $dataChanges[-1]
    Write-Output ('#' + $last.Time + ' DEC:' + $last.Bits)
    Write-Output ('M ' + $last.Time + ' txn_peak')
} else {
    Write-Output '#0 idle'
}

exit 0

param(
    [string]$Root = "",
    [int]$ExpectedRiskCount = 21,
    [int]$ExpectedClosedCount = 18,
    [string[]]$ExpectedOpenRiskIds = @(
        "RISK-ASYNC-001",
        "RISK-CALL-001",
        "RISK-LOOP-002"
    )
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$cgenruleDir = Join-Path $Root "cgenrule"
$failures = New-Object System.Collections.Generic.List[string]
$riskEntries = @{}
$closedMarker = ([char]0x5DF2) + ([char]0x6536) + ([char]0x53E3)

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

if ( ! (Test-Path -LiteralPath $cgenruleDir -PathType Container) ) {
    Write-Host "cgenrule docs directory missing: $cgenruleDir"
    exit 1
}

$docs = @(Get-ChildItem -LiteralPath $cgenruleDir -Filter "*.md" -File | Sort-Object Name)
foreach ( $doc in $docs ) {
    $lines = (Read-Utf8Text $doc.FullName) -split "`r?`n"
    for ( $i = 0; $i -lt $lines.Count; $i++ ) {
        $line = $lines[$i]
        $match = [regex]::Match($line, '^\|\s*(RISK-[A-Z]+-[0-9]+)\s*\|')
        if ( ! $match.Success ) {
            continue
        }

        $id = $match.Groups[1].Value
        if ( ! $riskEntries.ContainsKey($id) ) {
            $riskEntries[$id] = [pscustomobject]@{
                Id = $id
                Closed = $false
                Locations = New-Object System.Collections.Generic.List[string]
            }
        }

        $entry = $riskEntries[$id]
        $entry.Locations.Add(("{0}:{1}" -f $doc.Name, ($i + 1))) | Out-Null
        if ( $line.Contains($closedMarker) ) {
            $entry.Closed = $true
        }
    }
}

$ids = @($riskEntries.Keys | Sort-Object)
if ( $ids.Count -ne $ExpectedRiskCount ) {
    Add-Failure "risk ledger count mismatch: expected $ExpectedRiskCount, actual $($ids.Count)"
}

$expectedOpen = New-Object System.Collections.Generic.HashSet[string]
foreach ( $id in $ExpectedOpenRiskIds ) {
    $expectedOpen.Add($id) | Out-Null
}

$actualOpen = New-Object System.Collections.Generic.List[string]
$actualClosed = New-Object System.Collections.Generic.List[string]

foreach ( $id in $ids ) {
    $entry = $riskEntries[$id]
    if ( $entry.Closed ) {
        $actualClosed.Add($id) | Out-Null
    } else {
        $actualOpen.Add($id) | Out-Null
    }
}

if ( $actualClosed.Count -ne $ExpectedClosedCount ) {
    Add-Failure "closed risk count mismatch: expected $ExpectedClosedCount, actual $($actualClosed.Count)"
}

if ( $actualOpen.Count -ne $ExpectedOpenRiskIds.Count ) {
    Add-Failure "open risk count mismatch: expected $($ExpectedOpenRiskIds.Count), actual $($actualOpen.Count)"
}

foreach ( $id in $ExpectedOpenRiskIds ) {
    if ( ! $riskEntries.ContainsKey($id) ) {
        Add-Failure "expected open risk is missing: $id"
        continue
    }
    if ( $riskEntries[$id].Closed ) {
        Add-Failure "expected open risk is marked closed: $id"
    }
}

foreach ( $id in $actualOpen ) {
    if ( ! $expectedOpen.Contains($id) ) {
        $locations = [string]::Join(", ", $riskEntries[$id].Locations.ToArray())
        Add-Failure "unexpected open risk: $id at $locations"
    }
}

foreach ( $id in $ids ) {
    if ( $expectedOpen.Contains($id) ) {
        continue
    }
    if ( ! $riskEntries[$id].Closed ) {
        $locations = [string]::Join(", ", $riskEntries[$id].Locations.ToArray())
        Add-Failure "risk is neither expected-open nor marked closed: $id at $locations"
    }
}

if ( $failures.Count -ne 0 ) {
    Write-Host "Risk ledger check failed."
    foreach ( $failure in ($failures | Select-Object -First 80) ) {
        Write-Host "  $failure"
    }
    if ( $failures.Count -gt 80 ) {
        Write-Host "  ... $($failures.Count - 80) more failure(s)"
    }
    exit 1
}

Write-Host "Risk ledger ok: $($ids.Count) unique risks, $($actualClosed.Count) closed, $($actualOpen.Count) open."
Write-Host "Open risks: $([string]::Join(', ', $actualOpen.ToArray()))"
exit 0

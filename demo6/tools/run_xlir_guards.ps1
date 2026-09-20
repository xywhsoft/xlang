param(
    [ValidateSet("FullRegressionSupplement", "Rc0Supplement")]
    [string]$Mode = "FullRegressionSupplement",
    [string]$Root = "",
    [string]$StartAt = "",
    [string]$ReportPath = "",
    [switch]$ListOnly
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$toolsDir = Join-Path $Root "tools"
$wiringFiles = New-Object System.Collections.Generic.List[string]

if ( $Mode -eq "FullRegressionSupplement" ) {
    $wiringFiles.Add((Join-Path $Root "test.bat")) | Out-Null
    $suiteDir = Join-Path $Root "test\suites"
    foreach ( $file in @(Get-ChildItem -LiteralPath $suiteDir -Filter "*.bat" -File | Sort-Object Name) ) {
        $wiringFiles.Add($file.FullName) | Out-Null
    }
}
else {
    $wiringFiles.Add((Join-Path $toolsDir "rc0_guard.ps1")) | Out-Null
}

$explicit = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
foreach ( $file in $wiringFiles ) {
    if ( ! (Test-Path -LiteralPath $file -PathType Leaf) ) {
        throw "XLIR guard wiring source is missing: $file"
    }

    $text = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
    foreach ( $match in [regex]::Matches($text, 'check_xlir_[A-Za-z0-9_]+\.ps1') ) {
        $explicit.Add($match.Value) | Out-Null
    }
}

$allGuards = @(Get-ChildItem -LiteralPath $toolsDir -Filter "check_xlir_*.ps1" -File | Sort-Object Name)
$pending = @($allGuards | Where-Object { ! $explicit.Contains($_.Name) })
$selected = $pending

if ( $StartAt -ne "" ) {
    $startIndex = -1
    for ( $index = 0; $index -lt $pending.Count; $index++ ) {
        if ( $pending[$index].Name.Equals($StartAt, [System.StringComparison]::OrdinalIgnoreCase) ) {
            $startIndex = $index
            break
        }
    }
    if ( $startIndex -lt 0 ) {
        throw "XLIR supplemental start gate is not pending in ${Mode}: $StartAt"
    }
    $selected = @($pending | Select-Object -Skip $startIndex)
}

Write-Host "XLIR guard inventory: $($allGuards.Count) total, $($explicit.Count) explicit, $($pending.Count) supplemental, $($selected.Count) selected ($Mode)."

if ( $ListOnly ) {
    foreach ( $guard in $selected ) {
        Write-Host $guard.Name
    }
    exit 0
}

$runReport = [ordered]@{
    Mode = $Mode
    StartAt = $StartAt
    StartedUtc = [DateTime]::UtcNow.ToString("o")
    CompletedUtc = $null
    TotalGuards = $allGuards.Count
    ExplicitGuards = $explicit.Count
    SupplementalGuards = $pending.Count
    SelectedGuards = $selected.Count
    Status = "running"
    Results = @()
}
if ( $ReportPath -eq "" ) {
    $ReportPath = Join-Path $Root "build\tmp\xlir_guards.$Mode.results.json"
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)
[System.IO.Directory]::CreateDirectory(
    [System.IO.Path]::GetDirectoryName($ReportPath)) | Out-Null
function Save-RunReport {
    [System.IO.File]::WriteAllText(
        $ReportPath, ($runReport | ConvertTo-Json -Depth 6),
        [System.Text.UTF8Encoding]::new($false))
}
Save-RunReport

$previousLocation = Get-Location
try {
    Set-Location $Root
    for ( $index = 0; $index -lt $selected.Count; $index++ ) {
        $guard = $selected[$index]
        Write-Host "[$($index + 1)/$($selected.Count)] $($guard.Name)"
        $result = [pscustomobject]@{
            Guard = $guard.Name
            ScriptSha256 = (Get-FileHash -LiteralPath $guard.FullName -Algorithm SHA256).Hash
            StartedUtc = [DateTime]::UtcNow.ToString("o")
            CompletedUtc = $null
            Status = "running"
            ExitCode = $null
        }
        $runReport.Results += $result
        Save-RunReport
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $guard.FullName
        $result.ExitCode = $LASTEXITCODE
        $result.CompletedUtc = [DateTime]::UtcNow.ToString("o")
        $result.Status = if ( $result.ExitCode -eq 0 ) { "passed" } else { "failed" }
        Save-RunReport
        if ( $result.ExitCode -ne 0 ) {
            throw "XLIR guard failed with code $($result.ExitCode): $($guard.Name)"
        }
    }
    $runReport.Status = "passed"
}
catch {
    $runReport.Status = "failed"
    throw
}
finally {
    $runReport.CompletedUtc = [DateTime]::UtcNow.ToString("o")
    Save-RunReport
    Set-Location $previousLocation
}

Write-Host "XLIR supplemental guards ok: $($selected.Count) scripts ($Mode)."
exit 0

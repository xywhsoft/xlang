param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\xlir_foreign_effect_contract.xl"
$invalidFixture = Join-Path $Root "test\cases\invalid_extern_effect_contract.xl"
$outputDir = Join-Path $Root "build\test\foreign_effect_contract"
$irPath = Join-Path $outputDir "contract.lowered.xlir"
$cPath = Join-Path $outputDir "contract.c"
$invalidOut = Join-Path $outputDir "invalid.out.txt"
$invalidErr = Join-Path $outputDir "invalid.err.txt"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "foreign effect contract check requires release/xl.exe"
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $fixture --lowered --emit $irPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "lowered XLIR generation failed for foreign effect contract fixture"
}
& $xl cgen $fixture --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "C generation failed for foreign effect contract fixture"
}

$ir = [System.IO.File]::ReadAllText($irPath, [System.Text.Encoding]::UTF8)
$generatedC = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$sinTarget = [regex]::Match($ir, '(?m)^\s*call-target .*\bcname=sin\b.*$').Value
$fabsTarget = [regex]::Match($ir, '(?m)^\s*call-target .*\bcname=fabs\b.*$').Value
$cosTarget = [regex]::Match($ir, '(?m)^\s*call-target .*\bcname=cos\b.*$').Value

if ( $sinTarget -eq "" -or
     $sinTarget -notmatch '\bexecution=foreign-nonblocking\b' -or
     $sinTarget -notmatch '\beffect=conservative\b' -or
     $sinTarget -notmatch '\berrors=no-raise/none\b' ) {
    throw "math.sin did not retain its no-raise + nonblocking contract in lowered XLIR: $sinTarget"
}
if ( $fabsTarget -eq "" -or
     $fabsTarget -notmatch '\bexecution=foreign-nonblocking\b' -or
     $fabsTarget -notmatch '\beffect=pure\b' -or
     $fabsTarget -notmatch '\berrors=no-raise/none\b' ) {
    throw "math.abs(float) did not retain pure + no-raise + nonblocking in lowered XLIR: $fabsTarget"
}
if ( $cosTarget -eq "" -or
     $cosTarget -notmatch '\bexecution=foreign-may-block\b' -or
     $cosTarget -notmatch '\beffect=conservative\b' -or
     $cosTarget -notmatch '\berrors=may-raise/foreign-scope\b' ) {
    throw "unannotated extern did not retain the conservative lowered XLIR contract: $cosTarget"
}
if ( $generatedC -notmatch '(?m)^extern double sin\(double\);$' -or
     $generatedC -notmatch '=\s*sin\(' -or
     $generatedC -notmatch '(?m)^extern double fabs\(double\);$' -or
     $generatedC -notmatch '=\s*fabs\(' ) {
    throw "math.sin did not lower to a direct native C declaration and call"
}
$foreignScopeCount = ([regex]::Matches($generatedC, '\bxl6ForeignCallBegin\s*\(&')).Count
if ( $foreignScopeCount -ne 1 ) {
    throw "expected exactly one conservative foreign error scope, found $foreignScopeCount"
}

$process = Start-Process -FilePath $xl -ArgumentList @('check', $invalidFixture) `
    -NoNewWindow -PassThru -Wait -RedirectStandardOutput $invalidOut `
    -RedirectStandardError $invalidErr
if ( $process.ExitCode -eq 0 ) {
    throw "unknown extern effect contract should be rejected"
}
$diagnostic = [System.IO.File]::ReadAllText($invalidErr, [System.Text.Encoding]::UTF8)
if ( $diagnostic -notmatch 'unknown extern effect contract' ) {
    throw "unknown extern effect contract diagnostic is missing"
}

Write-Host "XLIR foreign effect contract check passed."

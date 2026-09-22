param([string]$Root = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path

$cgenruleDirectory = Join-Path $Root 'cgenrule'
$matrixName = ([char]0x6587) + ([char]0x6863) + ([char]0x72B6) +
    ([char]0x6001) + ([char]0x77E9) + ([char]0x9635) + '.md'
$canonicalDocuments = [System.Collections.Generic.List[string]]::new()
@(
    'cgenrule/README.md',
    'docs/README.md',
    ('docs/' + $matrixName)
) | ForEach-Object { $canonicalDocuments.Add($_) }
$xlirDocuments = @(Get-ChildItem -LiteralPath (Join-Path $Root 'docs') -Filter 'XLIR*.md' -File)
if ($xlirDocuments.Count -lt 2) {
    Write-Host 'current XLIR design and acceptance documents are missing'
    exit 1
}
foreach ($document in $xlirDocuments) {
    $canonicalDocuments.Add('docs/' + $document.Name)
}
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$Message) {
    $script:failures.Add($Message)
}

function Read-Utf8([string]$Path) {
    return [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
}

if (!(Test-Path -LiteralPath $cgenruleDirectory -PathType Container)) {
    Write-Host "cgenrule directory is missing: $cgenruleDirectory"
    exit 1
}

# The RISK-* ledger described implementation gaps in the retired AST Cgen.
# Its contents are Git history now, not release truth for the XLIR emitter. Keep
# this script name because it is a stable RC/static-smoke entry point, but make
# it prevent the obsolete ledger and its numbered document set from returning.
$ruleDocuments = @(Get-ChildItem -LiteralPath $cgenruleDirectory -Filter '*.md' -File)
if ($ruleDocuments.Count -ne 1 -or $ruleDocuments[0].Name -ne 'README.md') {
    Add-Failure 'retired Cgen risk-ledger documents returned under cgenrule'
}

foreach ($relative in $canonicalDocuments) {
    $path = Join-Path $Root $relative
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-Failure "canonical document is missing: $relative"
        continue
    }

    $text = Read-Utf8 $path
    if ($text -match 'RISK-[A-Z]+-[0-9]+') {
        Add-Failure "retired AST Cgen risk id appears in current authority: $relative"
    }
    if ($text -match 'cgenrule[/\\][0-9][0-9]_') {
        Add-Failure "retired numbered Cgen rule is cited by current authority: $relative"
    }
}

$readmePath = Join-Path $cgenruleDirectory 'README.md'
if (Test-Path -LiteralPath $readmePath -PathType Leaf) {
    $readme = Read-Utf8 $readmePath
    foreach ($required in @(
        'AST Cgen',
        'Git '
    )) {
        if (!$readme.Contains($required)) {
            Add-Failure "canonical C emitter boundary does not record risk-ledger retirement: $required"
        }
    }
}

if ($failures.Count -ne 0) {
    Write-Host 'Retired Cgen risk-ledger check failed.'
    $failures | Select-Object -First 80 | ForEach-Object { Write-Host "  $_" }
    if ($failures.Count -gt 80) {
        Write-Host "  ... $($failures.Count - 80) more failure(s)"
    }
    exit 1
}

Write-Host 'Retired Cgen risk ledger ok: no active AST Cgen risk records or numbered rule documents.'
exit 0

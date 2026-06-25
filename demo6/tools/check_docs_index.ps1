param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$docsDir = Join-Path $Root "docs"
$readmePath = Join-Path $docsDir "README.md"
$matrixName = ([char]0x6587) + ([char]0x6863) + ([char]0x72B6) + ([char]0x6001) + ([char]0x77E9) + ([char]0x9635) + ".md"
$matrixPath = Join-Path $docsDir $matrixName
$settlementName = ([char]0x6587) + ([char]0x6863) + ([char]0x6574) + ([char]0x7406) + ([char]0x6536) + ([char]0x53E3) + ".md"
$settlementPath = Join-Path $docsDir $settlementName
$toolsDir = Join-Path $Root "tools"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

if ( ! (Test-Path -LiteralPath $docsDir -PathType Container) ) {
    Write-Host "docs directory missing: $docsDir"
    exit 1
}

if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
    Write-Host "docs README missing: $readmePath"
    exit 1
}

if ( ! (Test-Path -LiteralPath $matrixPath -PathType Leaf) ) {
    Write-Host "docs status matrix missing: $matrixPath"
    exit 1
}

if ( ! (Test-Path -LiteralPath $settlementPath -PathType Leaf) ) {
    Write-Host "docs settlement document missing: $settlementPath"
    exit 1
}

$readme = Read-Utf8Text $readmePath
$matrix = Read-Utf8Text $matrixPath
$settlement = Read-Utf8Text $settlementPath
$docs = @(Get-ChildItem -LiteralPath $docsDir -Filter "*.md" -File | Sort-Object Name)
$stalePatterns = @(
    '\bTODO\b',
    '\bTBD\b',
    '\bFIXME\b',
    'placeholder',
    '\u5F85\u586B',
    '\u672A\u6574\u7406',
    '\u5F85\u786E\u8BA4'
)

$forbiddenPolicyPatterns = @(
    '\.\.\\demo5\\release',
    'demo5\\release\\tcc\\lib\\libtcc1-64\.a',
    'xlUninit',
    'xlUnInit'
)

foreach ( $doc in $docs ) {
    $text = Read-Utf8Text $doc.FullName

    if ( $text.Contains([char]0) ) {
        Add-Failure "NUL/control character found in docs/$($doc.Name)"
    }

    if ( $doc.Name -ne "README.md" -and ! $readme.Contains($doc.Name) ) {
        Add-Failure "docs README does not reference docs/$($doc.Name)"
    }

    if ( ! $matrix.Contains($doc.Name) ) {
        Add-Failure "docs status matrix does not reference docs/$($doc.Name)"
    }

    foreach ( $pattern in $stalePatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "stale marker remains in docs/$($doc.Name): $pattern"
        }
    }

    foreach ( $pattern in $forbiddenPolicyPatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "forbidden stale policy wording in docs/$($doc.Name): $pattern"
        }
    }
}

if ( ! $readme.Contains("../cgenrule/README.md") ) {
    Add-Failure "docs README does not link cgenrule README"
}

if ( Test-Path -LiteralPath $toolsDir -PathType Container ) {
    $checkTools = @(Get-ChildItem -LiteralPath $toolsDir -Filter "check_*.ps1" -File | Sort-Object Name)
    foreach ( $tool in $checkTools ) {
        if ( ! $settlement.Contains($tool.Name) ) {
            Add-Failure "docs settlement does not mention tools/$($tool.Name)"
        }
    }
}

if ( ! $settlement.Contains("rc0_guard.bat") -and ! $settlement.Contains("rc0_guard.ps1") ) {
    Add-Failure "docs settlement does not mention RC0 guard entry"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "docs index ok: $($docs.Count) markdown files."

param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$docsDir = Join-Path $Root "docs"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Check-BatchAsciiBytes($path, $relative)
{
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $start = 0

    if ( $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF ) {
        $start = 3
    }

    for ( $i = $start; $i -lt $bytes.Length; $i++ ) {
        $b = $bytes[$i]
        if ( $b -gt 127 -or ($b -lt 32 -and $b -ne 9 -and $b -ne 10 -and $b -ne 13) ) {
            Add-Failure "batch file must stay ASCII after optional UTF-8 BOM: ${relative} byte $i"
            return
        }
    }
}

function Normalize-RelativePath($path)
{
    return $path.Replace("/", "\")
}

if ( ! (Test-Path -LiteralPath $docsDir -PathType Container) ) {
    Write-Host "docs directory missing: $docsDir"
    exit 1
}

$baselineDocs = @(Get-ChildItem -LiteralPath $docsDir -Filter "*.md" -File | Where-Object {
    (Read-Utf8Text $_.FullName).Contains("rc0_guard.bat -SkipFullTests")
})

if ( $baselineDocs.Count -ne 1 ) {
    Write-Host "RC0 baseline doc lookup failed: expected 1 match, actual $($baselineDocs.Count)"
    exit 1
}

$baselineText = Read-Utf8Text $baselineDocs[0].FullName
$batchFiles = @(Get-ChildItem -LiteralPath $Root -File -Filter "*.bat" | Sort-Object Name)
$toolBatchFiles = @(Get-ChildItem -LiteralPath (Join-Path $Root "tools") -File -Filter "*.bat" | Sort-Object Name)
$toolchainEnv = "tools\xl6_toolchain_env.bat"
$toolchainEnvPath = Join-Path $Root $toolchainEnv

if ( ! (Test-Path -LiteralPath $toolchainEnvPath -PathType Leaf) ) {
    Add-Failure "toolchain environment batch is missing: $toolchainEnv"
}

foreach ( $bat in @($batchFiles + $toolBatchFiles) ) {
    $relative = $bat.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1)
    Check-BatchAsciiBytes $bat.FullName $relative
}

foreach ( $bat in $batchFiles ) {
    $text = Read-Utf8Text $bat.FullName

    if ( $text -notmatch '(?im)^@echo\s+off\s*$' ) {
        Add-Failure "batch file missing '@echo off': $($bat.Name)"
    }
    if ( $text -notmatch '(?im)^setlocal\b' ) {
        Add-Failure "batch file missing setlocal: $($bat.Name)"
    }
    if ( $text -notmatch '(?im)^cd\s+/d\s+%~dp0\s*$' ) {
        Add-Failure "batch file does not switch to script directory: $($bat.Name)"
    }
    if ( ! $baselineText.Contains($bat.Name) ) {
        Add-Failure "batch file is not listed in RC0 baseline doc: $($bat.Name)"
    }

    $toolchainUse = [regex]::Match($text, '(?i)%XL6_(TCC|FLEX|LEMON|XRT|DEFAULT)[A-Z0-9_]*%')
    if ( $toolchainUse.Success ) {
        $envCall = [regex]::Match($text, '(?im)^\s*call\s+tools[\\/]xl6_toolchain_env\.bat\s*$')
        if ( ! $envCall.Success ) {
            Add-Failure "batch file uses toolchain variables without calling ${toolchainEnv}: $($bat.Name)"
        } elseif ( $envCall.Index -gt $toolchainUse.Index ) {
            Add-Failure "batch file calls ${toolchainEnv} after first toolchain variable use: $($bat.Name)"
        }
    }

    foreach ( $match in [regex]::Matches($text, '(?im)^\s*call\s+([^"&|<>`r`n]+?\.bat)(?:\s|$)') ) {
        $targetText = Normalize-RelativePath ($match.Groups[1].Value.Trim())
        if ( $targetText.StartsWith("%") ) {
            continue
        }

        $targetPath = Join-Path $Root $targetText
        if ( ! (Test-Path -LiteralPath $targetPath -PathType Leaf) ) {
            Add-Failure "batch call target is missing: $($bat.Name) -> $targetText"
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Batch wiring ok: $($batchFiles.Count) root batch files, toolchain env guarded."
exit 0

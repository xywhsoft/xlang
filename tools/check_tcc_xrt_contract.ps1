param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$cgenDir = Join-Path $Root "src\cgen"
$commonPath = Join-Path $PSScriptRoot "xrt_snapshot_contract.ps1"
$runtimePath = Join-Path $Root "release\include\xl_runtime.h"

if ( ! (Test-Path -LiteralPath $cgenDir -PathType Container) ) {
    throw "missing C generator directory: $cgenDir"
}
if ( ! (Test-Path -LiteralPath $commonPath -PathType Leaf) ) {
    throw "missing XRT snapshot contract helper: $commonPath"
}
if ( ! (Test-Path -LiteralPath $runtimePath -PathType Leaf) ) {
    throw "missing xlang runtime header: $runtimePath"
}

. $commonPath

$registered = Read-XrtSnapshotRuntimeSymbols -Root $Root
Assert-XrtSnapshotRuntimeWiring -Root $Root
$declarations = @()
$runtimeText = [System.IO.File]::ReadAllText(
    $runtimePath,
    [System.Text.Encoding]::UTF8)

if ( $runtimeText -match '\bmax_align_t\b' ) {
    throw "xl_runtime.h must not require the C11-only max_align_t typedef"
}
if ( $runtimeText -notmatch 'typedef\s+union\s+xl6_value_record_align\b' -or
     $runtimeText -notmatch '\bxl6_value_record_align\s+Align\s*;' ) {
    throw "xl_runtime.h is missing the C99-compatible value-record alignment contract"
}

# Read only C prototypes starting at the first byte of an emitted string.
# Slice before parsing so Windows PowerShell and long lines behave consistently.
$outputPrefix = 'xl6OutputPuts("'
$outputSuffix = '\n"'
$prototypePattern = '^(?!(?:return|if|for|while|switch|case)\b)(?<return>[A-Za-z_][A-Za-z0-9_]*(?:[ \t]+[A-Za-z_][A-Za-z0-9_]*)*(?:[ \t]*\*)*)[ \t]*(?<![A-Za-z0-9_])(?<name>xrt[A-Za-z0-9_]+)\s*\((?<params>[^;{}=]*)\);$'

foreach ( $file in (Get-ChildItem -LiteralPath $cgenDir -File -Filter '*.c') ) {
    foreach ( $line in (Get-Content -LiteralPath $file.FullName -Encoding UTF8) ) {
        $trimmed = $line.TrimStart()
        if ( ! $trimmed.StartsWith($outputPrefix, [System.StringComparison]::Ordinal) ) {
            continue
        }

        $payloadStart = $outputPrefix.Length
        $payloadEnd = $trimmed.IndexOf(
            $outputSuffix,
            $payloadStart,
            [System.StringComparison]::Ordinal)
        if ( $payloadEnd -lt 0 ) {
            continue
        }

        $payload = $trimmed.Substring($payloadStart, $payloadEnd - $payloadStart)
        if ( $payload.Length -eq 0 -or [char]::IsWhiteSpace($payload[0]) ) {
            continue
        }
        if ( $payload -notmatch $prototypePattern ) {
            continue
        }

        $declarations += [pscustomobject]@{
            Name = $Matches['name']
            ReturnType = (($Matches['return'] -replace '\s+', ' ').Trim())
            Signature = (($payload -replace '\\"', '"') -replace '\s+', ' ').Trim()
            Path = $file.FullName
        }
    }
}

if ( $env:XL6_CONTRACT_DEBUG ) {
    Write-Host "root=$Root declarations=$($declarations.Count) runtimeSymbols=$($registered.Count)"
}

$failed = $false
$declaredNames = @(
    $declarations |
        ForEach-Object { $_.Name } |
        Sort-Object -Unique
)

foreach ( $name in $declaredNames ) {
    if ( $registered.Contains($name) ) {
        continue
    }
    Write-Host "emitted XRT declaration has no runtime symbol: $name" -ForegroundColor Red
    $failed = $true
}

# Storage-class specifiers are not part of a C return type.  Canonical XLIR
# output uses explicit extern declarations; normalize them before comparison.
# Parameter names are not ABI. Check normalized return-type conflicts here;
# generated TCC compilation gates validate the complete parameter contract.
foreach ( $item in @($declarations | Group-Object Name) ) {
    $returnTypes = @($item.Group | ForEach-Object {
        ($_.ReturnType -replace '^extern\s+', '').Trim()
    } | Select-Object -Unique)
    if ( $returnTypes.Count -le 1 ) {
        continue
    }

    Write-Host "conflicting emitted return types for $($item.Name)" -ForegroundColor Red
    $item.Group |
        Sort-Object Signature, Path |
        ForEach-Object {
            Write-Host "  $($_.Signature) [$($_.Path)]" -ForegroundColor Red
        }
    $failed = $true
}

if ( $failed ) {
    exit 1
}

Write-Host "TCC/XRT contract ok: $($registered.Count) runtime symbols, $($declaredNames.Count) emitted declarations"

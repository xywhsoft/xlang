function Read-XrtSnapshotRuntimeSymbols
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $symbolPath = Join-Path $Root "src\api\xl_xrt_symbols.inc"
    $snapshotPath = Join-Path $Root "lib\xrt\xrt.snapshot.json"
    $runtimeSnapshotPath = Join-Path $Root "lib\xruntime\xruntime.snapshot.json"

    if ( ! (Test-Path -LiteralPath $symbolPath -PathType Leaf) ) {
        throw "XRT runtime symbol table missing: $symbolPath"
    }
    if ( ! (Test-Path -LiteralPath $snapshotPath -PathType Leaf) ) {
        throw "XRT snapshot metadata missing: $snapshotPath"
    }
    if ( ! (Test-Path -LiteralPath $runtimeSnapshotPath -PathType Leaf) ) {
        throw "xruntime snapshot metadata missing: $runtimeSnapshotPath"
    }

    $text = [System.IO.File]::ReadAllText($symbolPath, [System.Text.Encoding]::UTF8)
    $matches = [regex]::Matches(
        $text,
        '(?m)^\s*XL_XRT_SYMBOL\((?<name>xrt[A-Za-z0-9_]+)\)\s*$')
    $invocations = [regex]::Matches($text, '(?m)^\s*XL_XRT_SYMBOL\(').Count

    if ( $matches.Count -ne $invocations ) {
        throw "non-canonical XRT symbol entry in $symbolPath"
    }
    if ( $matches.Count -lt 1000 ) {
        throw "XRT runtime symbol baseline regressed: $($matches.Count) < 1000"
    }

    $symbols = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ( $match in $matches ) {
        $name = $match.Groups['name'].Value
        if ( ! $symbols.Add($name) ) {
            throw "duplicate XRT runtime symbol: $name"
        }
    }

    $snapshot = Get-Content -LiteralPath $snapshotPath -Raw -Encoding UTF8 |
        ConvertFrom-Json
    $runtimeSnapshot = Get-Content -LiteralPath $runtimeSnapshotPath -Raw -Encoding UTF8 |
        ConvertFrom-Json
    if ( [int]$snapshot.schema -ne 3 -or $snapshot.product -cne "xrt" ) {
        throw "unsupported XRT snapshot schema: $($snapshot.schema)"
    }
    if ( [int]$runtimeSnapshot.schema -ne 3 -or $runtimeSnapshot.product -cne "xruntime" ) {
        throw "unsupported xruntime snapshot schema: $($runtimeSnapshot.schema)"
    }
    if ( [int]$runtimeSnapshot.publicApiCount -ne $symbols.Count ) {
        throw "xruntime snapshot count mismatch: metadata=$($runtimeSnapshot.publicApiCount), symbols=$($symbols.Count)"
    }
    if ( [int]$runtimeSnapshot.ownedPublicApiCount -ne
         ($symbols.Count - [int]$snapshot.publicApiCount) ) {
        throw "xruntime owned API count mismatch"
    }

    return ,$symbols
}



function Assert-XrtSnapshotRuntimeWiring
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $apiPath = Join-Path $Root "src\api\xl_api_symbols.c"
    if ( ! (Test-Path -LiteralPath $apiPath -PathType Leaf) ) {
        throw "xl API source missing: $apiPath"
    }

    $text = [System.IO.File]::ReadAllText($apiPath, [System.Text.Encoding]::UTF8)
    $includeCount = [regex]::Matches(
        $text,
        '(?m)^\s*#include\s+"xl_xrt_symbols\.inc"\s*$').Count
    $macroCount = [regex]::Matches(
        $text,
        '(?m)^\s*#define\s+XL_XRT_SYMBOL\(name\)\s+\{\s*#name\s*,\s*\(const void\*\)\(name\)\s*\}\s*,\s*$').Count
    $tableCount = [regex]::Matches(
        $text,
        '(?m)^\s*static const XL_XrtRuntimeSymbol xl__xrtRuntimeSymbols\[\]\s*=\s*\{\s*$').Count
    $registrationCount = [regex]::Matches(
        $text,
        '(?s)xlApiAddTccSymbol\(\s*s\s*,\s*xl__xrtRuntimeSymbols\[i\]\.name\s*,\s*xl__xrtRuntimeSymbols\[i\]\.value\s*\)').Count

    if ( $includeCount -ne 1 -or
         $macroCount -ne 1 -or
         $tableCount -ne 1 -or
         $registrationCount -ne 1 ) {
        throw "XRT runtime symbol table is not canonically wired in $apiPath"
    }
}

param(
    [string]$Root = "",
    [int]$MaxLinesPerFile = 2400,
    [int]$MaxBytesPerFile = 95000,
    [int]$LargeLineThreshold = 1200,
    [int]$MaxLargeFiles = 0
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$cgenDir = Join-Path $Root "src\cgen"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

if ( ! (Test-Path -LiteralPath $cgenDir -PathType Container) ) {
    Write-Host "cgen source directory missing: $cgenDir"
    exit 1
}

$files = @(Get-ChildItem -LiteralPath $cgenDir -Filter "*.c" -File | Sort-Object Name)
if ( $files.Count -eq 0 ) {
    Write-Host "No cgen source files found."
    exit 1
}

$irRoot = @($files | Where-Object Name -eq 'xl6_ir_cgen.c')
$irShards = @($files | Where-Object Name -Like 'xl6_ir_cgen_*.c')
$runtimeRoot = @($files | Where-Object Name -eq 'xl6_cgen_shared_runtime.c')
$runtimeComponents = @($files | Where-Object {
    $_.Name -Like 'xl6_cgen_*.c' -and $_.Name -ne 'xl6_cgen_shared_runtime.c'
})
if ( $irRoot.Count -ne 1 -or $irShards.Count -eq 0 ) {
    Add-Failure 'canonical XLIR emitter root/shards are missing'
}
if ( $runtimeRoot.Count -ne 1 -or $runtimeComponents.Count -eq 0 ) {
    Add-Failure 'canonical generated-runtime emitter root/components are missing'
}

$records = New-Object System.Collections.Generic.List[object]
foreach ( $file in $files ) {
    $lineCount = ([System.IO.File]::ReadAllLines($file.FullName)).Count
    $records.Add([pscustomobject]@{
        Name = $file.Name
        Lines = $lineCount
        Bytes = $file.Length
    }) | Out-Null

    if ( $lineCount -gt $MaxLinesPerFile ) {
        Add-Failure "cgen file is too large by line count: $($file.Name) has $lineCount lines, limit $MaxLinesPerFile"
    }

    if ( $file.Length -gt $MaxBytesPerFile ) {
        Add-Failure "cgen file is too large by byte count: $($file.Name) has $($file.Length) bytes, limit $MaxBytesPerFile"
    }
}

$largeFiles = @($records | Where-Object { $_.Lines -gt $LargeLineThreshold } | Sort-Object Lines -Descending)
if ( $largeFiles.Count -gt $MaxLargeFiles ) {
    $sample = $largeFiles |
        Select-Object -First 12 |
        ForEach-Object { "{0}({1})" -f $_.Name, $_.Lines }
    Add-Failure "too many large cgen files: $($largeFiles.Count) > $MaxLargeFiles over $LargeLineThreshold lines: $($sample -join ', ')"
}

if ( $failures.Count -ne 0 ) {
    Write-Host "Cgen module size check failed."
    foreach ( $failure in $failures ) {
        Write-Host "  $failure"
    }
    exit 1
}

$largest = @($records | Sort-Object Lines -Descending | Select-Object -First 1)[0]
Write-Host "Cgen module size ok: $($irShards.Count) XLIR shards, $($runtimeComponents.Count) runtime components, largest $($largest.Name) has $($largest.Lines) lines, $($largeFiles.Count) files over $LargeLineThreshold lines."
exit 0

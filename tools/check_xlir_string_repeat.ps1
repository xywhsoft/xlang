param([string]$Root = '', [string]$SourceDriver = '', [string]$PublicSourceDirectory = '', [string]$WorkDirectory = '')
# Repeat shares the exact current ABI, OOM and JIT/AOT matrix.
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
if ($SourceDriver) {
    if ($PublicSourceDirectory) {throw 'Select a frozen build directory or its driver, not both'}
    $SourceDriver = (Resolve-Path -LiteralPath $SourceDriver).Path
    $PublicSourceDirectory = Split-Path -Parent $SourceDriver
    $manifest = Join-Path $PublicSourceDirectory 'build.json'
    if (!(Test-Path -LiteralPath $manifest) -or
        (Get-Content $manifest -Raw -Encoding UTF8 | ConvertFrom-Json).Driver -ne $SourceDriver) {
        throw 'Legacy mutable drivers are unsupported; supply a frozen public-source build'
    }
}
if (!$WorkDirectory) {$WorkDirectory = 'build/test/string-repeat-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')}
& "$PSScriptRoot/check_xlir_string_bridges.ps1" -Root $Root -PublicSourceDirectory $PublicSourceDirectory -WorkDirectory $WorkDirectory
if (!$?) {throw 'Repeat contract matrix failed'}

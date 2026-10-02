param([string]$Root = '', [string]$RuntimeDirectory = '', [string]$WorkDirectory = '', [string]$PublicSourceDirectory = '')
# One authoritative current-source matrix, not an old generated bridge oracle.
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if ($RuntimeDirectory -and (Resolve-Path -LiteralPath $RuntimeDirectory).Path -ne (Join-Path $Root 'release')) {
    throw 'Use the frozen PublicSourceDirectory contract for non-default compilers'
}
if (!$WorkDirectory) { $WorkDirectory = 'build/test/string-bridges-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
Push-Location $Root
try {
    if (!$PublicSourceDirectory) {
        $PublicSourceDirectory = "$WorkDirectory-source"
        & ./tools/build_xlir_public_source_driver.ps1 -Directory $PublicSourceDirectory
        if (!$?) {throw 'Current-source compiler build failed'}
    }
    & ./tools/check_xlir_string_join.ps1 -Root $Root -PublicSourceDirectory $PublicSourceDirectory -WorkDirectory $WorkDirectory
    if (!$?) {throw 'Current string bridge matrix failed'}
} finally {Pop-Location}

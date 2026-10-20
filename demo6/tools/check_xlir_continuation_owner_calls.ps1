param([string]$Root = '', [string]$PublicDirectory = '',
    [string]$Out = '', [string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
$Root = [IO.Path]::GetFullPath($Root)
# Complete-source receipts replace the old partial source + shipping-DLL hybrid.
# Never overwrite shared build/test products or a loaded DLL.
Push-Location $Root
try {
    if (!$Out) { $Out = Join-Path $Root ('build/test/owner-calls-' + [guid]::NewGuid().ToString('N')) }
    $Out = [IO.Path]::GetFullPath($Out)
    if (Test-Path -LiteralPath $Out) { throw 'Choose fresh owner-call evidence; prior artifacts must be preserved' }
    if (!$PublicDirectory) {
        $PublicDirectory = $Out + '-source'
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Complete-source owner-call compiler build failed' }
    }
    & python (Join-Path $Root 'tools/check_xlir_continuation_owner_calls.py') `
        --root $Root --public-directory $PublicDirectory --out $Out --compiler $Compiler
    if ($LASTEXITCODE -ne 0) { throw 'Current-source continuation owner-call contract failed' }
} finally { Pop-Location }
Write-Output 'Current-source owner-call contract passed (strict C/queued runtime; not shipping or JIT/AOT acceptance).'

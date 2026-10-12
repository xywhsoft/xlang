param([string]$Root='', [string]$PublicDirectory='', [string]$Directory='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=[IO.Path]::GetFullPath($Root)
if (!$Directory) { $Directory=Join-Path $Root ('build/test/persistence_'+[guid]::NewGuid().ToString('N')) }
elseif (![IO.Path]::IsPathRooted($Directory)) { $Directory=Join-Path $Root $Directory }
if ($PublicDirectory -and ![IO.Path]::IsPathRooted($PublicDirectory)) { $PublicDirectory=Join-Path $Root $PublicDirectory }
if (!$PublicDirectory) {
    $PublicDirectory=$Directory+'_public'
    Push-Location $Root
    try { & (Join-Path $Root 'tools/build_xlir_public_source_driver.ps1') -Directory $PublicDirectory }
    finally { Pop-Location }
}
# The shared driver pins compiled production inputs, verifies current
# xlir_persistence_json_abi.xl typed/lowered schemas/effects and boxing owners,
# and executes strict C, real source JIT/AOT and complete synchronous OOM.
& python (Join-Path $Root 'tools/check_persistence_project_language.py') --root $Root --public-directory $PublicDirectory --fixture abi --out $Directory
if ($LASTEXITCODE) { throw 'Current-source persistence ABI gate failed' }
Write-Output 'Current-source persistence ABI passed; no full53/shipping acceptance claim.'

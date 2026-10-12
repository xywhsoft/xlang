param([string]$Root='', [string]$PublicDirectory='', [string]$Directory='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=[IO.Path]::GetFullPath($Root)
if (!$Directory) { $Directory=Join-Path $Root ('build/test/class_identity_'+[guid]::NewGuid().ToString('N')) }
if (!$PublicDirectory) {
    $PublicDirectory=$Directory+'_public'
    Push-Location $Root
    try { & (Join-Path $Root 'tools/build_xlir_public_source_driver.ps1') -Directory $PublicDirectory }
    finally { Pop-Location }
}
# Preserves canonical typed hash/equality modes, @compare(skip) partition and
# exact bind/equality counts; verifies actual resident V3 schemas, metadata
# mutation/OOM refusal, deterministic emitted C, O0/O2, TCC JIT, new module and
# standalone AOT. It does not fall back to an older release compiler/library.
& python (Join-Path $Root 'tools/check_xlir_class_identity_plan.py') --root $Root --public $PublicDirectory --out $Directory
if ($LASTEXITCODE) { throw 'Current-source resident class identity language gate failed' }
Write-Output 'XLIR resident class identity source gate passed; not full53/shipping acceptance.'

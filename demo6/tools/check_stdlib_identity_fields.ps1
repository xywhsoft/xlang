param([string]$Root='', [string]$PublicDirectory='', [string]$Directory='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=[IO.Path]::GetFullPath($Root)
if (!$Directory) { $Directory=Join-Path $Root ('build/test/identity_fields_'+[guid]::NewGuid().ToString('N')) }
if (![IO.Path]::IsPathRooted($Directory)) { $Directory=Join-Path $Root $Directory }
if (Test-Path -LiteralPath $Directory) { throw 'Choose a fresh identity/fields evidence directory' }
if (!$PublicDirectory) {
    $PublicDirectory=$Directory+'_public'
    Push-Location $Root
    try { & (Join-Path $Root 'tools/build_xlir_public_source_driver.ps1') -Directory $PublicDirectory }
    finally { Pop-Location }
}
New-Item -ItemType Directory -Path $Directory | Out-Null
$parts=@(
    @('fields','check_stdlib_dynamic_fields_resident.py'),
    @('identity','check_xlir_class_identity_plan.py'),
    @('class-runtime','check_xlir_class_identity_runtime.py'),
    @('fields-native','check_stdlib_object_fields_native.py')
)
$reports=@()
Push-Location $Root
try {
    foreach ($part in $parts) {
        $target=Join-Path $Directory $part[0]
        $arguments=@((Join-Path $Root ('tools/'+$part[1])),'--out',$target)
        if ($part[0] -in @('fields','identity')) { $arguments+=@('--root',$Root,'--public-directory',$PublicDirectory) }
        & python @arguments
        if ($LASTEXITCODE) { throw ('Identity/fields batch failed: '+$part[0]) }
        $reportPath=Join-Path $target 'results.json'
        $result=Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
        if (!$result.Complete -or !$result.InputsUnchanged) { throw 'Incomplete/changed source evidence' }
        $reports+=@{Name=$part[0];Path=$reportPath;Sha256=(Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash;Phases=$result.Rows.Count}
    }
} finally { Pop-Location }
$report=@{Complete=$true;Reports=$reports;Full53Acceptance=$false;ShippingArtifact=$false;WaitQueueComplete=$false}
$json=$report | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText((Join-Path $Directory 'results.json'),$json,[Text.UTF8Encoding]::new($false))
Write-Output 'Current-source identity/fields batch passed; full53 and shipping acceptance remain separate.'

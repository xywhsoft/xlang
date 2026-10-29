param([string]$Root='', [string]$PublicDirectory='', [string]$Directory='', [switch]$CatalogOnly)
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=[IO.Path]::GetFullPath($Root)
$python=(Get-Command python -CommandType Application | Select-Object -First 1).Source
Push-Location $Root
try {
    if (!$Directory) { $Directory='build/test/runtime-catalog-'+[guid]::NewGuid().ToString('N') }
    $out=[IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Directory)) { $Directory } else { Join-Path $Root $Directory }))
    if (Test-Path -LiteralPath $out) { throw 'Preserve previous ownership/Thread evidence; choose a fresh directory' }
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($out)) | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory=$out+'-source'
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source ownership/Thread compiler build failed' }
    }
    $arguments=@("$PSScriptRoot/check_xlir_runtime_operand_catalog.py",'--root',$Root,
        '--public-directory',$PublicDirectory,'--out',$out)
    if ($CatalogOnly) { $arguments+='--catalog-only' }
    & $python @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Compiled ownership/resident Thread quality gate failed' }
    $report=Get-Content -LiteralPath "$out/results.json" -Raw | ConvertFrom-Json
    if (!$report.Complete -or !$report.InputsUnchanged -or !$report.SourceInputsMatch) { throw 'Incomplete compiled contract evidence' }
    Write-Output "Compiled ownership/Thread passed: $($report.Rows.Count) phases; full53 and shipping acceptance remain separate."
} finally { Pop-Location }

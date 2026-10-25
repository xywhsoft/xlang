param([string]$Driver = 'test/cases/xlir_owner_consumption_contract.c',
    [string]$Output = 'build/test/xlir_validation_driver.exe',
    [string[]]$BatchDrivers = @(), [string]$BatchDirectory = '',
    [string]$PublicDirectory = '', [switch]$NoCache)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_c_object_cache.ps1')
$rootPath = [IO.Path]::GetFullPath((Get-Location).Path)
$drivers = @(if ($BatchDrivers.Count) { $BatchDrivers } else { $Driver })
$drivers = @($drivers | ForEach-Object { (Get-Item -LiteralPath $_ -ErrorAction Stop).FullName })
if ($BatchDrivers.Count -and !$BatchDirectory) { throw 'BatchDirectory is required for shared contract builds' }
$batchPath = [IO.Path]::GetFullPath($(if ($BatchDrivers.Count) { $BatchDirectory } else { $Output + '.build' }))
if (Test-Path -LiteralPath $batchPath) { throw 'Choose fresh contract evidence; prior builds must not be overwritten' }
$targets = @($drivers | ForEach-Object {
    if ($BatchDrivers.Count) { Join-Path $batchPath ([IO.Path]::GetFileNameWithoutExtension($_) + '.exe') }
    else { [IO.Path]::GetFullPath($Output) }
})
if (@($targets | Select-Object -Unique).Count -ne $targets.Count) { throw 'Contract target basename collision' }
foreach ($target in $targets) {
    if (Test-Path -LiteralPath $target) { throw "Refusing to overwrite contract executable: $target" }
}
# Contracts and public scenarios use one complete source cohort. A second
# hand-maintained frontend list cannot describe modern Cgen/provider dependencies.
if (!$PublicDirectory) {
    $PublicDirectory = $batchPath + '-source'
    & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory -NoCache:$NoCache
    if (!$? -or $LASTEXITCODE -ne 0) { throw 'Complete current-source contract prerequisite failed' }
}
$publicPath = (Resolve-Path -LiteralPath $PublicDirectory).Path
$publicManifest = Join-Path $publicPath 'build.json'
$public = Get-Content -LiteralPath $publicManifest -Raw | ConvertFrom-Json
if (!$public.Complete -or !$public.InputsUnchanged -or !$public.SourceXrt -or $public.RuntimeOnly) {
    throw 'Contracts require a complete current-source compiler cohort'
}
$driverInputs = @($public.Inputs | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xlir_public_source_driver.c' })
if ($driverInputs.Count -ne 1) { throw 'Ambiguous public compiler source root' }
$compiledRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $driverInputs[0].Path))
if (![IO.Path]::GetFullPath($compiledRoot).Equals($rootPath,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a source cohort compiled from this exact reviewed root'
}
$objects = @($public.Objects | Where-Object {
    [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o'
} | ForEach-Object Path)
if (!$objects.Count -or $objects.Count + 1 -ne $public.Objects.Count) { throw 'Incomplete source object population' }
$cohortInputs = @($public.Inputs) + @($public.Headers) + @($public.Objects)
function Assert-Cohort {
    foreach ($row in $cohortInputs) {
        if ((Get-FileHash -LiteralPath $row.Path).Hash -ine $row.Sha256) { throw "Cohort input changed: $($row.Path)" }
    }
    if ((Get-FileHash -LiteralPath $public.Driver).Hash -ine $public.DriverSha256) { throw 'Public source compiler changed' }
}
Assert-Cohort
$buildContext = New-XlirCBuildContext -Root $rootPath -Directory $batchPath -NoCache:$NoCache
$arName = @(& $buildContext.Compiler '-print-prog-name=ar')
if ($LASTEXITCODE -ne 0 -or $arName.Count -ne 1) { throw 'Cannot resolve compiler archive tool' }
$ar = (Get-Command -Name $arName[0] -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$inputFiles = @($PSCommandPath,"$PSScriptRoot/xlir_c_object_cache.ps1","$PSScriptRoot/xlir_native_process.ps1",
    $publicManifest,$ar,'release/xl.dll') + @($drivers)
$inputRecords = @($inputFiles | ForEach-Object {
    $file = Get-Item -LiteralPath $_
    [pscustomobject]@{Path=$file.FullName;Sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
})
$utf8 = [Text.UTF8Encoding]::new($false)
$archive = Join-Path $batchPath 'current-compiler.a'
$response = Join-Path $batchPath 'archive.rsp'
[IO.File]::WriteAllText($response,(@($objects | ForEach-Object {
    '"' + $_.Replace('\','/').Replace('"','\"') + '"'
}) -join [Environment]::NewLine),$utf8)
& $ar 'rcs' $archive ("@" + $response)
if ($LASTEXITCODE -ne 0) { throw 'Current source archive failed' }
$archiveHash = (Get-FileHash -LiteralPath $archive).Hash
$runtimeHash = (Get-FileHash -LiteralPath release/xl.dll).Hash
$runtimeDirectories = @(@($targets | ForEach-Object { Split-Path -Parent $_ }) + @($batchPath) | Sort-Object -Unique)
$runtimeDeployments = @()
foreach ($directory in $runtimeDirectories) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $destination = Join-Path $directory 'xl.dll'
    if (Test-Path -LiteralPath $destination) {
        if ((Get-FileHash -LiteralPath $destination).Hash -cne $runtimeHash) {
            throw "Existing runtime differs; use a fresh contract output directory: $destination"
        }
    } else { Copy-Item -LiteralPath release/xl.dll -Destination $destination }
    if ((Get-FileHash -LiteralPath $destination).Hash -cne $runtimeHash) { throw 'Runtime deployment changed bytes' }
    $runtimeDeployments += [pscustomobject]@{Path=$destination;Sha256=$runtimeHash}
}
$strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-DXL_STATIC_EMBED',
    '-ffunction-sections','-fdata-sections','-Wno-unused-function','-Wno-unused-label',
    '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
    '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign',
    '-Wno-format-truncation',"-I$rootPath","-I$rootPath/release","-I$rootPath/release/include",
    "-I$publicPath/include")
$links = @('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-Wl,--gc-sections')
$records = @(); $driverRecords = @(); $failed = @()
for ($index=0; $index -lt $drivers.Count; ++$index) {
    $source = (Resolve-Path -LiteralPath $drivers[$index]).Path
    $object = Join-Path $batchPath ("contract-$index.o")
    $record = Invoke-XlirCObject -Context $buildContext -Source $source -Object $object -Flags $strict
    $records += $record
    Write-Output "[$($index+1)/$($drivers.Count)] contract object: $source"
    $parent = Split-Path -Parent $targets[$index]
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    & $buildContext.Compiler @strict $object $archive @links '-o' $targets[$index]
    if ($LASTEXITCODE -ne 0) { $failed += $drivers[$index] }
    else { $driverRecords += [pscustomobject]@{Path=$targets[$index];Sha256=(Get-FileHash -LiteralPath $targets[$index]).Hash} }
}
if ($failed.Count) { throw "Contract driver links failed: $($failed -join ', ')" }
Assert-Cohort
foreach ($row in $inputRecords) {
    if ((Get-FileHash -LiteralPath $row.Path).Hash -ine $row.Sha256) { throw "Contract build input changed: $($row.Path)" }
}
if ((Get-FileHash -LiteralPath $archive).Hash -cne $archiveHash) { throw 'Compiler archive changed during links' }
Assert-XlirCToolchainUnchanged $buildContext
$manifest = [pscustomobject]@{Complete=$true;InputsUnchanged=$true;SourceXrt=$true;
    RecordedAt=(Get-Date).ToString('o');Inputs=$inputRecords;Drivers=$driverRecords;
    PublicDirectory=$publicPath;PublicManifestSha256=(Get-FileHash -LiteralPath $publicManifest).Hash;
    CompilerObjects=$objects.Count;Archive=$archive;ArchiveSha256=$archiveHash;
    RuntimeSha256=(Get-FileHash release/xl.dll).Hash;
    Objects=@($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' });
    DriverObjects=$records;RuntimeDeployments=$runtimeDeployments;
    ToolchainSha256=$buildContext.ToolchainSha256;
    Scope='One complete current-source cohort; ordinary lazy archive linking, no alternate provider or partial frontend source list. Private contracts are not shipping acceptance.'}
[IO.File]::WriteAllText((Join-Path $batchPath 'build.json'),($manifest | ConvertTo-Json -Depth 8),$utf8)
Write-Output "Contract archive built: $($drivers.Count) drivers from $($objects.Count) verified source objects"
exit 0

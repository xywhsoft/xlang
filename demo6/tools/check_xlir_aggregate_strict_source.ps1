param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$AggregateDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$outputPath=if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory)) }
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new strict aggregate directory' }
$build=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$aggregate=Get-Content -LiteralPath (Join-Path $AggregateDirectory 'results.json') -Raw | ConvertFrom-Json
$emission=@($aggregate.Emission)
$producerLanes=@($aggregate.Lanes)
if (!$build.SourceXrt -or !$aggregate.InputsUnchanged -or !$aggregate.Deterministic -or
    $emission.Count -ne 2 -or @($emission | Where-Object {!$_.Complete -or $_.Exit -ne 0}).Count -or
    $producerLanes.Count -ne 4 -or @($producerLanes | Where-Object {!$_.Complete}).Count) {
    throw 'Missing current aggregate source provenance'
}
$generated=Join-Path $AggregateDirectory 'emit1.c'
$repeat=Join-Path $AggregateDirectory 'emit2.c'
if (!(Test-Path -LiteralPath $generated -PathType Leaf) -or
    (Get-FileHash -LiteralPath $generated).Hash -ne (Get-FileHash -LiteralPath $repeat).Hash) {
    throw 'Missing deterministic aggregate source'
}
$objects=@($build.Objects | Where-Object {
    [IO.Path]::GetFileName($_.Path) -notin @('xlir_public_source_driver.o','xlir_public_runtime_driver.o')
})
if ($objects.Count -ne $build.Objects.Count - 1) { throw 'Ambiguous public driver object exclusion' }
$hostSource='test/cases/xlir_aggregate_strict_source_host.c'
$files=@(Get-Item $PSCommandPath,$hostSource,$generated,$repeat,release/xl.dll)
$inputs=@($build.Inputs)+@($build.Headers)+@($objects)+
    @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) {
    throw 'Stale strict aggregate source or objects'
}
New-Item -ItemType Directory -Path $outputPath | Out-Null
Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $outputPath 'xl.dll')
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation','-DXL_STATIC_EMBED','-DXLIR_GENERATED_MODULE_PREINCLUDED',
    "-I$includePath",'-I.','-Irelease','-Irelease/include','-Ilib/tcc')
$links=@($objects | ForEach-Object {$_.Path})+@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$results=@()
foreach ($optimization in @('-O0','-O2')) {
    $name="aggregate$optimization"; $binary="$outputPath/$name.exe"
    $compile=Invoke-XlirGccToLog ($flags+@($optimization,'-include',$generated,$hostSource)+$links+@('-o',$binary)) "$outputPath/$name.build.log"
    $run=$null
    if ($compile -eq 0) { $run=Invoke-XlirNativeToLog $binary @() "$outputPath/$name.run.log" }
    $results+=[pscustomobject]@{Optimization=$optimization;Build=$compile;Run=$run}
    Write-Output "$name build=$compile run=$run"
}
$unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
    Generated=$generated;GeneratedSha256=(Get-FileHash -LiteralPath $generated).Hash;Lanes=$results;Expected=2;
    Scope='Deterministic aggregate source compiled as strict C at host O0/O2 and initialized against the pinned current-source public runtime object set.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or $results.Count -ne 2 -or
    @($results | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0}).Count) { throw 'Strict aggregate batch failed' }

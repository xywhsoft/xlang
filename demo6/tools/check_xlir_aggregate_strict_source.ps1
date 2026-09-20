param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$AggregateDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new strict aggregate directory' }
$build=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$aggregate=Get-Content -LiteralPath (Join-Path $AggregateDirectory 'results.json') -Raw | ConvertFrom-Json
if (!$build.SourceXrt -or !$aggregate.InputsUnchanged -or $aggregate.Public.Count -ne 5) { throw 'Missing source provenance' }
$names=@('xrt_single_header_impl.o','xl_future_bridge.o','xl6_future_payload.o',
    'xl6_continuation_queue.o','xl6_continuation_executor.o','xl6_continuation_future.o')
$objects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path) -in $names})
if ($objects.Count -ne $names.Count) { throw 'Incomplete resident object set' }
$files=@(Get-Item $PSCommandPath,test/cases/xlir_generated_module_diagnostic_host.c)
$files+=@($aggregate.Public|ForEach-Object {Get-Item -LiteralPath (Join-Path $AggregateDirectory "$($_.Name).c")})
$inputs=@($build.Inputs)+@($build.Headers)+@($objects)+
    @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if (@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale strict aggregate source or objects' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
    '-DXL_STATIC_EMBED','-DXLIR_GENERATED_MODULE_PREINCLUDED',"-I$includePath",'-I.','-Irelease')
$links=@($objects|ForEach-Object {$_.Path})+@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$results=@()
foreach ($optimization in @('-O0','-O2')) { foreach ($case in $aggregate.Public) {
    $name="$($case.Name)$optimization"; $binary="$outputPath/$name.exe"
    $generated=Join-Path $AggregateDirectory "$($case.Name).c"
    & gcc @flags $optimization -include $generated test/cases/xlir_generated_module_diagnostic_host.c @links -o $binary *> "$outputPath/$name.build.log"
    $compile=$LASTEXITCODE; $run=$null
    if ($compile -eq 0) { & $binary *> "$outputPath/$name.run.log"; $run=$LASTEXITCODE }
    $results+=[pscustomobject]@{Name=$case.Name;Optimization=$optimization;Build=$compile;Run=$run}
    Write-Output "$name build=$compile run=$run"
} }
$unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$results;Expected=10}
[IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or $results.Count -ne 10 -or @($results|Where-Object {$_.Build -ne 0 -or $_.Run -ne 0}).Count) { throw 'Strict aggregate batch failed' }

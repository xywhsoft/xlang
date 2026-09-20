param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [string]$Directory = 'build/test/xlir_capture_ownership_batch',
    [string]$Tcc = 'E:/software/tcc/tcc.exe')
$ErrorActionPreference = 'Stop'
$rootPath = (Get-Location).Path
$outputPath = [IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new evidence directory' }
$manifestPath = Join-Path $PublicDirectory 'build.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if (!$manifest.SourceXrt) { throw 'Capture graph tests require current source XRT' }
$runtimeObject = @($manifest.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
if ($runtimeObject.Count -ne 1) { throw 'Source runtime object is ambiguous' }
$records = @($manifest.Inputs) + @($manifest.Headers) + @($runtimeObject)
$extra = @(Get-Item $PSCommandPath,$ContractDriver,$manifest.Driver,
    test/cases/xlir_capture_ownership.xl,test/cases/xlir_capture_ownership_host.c,
    test/cases/xlir_capture_ownership_contract.c)
$records += @($extra | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
if (@($records | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Stale capture graph inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$prefix = Join-Path $outputPath 'xlir_capture_ownership'
$results = @()
& $ContractDriver *> "$outputPath/ir-contract.log"
$contractExit = $LASTEXITCODE
foreach ($pass in @('', '.repeat')) {
    & $manifest.Driver test/cases/xlir_capture_ownership.xl emit "$prefix$pass" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Capture graph generation failed' }
}
foreach ($suffix in @('c','h','json')) {
    if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { throw "Nondeterministic capture $suffix" }
}
$includePath = [IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
    '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation',"-I$includePath","-I$outputPath",'-I.')
$libs = @('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$residentSources = @('src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c',
    'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_future_payload.c','src/runtime/xl6_continuation_queue.c')
foreach ($optimization in @('O0','O2')) {
    $binary = "$outputPath/gcc-$optimization.exe"
    & gcc @strict "-$optimization" test/cases/xlir_capture_ownership_host.c @residentSources $runtimeObject[0].Path @libs -o $binary *> "$outputPath/gcc-$optimization.build.log"
    $buildExit = $LASTEXITCODE; $runExit = $null
    if ($buildExit -eq 0) { & $binary *> "$outputPath/gcc-$optimization.run.log"; $runExit = $LASTEXITCODE }
    $results += [pscustomobject]@{Lane="gcc-$optimization";Build=$buildExit;Run=$runExit}
}
# Standalone TinyCC uses legacy CRT imports by default; explicitly link the
# system UCRT for standard strtoll/strtoull, without source substitutions.
$binary = "$outputPath/tcc.exe"
& $Tcc -std=c11 "-I$includePath" "-I$outputPath" -I. test/cases/xlir_capture_ownership_host.c @residentSources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/tcc.build.log"
$buildExit = $LASTEXITCODE; $runExit = $null
if ($buildExit -eq 0) { & $binary *> "$outputPath/tcc.run.log"; $runExit = $LASTEXITCODE }
$results += [pscustomobject]@{Lane='tcc';Build=$buildExit;Run=$runExit}
$unchanged = @($records | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$records;InputsUnchanged=$unchanged;
    IrContractExit=$contractExit;Deterministic=$true;Lanes=$results;GraphsPerLane=700;
    Scope='Verified capture ABI emitted into real closures/cells/bound receivers and callable projections; current XRT graph inspection, native aliases, calls and exact memory balance. No automatic module pin or graph safepoint claim.'}
[IO.File]::WriteAllText("$outputPath/results.json", ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
$results | Format-Table
if (!$unchanged -or $contractExit -ne 0 -or @($results | Where-Object { $_.Build -ne 0 -or $_.Run -ne 0 }).Count) { throw 'Capture graph batch failed; inspect all logs' }

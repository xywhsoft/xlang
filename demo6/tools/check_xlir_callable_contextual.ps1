param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc = 'E:/software/tcc/tcc.exe')
$ErrorActionPreference = 'Stop'
$outputPath = if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory)) }
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new evidence directory' }
$manifest = Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
if (!$manifest.SourceXrt) { throw 'Current source XRT is required' }
$runtime = @($manifest.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
if ($runtime.Count -ne 1) { throw 'Ambiguous runtime object' }
$resident = @('src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c',
    'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
$fixtures = @('xlir_callable_contextual_conversion','xlir_callable_implicit_projection','xlir_callable_contextual_generator')
$files = @($PSCommandPath,$ContractDriver,$manifest.Driver,'test/cases/xlir_callable_contextual_host.c',
    'test/cases/xlir_callable_contextual_contract.c','test/cases/xlir_owner_consumption_contract.c',$Tcc)
$files += $resident
$files += @($fixtures | ForEach-Object { "test/cases/$_.xl" })
$records = @($manifest.Inputs) + @($manifest.Headers) + @($runtime)
$records += @($files | ForEach-Object { $item = Get-Item -LiteralPath $_; [pscustomobject]@{Path=$item.FullName;Sha256=(Get-FileHash -LiteralPath $item.FullName).Hash} })
if (@($records | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Stale source inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$publicResults = @(); $lanes = @()
& $ContractDriver *> "$outputPath/contract.log"
$contractExit = $LASTEXITCODE
foreach ($fixture in $fixtures) {
    $prefix = "$outputPath/$fixture"
    $phases = [ordered]@{Fixture=$fixture;Emit=$null;Repeat=$null;Run=$null;Module=$null;Deterministic=$false}
    foreach ($mode in @('emit','repeat','run','module')) {
        $operation = if ($mode -eq 'repeat') { 'emit' } else { $mode }
        $target = if ($mode -eq 'repeat') { "$prefix.repeat" } else { $prefix }
        & $manifest.Driver "test/cases/$fixture.xl" $operation $target *> "$prefix.$mode.log"
        $phases[$mode] = $LASTEXITCODE
        if ($phases[$mode] -eq 0 -and !(Select-String -LiteralPath "$prefix.$mode.log" -SimpleMatch "public source $operation complete" -Quiet)) {
            $phases[$mode] = 'missing-completion'
        }
    }
    if ($phases.Emit -eq 0 -and $phases.Repeat -eq 0) {
        $phases.Deterministic = $true
        foreach ($suffix in @('c','h','json')) {
            if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { $phases.Deterministic = $false }
        }
    }
    $publicResults += [pscustomobject]$phases
}
$include = [IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
    '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation',"-I$include","-I$outputPath",'-I.')
$libraries = @('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
foreach ($fixture in @('xlir_callable_contextual_conversion','xlir_callable_contextual_generator')) {
  # Preinclude applies only to the host TU, not the separately built runtime.
  foreach ($optimization in @('O0','O2')) {
    $binary = "$outputPath/$fixture-gcc-$optimization.exe"
    $object = "$outputPath/$fixture-gcc-$optimization.o"
    & gcc @flags "-$optimization" -DXLIR_CONTEXTUAL_GENERATED_PREINCLUDED -include "$outputPath/$fixture.c" -c test/cases/xlir_callable_contextual_host.c -o $object *> "$binary.build.log"
    $buildExit = $LASTEXITCODE; $runExit = $null
    if ($buildExit -eq 0) {
        & gcc @flags "-$optimization" $object @resident $runtime[0].Path @libraries -o $binary *>> "$binary.build.log"
        $buildExit = $LASTEXITCODE
    }
    if ($buildExit -eq 0) { & $binary *> "$binary.run.log"; $runExit = $LASTEXITCODE }
    $lanes += [pscustomobject]@{Fixture=$fixture;Lane="gcc-$optimization";Build=$buildExit;Run=$runExit}
  }
$binary = "$outputPath/$fixture-tcc.exe"
$object = "$outputPath/$fixture-tcc.o"
& $Tcc -std=c11 "-I$include" "-I$outputPath" -I. -DXLIR_CONTEXTUAL_GENERATED_PREINCLUDED -include "$outputPath/$fixture.c" -c test/cases/xlir_callable_contextual_host.c -o $object *> "$binary.build.log"
$buildExit = $LASTEXITCODE; $runExit = $null
if ($buildExit -eq 0) {
    & $Tcc -std=c11 "-I$include" -I. $object @resident tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libraries -o $binary *>> "$binary.build.log"
    $buildExit = $LASTEXITCODE
}
if ($buildExit -eq 0) { & $binary *> "$binary.run.log"; $runExit = $LASTEXITCODE }
$lanes += [pscustomobject]@{Fixture=$fixture;Lane='tcc';Build=$buildExit;Run=$runExit}
}
$unchanged = @($records | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$records;InputsUnchanged=$unchanged;
    ContractExit=$contractExit;Public=$publicResults;Lanes=$lanes;ModuleCyclesPerLane=100;
    Scope='Non-null callable contextual declaration, assignment, argument and return; actual async/finally and TypeError paths, exact per-cycle memory balance. Busy executor destruction is retried while code stays resident; not an automatic module pin test.'}
[IO.File]::WriteAllText("$outputPath/results.json", ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
$publicResults | Format-Table
$lanes | Format-Table
if (!$unchanged -or $contractExit -ne 0 -or
    @($publicResults | Where-Object { !$_.Deterministic -or $_.Run -ne 0 -or $_.Module -ne 0 }).Count -or
    @($lanes | Where-Object { $_.Build -ne 0 -or $_.Run -ne 0 }).Count) { throw 'Contextual callable batch failed; see all logs' }

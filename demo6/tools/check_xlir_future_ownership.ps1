param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc = 'E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new evidence directory' }
$manifest=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
if (!$manifest.SourceXrt) { throw 'Future graphs require current source XRT' }
$runtimeObject=@($manifest.Objects|Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
if ($runtimeObject.Count -ne 1) { throw 'Ambiguous current XRT object' }
$runtimeSources=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
    'src/runtime/xl6_continuation_future.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
$files=@(Get-Item $PSCommandPath,$manifest.Driver,test/cases/xlir_future_ownership.xl,
    test/cases/xlir_future_ownership_host.c,test/cases/xlir_future_payload_contract.c)
$records=@($manifest.Inputs)+@($manifest.Headers)+@($runtimeObject)+
    @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash $_.FullName).Hash}})
if ((Get-FileHash $manifest.Driver).Hash -ne $manifest.DriverSha256 -or
    @($records|Where-Object {(Get-FileHash $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale Future graph inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$prefix=Join-Path $outputPath 'xlir_future_ownership'
foreach ($pass in @('','.repeat')) {
    & $manifest.Driver test/cases/xlir_future_ownership.xl emit "$prefix$pass" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Future graph emission failed' }
}
foreach ($suffix in @('c','h','json')) {
    if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { throw "Nondeterministic Future $suffix" }
}
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
    '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation',"-I$includePath","-I$outputPath",'-I.')
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$results=@()
foreach ($lane in @('gcc-O0','gcc-O2','tcc')) {
    foreach ($fixture in @('graphs','resident-payload')) {
        $name="$lane-$fixture"; $binary="$outputPath/$name.exe"
        $sources=if ($fixture -eq 'graphs') { @('test/cases/xlir_future_ownership_host.c')+$runtimeSources }
            else { @('test/cases/xlir_future_payload_contract.c')+$runtimeSources }
        if ($lane -eq 'tcc') {
            & $Tcc -std=c11 "-I$includePath" "-I$outputPath" -I. @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"
        } else {
            $optimization=if ($lane -eq 'gcc-O0') {'-O0'} else {'-O2'}
            & gcc @strict $optimization @sources $runtimeObject[0].Path @libs -o $binary *> "$outputPath/$name.build.log"
        }
        $buildExit=$LASTEXITCODE; $runExit=$null; $complete=$false
        if ($buildExit -eq 0) {
            & $binary *> "$outputPath/$name.run.log"; $runExit=$LASTEXITCODE
            $marker=if ($fixture -eq 'graphs') {'Future ownership: 700 generated graphs'} else {'Future payload: 1500 accepted/rejected/empty cases'}
            $complete=[bool](Select-String -LiteralPath "$outputPath/$name.run.log" -SimpleMatch $marker -Quiet)
        }
        $results+=[pscustomobject]@{Lane=$lane;Fixture=$fixture;Build=$buildExit;Run=$runExit;Complete=$complete}
        Write-Output "$name build=$buildExit run=$runExit complete=$complete"
    }
}
$unchanged=@($records|Where-Object {(Get-FileHash $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$records;InputsUnchanged=$unchanged;Deterministic=$true;
    Lanes=$results;GraphsPerLane=700;ResidentCasesPerLane=1500;
    Scope='Real generated ready/task/resumed/continuation results, task.spawn and captured Future graphs; strict C, resident payloads, native aliases and exact memory. The host-controlled pending scalar frame has precise queue-root accounting; a real opaque native Watch still rejects atomically. Other graphs are inspected after worker join, code remains mapped: no general worker safepoint, automatic module pin or unload permission claim.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or @($results|Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) { throw 'Future ownership batch failed; inspect all logs' }

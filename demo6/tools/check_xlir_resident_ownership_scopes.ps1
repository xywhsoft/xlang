param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Preserve prior resident-scope evidence' }
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if (!$build.SourceXrt -or $runtime.Count -ne 1) { throw 'Current source runtime required' }
    $sources=@('test/cases/xlir_resident_ownership_scope_contract.c','src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_finalizer.c',
        'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source)+
        @(Get-Item -LiteralPath $sources)
    $inputs=@($build.Inputs)+@($build.Headers)+@($runtime)+@($files | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale resident-scope inputs' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $markers=@(
        'Resident ownership scopes: 100 frozen lease/stack/closure transitions, 50 in-flight callback tails, 50 allocator-held factory transfers, 10 real pending invocations; exact graph/count/allocator balance, no native root registration; not module collection',
        'Continuation ownership scopes: 100 frozen queue/transport transitions, 10 actual pending dispatches, 10 physical retirement tails with after-Close child escape; exact credits/code roots/allocator balance; not module collection')
    $lanes=@(); $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    foreach ($name in @('gcc-O0','gcc-O2','tcc')) {
        $lane="$outputPath/$name"; $timer=[Diagnostics.Stopwatch]::StartNew()
        if ($name -eq 'tcc') {
            # TCC cannot consume GCC COFF objects. Compile the actual source
            # implementation, and import the standard C99 conversions from UCRT.
            & $Tcc -std=c11 -Wall -Werror -I. @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o "$lane.exe" *> "$lane.build.log"
        } else {
            & gcc -std=c11 -Wall -Wextra -Werror "-$($name.Substring(4))" -I. @sources $runtime[0].Path @libs -o "$lane.exe" *> "$lane.build.log"
        }
        $buildExit=$LASTEXITCODE; $runExit=$null; $complete=$false
        if ($buildExit -eq 0) {
            $process=Start-Process -FilePath "$lane.exe" -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$lane.out.log" -RedirectStandardError "$lane.err.log"
            if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $runExit=-999 }
            else { $process.Refresh(); $runExit=$process.ExitCode }
            $process.Dispose()
            # An assertion can leave an empty stdout file. Keep that failed
            # lane and run the remaining compilers instead of aborting parsing.
            $output=[IO.File]::ReadAllText("$lane.out.log")
            $complete=@($markers | Where-Object {[regex]::Matches($output,[regex]::Escape($_)).Count -ne 1}).Count -eq 0
        }
        $timer.Stop()
        $lanes+=[pscustomobject]@{Name=$name;Build=$buildExit;Run=$runExit;Complete=$complete;Seconds=[Math]::Round($timer.Elapsed.TotalSeconds,3)}
        Write-Output "$name build=$buildExit run=$runExit complete=$complete"
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Lanes=$lanes;Markers=$markers;PerProgram=[pscustomobject]@{FrozenLeaseTransitions=100;CallbackTails=50;FactoryTransfers=50;
            PendingInvocations=10;FrozenQueueTransitions=100;PendingDispatches=10;RetirementChildEscapes=10};
        Scope='Current resident lease/closure/class/continuation ownership transitions and real pending activity roots. Native Windows GCC O0/O2 and standalone source TCC/UCRT. Custom test coordination is borrowed, not an owning graph edge. No Linux, release/VFS rebuild, full graph participation or module collection claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) {
        throw 'Resident ownership scope batch collected failures; inspect every lane'
    }
} finally { Pop-Location }

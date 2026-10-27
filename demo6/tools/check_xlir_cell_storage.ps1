param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Preserve prior cell storage evidence' }
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if (!$public.SourceXrt -or $runtime.Count -ne 1) { throw 'Current source XRT is required' }
    $driverNames=@('xlir_cell_storage_contract','xlir_cell_allocation_outcome_contract',
        'xlir_cell_thread_lifetime_contract','xlir_capture_ownership_contract')
    $drivers=@{}
    foreach ($name in $driverNames) {
        $drivers[$name]=[IO.Path]::GetFullPath((Join-Path $ContractDirectory "$name.exe"))
        if (@($contracts.Drivers | Where-Object Path -eq $drivers[$name]).Count -ne 1) { throw "Unrecorded contract: $name" }
    }
    $residentSources=@('src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c','src/runtime/xl6_time_core.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,$public.Driver)+
        @(Get-ChildItem test/cases -File | Where-Object {$_.Name -match '^xlir_(cell_|capture_ownership|closure_mutable_xvalue_capture|frame_ownership)'})
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale cell storage inputs' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $closure=Get-XlirResidentClosure -Root $rootPath -Directory "$outputPath/resident-closure" -Sources $residentSources -NativeProvider 'tools/xrt_single_header_impl.c' -PublicDirectory $PublicDirectory -Fixtures @(
        'test/cases/xlir_cell_allocation_outcome.xl','test/cases/xlir_cell_allocation_suspension.xl',
        'test/cases/xlir_cell_thread_lifetime.xl','test/cases/xlir_capture_ownership.xl')
    $residentSources=@($closure.Sources);$inputs+=@($closure.Inputs)
    $checks=@(); $emission=@(); $deterministic=$true
    $irMarker='Resident cell IR: 5 original fixtures, 20 rejected C/header-gated mutations; 22 allocations, 22 initializations, 14 owning exchanges; explicit text/JSON ABI and no generated lifecycle replica'
    & $drivers.xlir_cell_storage_contract *> "$outputPath/storage-ir.log"
    $checks += [pscustomobject]@{Name='storage';Exit=$LASTEXITCODE;Complete=[bool](Select-String -LiteralPath "$outputPath/storage-ir.log" -SimpleMatch $irMarker -Quiet)}
    & $drivers.xlir_capture_ownership_contract *> "$outputPath/capture-ir.log"
    $checks += [pscustomobject]@{Name='capture';Exit=$LASTEXITCODE;Complete=[bool](Select-String -LiteralPath "$outputPath/capture-ir.log" -SimpleMatch 'capture ownership: 10 typed/lowered captures, 22 mutations rejected before C/header emission' -Quiet)}
    foreach ($name in @('xlir_cell_allocation_outcome','xlir_cell_thread_lifetime')) {
        foreach ($repeat in @('','.repeat')) {
            $prefix="$outputPath/$name$repeat"
            & $drivers["${name}_contract"] "$prefix.c" "$prefix.h" "$prefix.json" *> "$prefix.emit.log"
            $exitCode=$LASTEXITCODE
            $markers=if ($name -eq 'xlir_cell_allocation_outcome') {
                @('cell allocation entries: 14 checked allocations/installations, 7 parameter cells, 224 rejected C/header-gated mutations',
                  'cell suspension: 3 checked cell allocations, 3 rejected mutations')
            } else { @('cell thread lifetime: 6 shared captures, 3 source cells, 9 C/header-gated mutations') }
            $text=[IO.File]::ReadAllText("$prefix.emit.log")
            $complete=@($markers | Where-Object {[regex]::Matches($text,[regex]::Escape($_)).Count -ne 1}).Count -eq 0
            $emission += [pscustomobject]@{Name=$name;Pass=$repeat;Exit=$exitCode;Complete=$complete}
        }
        $suffixes=@('c','h','json','h.bind.h')
        if ($name -eq 'xlir_cell_allocation_outcome') { $suffixes+=@('c.suspension.c','c.suspension.h','c.suspension.json') }
        foreach ($suffix in $suffixes) {
            if (!(Test-Path "$outputPath/$name.$suffix") -or !(Test-Path "$outputPath/$name.repeat.$suffix") -or
                (Get-FileHash "$outputPath/$name.$suffix").Hash -ne (Get-FileHash "$outputPath/$name.repeat.$suffix").Hash) { $deterministic=$false }
        }
    }
    foreach ($repeat in @('','.repeat')) {
        & $public.Driver test/cases/xlir_capture_ownership.xl emit "$outputPath/xlir_capture_ownership$repeat" *> "$outputPath/capture$repeat.emit.log"
        $emission += [pscustomobject]@{Name='xlir_capture_ownership';Pass=$repeat;Exit=$LASTEXITCODE;Complete=($LASTEXITCODE -eq 0)}
    }
    foreach ($suffix in @('c','h','json')) {
        if (!(Test-Path "$outputPath/xlir_capture_ownership.$suffix") -or !(Test-Path "$outputPath/xlir_capture_ownership.repeat.$suffix") -or
            (Get-FileHash "$outputPath/xlir_capture_ownership.$suffix").Hash -ne (Get-FileHash "$outputPath/xlir_capture_ownership.repeat.$suffix").Hash) { $deterministic=$false }
    }
    $includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $includes=@("-I$includePath","-I$outputPath",'-I.','-Irelease','-Irelease/include')
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $runtimeMarker='Resident cells: 450 typed lifetime/exchange/graph families, 500 actual allocation refusals, 250 frozen init/store/exchange/ref transitions, 50 in-flight payload finalizers; actual counts and exact allocator balance, no generated descriptors or native root registration'
    $cases=@(
        @{Name='resident';Sources=@('test/cases/xlir_cell_storage_runtime.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c');Pattern=[regex]::Escape($runtimeMarker)},
        @{Name='allocation';Sources=@('test/cases/xlir_cell_allocation_outcome_runtime.c','test/cases/xlir_cell_allocation_outcome_host.c')+$residentSources;
            Pattern='cell allocation entries: 13 contexts, 260 baseline runs, 1800 real allocation failures \(1200 creation, 540 invocation, 60 caught\), 9700 balanced allocations'},
        @{Name='suspension';Sources=@('test/cases/xlir_cell_allocation_suspension_host.c')+$residentSources;Pattern=''},
        @{Name='thread';Sources=@('test/cases/xlir_cell_thread_lifetime_runtime.c','test/cases/xlir_cell_thread_lifetime_host.c')+$residentSources;
            Pattern='cell thread lifetime: 8 workers, 800000 raw retain/release operations, 48240 source readers, 30 final-owner races, [1-9][0-9]* balanced allocations'},
        @{Name='capture';Sources=@('test/cases/xlir_capture_ownership_host.c')+$residentSources;
            Pattern='capture ownership: 700 generated graphs, aliases, calls and exact memory balance complete'})
    $residentClosure=Get-XlirResidentClosure -Root $rootPath -Directory "$outputPath/cell-closure" -Sources @($cases[0].Sources|Where-Object {$_-like'src/*'}) -NativeProvider 'tools/xrt_single_header_impl.c'
    $cases[0].Sources=@($cases[0].Sources|Where-Object {$_-notlike'src/*'})+@($residentClosure.Sources)
    $inputs+=@($residentClosure.Inputs)
    $lanes=@()
    foreach ($case in $cases) {
        foreach ($compiler in @('gcc-O0','gcc-O2','tcc')) {
            $name="$($case.Name)-$compiler"; $binary="$outputPath/$name.exe"; $timer=[Diagnostics.Stopwatch]::StartNew()
            if ($compiler -eq 'tcc') {
                $buildExit=Invoke-XlirTccToLog $Tcc (@('-std=c11')+$includes+@($case.Sources)+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
            } else {
                $buildExit=Invoke-XlirGccToLog ($strict+@("-$($compiler.Substring(4))")+$includes+@($case.Sources)+@($runtime[0].Path)+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
            }
            $runExit=$null; $complete=$false
            if ($buildExit -eq 0) {
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
                $null = $process.Handle
                if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $runExit=-999 }
                else { $process.Refresh(); $runExit=$process.ExitCode }
                $process.Dispose()
                $output=[IO.File]::ReadAllText("$outputPath/$name.out.log")
                $complete=$runExit -eq 0 -and (!$case.Pattern -or [regex]::Matches($output,$case.Pattern).Count -eq 1)
            }
            $timer.Stop()
            $lanes += [pscustomobject]@{Name=$name;Build=$buildExit;Run=$runExit;Complete=$complete;Seconds=[Math]::Round($timer.Elapsed.TotalSeconds,3)}
            Write-Output "$name build=$buildExit run=$runExit complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Contracts=$checks;Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;IrMarker=$irMarker;RuntimeMarker=$runtimeMarker;
        PerRuntimeProgram=[pscustomobject]@{Families=400;AllocationRefusals=450;FrozenTransitions=250;InFlightFinalizers=50};
        Scope='Resident cell V1, source-generated original allocation/suspension/thread/capture fixtures, full original fault sweep and exact physical graph/count/allocator assertions. Native Windows GCC O0/O2 and standalone source TCC/UCRT. No release/JIT/AOT host lane, module cycle collection, concurrent-variable lock, outer frame/global slot freeze or whole-graph certification claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$deterministic -or @($checks | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) {
        throw 'Cell storage batch collected failures; inspect every original population'
    }
} finally { Pop-Location }

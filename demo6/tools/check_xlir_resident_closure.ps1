param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Keep prior closure evidence; choose a fresh directory' }
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if (!$build.SourceXrt -or $runtime.Count -ne 1) { throw 'A current source runtime is required' }
    $fixtures=@(
        @{Name='cgen_closure';Native='test/cases/cgen_class_host.c'},
        @{Name='cgen_typed_container_functions';Native='test/cases/cgen_class_host.c'},
        @{Name='cgen_closure_ownership';Native=''},
        @{Name='cgen_optional_nullable_function_default';Native=''},
        @{Name='cgen_function_value_error_propagation';Native=''}
    )
    $files=@(Get-Item $PSCommandPath,src/runtime/xl6_closure.c,src/runtime/xl6_cell.c,src/runtime/xl6_slot.c,src/runtime/xl6_frame_storage.c,src/runtime/xl6_continuation_frame.c,src/runtime/xl6_future_wait.c,src/runtime/xl6_task.c,src/runtime/xl6_task_frame.c,src/runtime/xl6_task_pool.c,src/runtime/xl6_capture.c,src/runtime/xl6_value_iterator.c,src/runtime/xl6_future_callback.c,src/runtime/xl6_generator.c,src/runtime/xl6_closure.h,
        src/runtime/xl6_code_lease.c,src/runtime/xl6_code_lease.h,
        test/cases/xlir_resident_closure_contract.c,test/cases/xlir_resident_closure_dll.c,
        test/cases/xlir_resident_closure_dll.h,test/cases/xlir_closure_storage_contract.c,
        test/cases/cgen_class_host.c)
    $files+=@($fixtures | ForEach-Object {Get-Item "test/cases/$($_.Name).xl"})
    $inputs=@($build.Inputs)+@($build.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale closure inputs' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $roots=@($files|Where-Object {$_.Extension-eq'.c'-and$_.FullName.Replace('\','/').Contains('/src/runtime/')}|ForEach-Object {(Get-XlirContainedRelativePath $rootPath $_.FullName).Replace('\','/')})
    $closure=Get-XlirResidentClosure -Root $rootPath -Directory "$outputPath/resident-closure" -Sources $roots -NativeProvider 'tools/xrt_single_header_impl.c' -PublicDirectory $PublicDirectory -Fixtures @($fixtures|ForEach-Object {"test/cases/$($_.Name).xl"})
    $resident=@($closure.Sources);$inputs+=@($closure.Inputs)
    & $ContractDriver *> "$outputPath/ir.log"
    $irExit=$LASTEXITCODE
    $irComplete=[bool](Select-String -LiteralPath "$outputPath/ir.log" -SimpleMatch 'Resident closure IR: 4 source fixtures, 24 rejected C/header-gated mutations, text/JSON and resident-only control storage' -Quiet)
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Itest/cases')
    $libs=@('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32')
    $lanes=@()
    foreach ($level in @(0,2)) {
        foreach ($name in @('producer','adapter')) {
            $dllExit=Invoke-XlirGccToLog ($strict+@("-O$level",'-shared','test/cases/xlir_resident_closure_dll.c','-o',"$outputPath/$name-O$level.dll")) "$outputPath/$name-O$level.build.log"
            if ($dllExit -ne 0) { throw 'Resident closure DLL build failed' }
        }
        $lane="$outputPath/closure-O$level"
        $buildExit=Invoke-XlirGccToLog ($strict+@("-O$level",'test/cases/xlir_resident_closure_contract.c')+$resident+@($runtime[0].Path)+$libs+@('-o',"$lane.exe")) "$lane.build.log"
        $runExit=$null; $complete=$false; $oomComplete=$false
        if ($buildExit -eq 0) {
$process=Start-Process -FilePath "$lane.exe" -WindowStyle Hidden -PassThru -ArgumentList @('"'+"$outputPath/producer-O$level.dll"+'"','"'+"$outputPath/adapter-O$level.dll"+'"') -RedirectStandardOutput "$lane.out.log" -RedirectStandardError "$lane.err.log"
            $null = $process.Handle
            if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $runExit=-999 }
            else { $process.Refresh(); $runExit=$process.ExitCode }
            $process.Dispose()
            $complete=[bool](Select-String -LiteralPath "$lane.out.log" -SimpleMatch 'Resident closures: 50 distinct A/B DLL invocation lifetimes, 25 dual-owner cyclic graph transitions, 100000 native retain/release pairs, callback-return-before-unmap and allocator balance' -Quiet)
            $oomComplete=[bool](Select-String -LiteralPath "$lane.out.log" -SimpleMatch 'Resident closure OOM: 50 create refusals, 100 box refusals across 4 allocation positions, 50 successes, borrowed inputs and primary diagnostics preserved' -Quiet)
        }
        $lanes+=[pscustomobject]@{Name="closure-O$level";Build=$buildExit;Run=$runExit;Complete=$complete;OomComplete=$oomComplete}
        Write-Output "closure-O$level build=$buildExit run=$runExit complete=$complete oom=$oomComplete"
    }
    $language=@(); $deterministic=$true
    foreach ($fixture in $fixtures) {
        foreach ($phase in @('emit','repeat','run','module')) {
            $mode=if ($phase -eq 'repeat') {'emit'} else {$phase}
            $target="$outputPath/$($fixture.Name)-$phase"
            $arguments=@("test/cases/$($fixture.Name).xl",$mode,$target)
            if ($fixture.Native) { $arguments+=$fixture.Native }
            & $build.Driver @arguments *> "$target.log"
            $code=$LASTEXITCODE
            $complete=[bool](Select-String -LiteralPath "$target.log" -SimpleMatch "public source $mode complete" -Quiet)
            $language+=[pscustomobject]@{Fixture=$fixture.Name;Phase=$phase;Exit=$code;Complete=$complete}
            Write-Output "$($fixture.Name) $phase exit=$code complete=$complete"
        }
        foreach ($extension in @('c','h','json')) {
            $a="$outputPath/$($fixture.Name)-emit.$extension"; $b="$outputPath/$($fixture.Name)-repeat.$extension"
            if (!(Test-Path -LiteralPath $a) -or !(Test-Path -LiteralPath $b) -or (Get-FileHash $a).Hash -ne (Get-FileHash $b).Hash) { $deterministic=$false }
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        IrExit=$irExit;IrComplete=$irComplete;Lanes=$lanes;Language=$language;Deterministic=$deterministic;
        Scope='Resident closure control, ordinary invocation scopes, distinct producer/adapter leases and callbacks. Synthetic graph inspection uses caller-provided whole-graph quiescence. Does not prove production cycle retirement, continuation scope inheritance, raw native callback ownership or every escaping language/thread adapter.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$deterministic -or $irExit -ne 0 -or !$irComplete -or
        @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete -or !$_.OomComplete}).Count -or
        @($language | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count) { throw 'Resident closure batch collected failures; inspect all lanes' }
} finally { Pop-Location }

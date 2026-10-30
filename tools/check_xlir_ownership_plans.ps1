param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior ownership-plan evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$build.SourceXrt -or $runtime.Count -ne 1){throw 'Current complete source XRT object required'}
    $programs=@(
        @{Name='native-finish';Source='test/cases/xlir_ownership_native_finish_runtime.c';Runtime='src/runtime/xl6_ownership_plan.c';
            Marker='Native completion ABI: 200 physical cycles, 800 BUSY turns, 100 diagnostic-free failures, 100 restored registrations; exact native descriptors, all pins retained, zero live delta'},
        @{Name='finish';Source='test/cases/xlir_ownership_finish_runtime.c';Runtime='src/runtime/xl6_ownership_plan.c';
            Marker='Ownership completion: 501 real cycles, 900 BUSY turns, 200 diagnostic-free failures, 100 invalid outcomes, 400 pre-trace descriptor refusals, 2 rejected reentries; exact pins, no repeated completed duty, zero live delta'},
        @{Name='preparation';Source='test/cases/xlir_ownership_preparation_runtime.c';Runtime='src/runtime/xl6_ownership_plan.c';
            Marker='Ownership preparation: 150 actual cycles, 600 pre-trace descriptor refusals, 150 busy paths, 50 explicit preparation failures, 25 resurrections, 50 new preparation duties, 200 notifications before 300 finalizers, 200 cross-thread freezes and zero live delta'},
        @{Name='plan';Source='test/cases/xlir_ownership_plan_runtime.c';Runtime='src/runtime/xl6_ownership_plan.c';
            Marker='Ownership plans: 400 physical cycles across commit/resurrection/new-node/finalizer-error paths, 100 busy and 100 admission refusals, 140 real preparation faults; 1632 allocations retired, zero live delta; adapter engine, not module acceptance'},
        @{Name='value';Source='test/cases/xlir_ownership_value_plan_runtime.c';Runtime='src/runtime/xl6_ownership_plan.c';
            Marker='Value ownership plans: 1600 physical Array/IntMap/Object/Set COW transactions, 40000 concurrent quarantined promotions, 84 preparation/refresh/commit faults, 1696 real native owners retired; zero live delta; custom policies remain refused'},
        @{Name='tail';Source='test/cases/xlir_code_lease_retirement_tail.c';Runtime='src/runtime/xl6_code_lease.c';
            Marker='Code lease retirement tails: 100 direct and 100 scoped final releases, exclusive graph freezes and native worker joins, exact diagnostic and allocator balance'})
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'src/runtime/xl6_ownership_plan.h','src/runtime/xl6_ownership_plan.c','src/runtime/xl6_code_lease.h','src/runtime/xl6_code_lease.c',
        'test/support/ownership_test_thread.h')+
        @($programs | ForEach-Object {Get-Item -LiteralPath $_.Source})
    $inputs=@($build.Inputs)+@($build.Headers)+@($runtime)+@($files | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale ownership-plan inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $rows=@();$flags=@('-std=c11','-Wall','-Wextra','-Werror','-I.');$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        foreach($program in $programs){
            $name="$($program.Name)-$compiler";$binary="$outputPath/$name.exe";$timer=[Diagnostics.Stopwatch]::StartNew()
            if($compiler -eq 'tcc'){& $Tcc -std=c11 -Wall -Werror -I. $program.Source $program.Runtime tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"}
            else{& gcc @flags "-$($compiler.Substring(4))" $program.Source $program.Runtime $runtime[0].Path @libs -o $binary *> "$outputPath/$name.build.log"}
            $buildExit=$LASTEXITCODE;$run=$null;$complete=$false
            if(!$buildExit){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
                $null = $process.Handle
                if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
                $process.Dispose();$complete=$run -eq 0 -and [regex]::Matches([IO.File]::ReadAllText("$outputPath/$name.out.log"),[regex]::Escape($program.Marker)).Count -eq 1
                if($program.Name -eq 'preparation'){
                    $complete=$complete -and [bool](Select-String -LiteralPath "$outputPath/$name.out.log" -SimpleMatch -Pattern 'Preparation snapshot recovery: 25 real allocation failures after completed notification, distinct infrastructure outcome, no repeated semantic duty' -Quiet)
                }
                if($program.Name -eq 'plan'){
                    $complete=$complete -and [bool](Select-String -LiteralPath "$outputPath/$name.out.log" -SimpleMatch -Pattern 'Ownership preparation continuation: 96 real self-cycles' -Quiet)
                }
            }
            $timer.Stop();$rows += [pscustomobject]@{Name=$name;Build=$buildExit;Run=$run;Complete=$complete;Seconds=[Math]::Round($timer.Elapsed.TotalSeconds,3)}
            Write-Output "$name build=$buildExit run=$run complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$rows;
        PerProgram=[pscustomobject]@{NativeCycles=400;BusyRefusals=100;AdmissionRefusals=100;CreationFaults=140;
            ValueTransactions=1600;QuarantinedNativePromotions=40000;ValuePreparationRefreshCommitFaults=84;
            DirectHostReleaseTails=100;ScopedHostReleaseTails=100};
        Preparation=[pscustomobject]@{Cycles=150;DescriptorRefusals=600;BusyPaths=150;ExplicitFailures=50;
            Resurrections=25;NewDuties=50;Notifications=200;Finalizers=300;CrossThreadFreezes=200;PostNotificationAllocationFailures=25};
        Completion=[pscustomobject]@{Cycles=501;BusyTurns=900;DiagnosticFreeFailures=200;
            InvalidOutcomes=100;DescriptorRefusals=400;RejectedReentries=2};
        NativeCompletion=[pscustomobject]@{Cycles=200;BusyTurns=800;DiagnosticFreeFailures=100;RestoredRegistrations=100};
        Scope='Actual native cycles and ordinary XRT physical Value/COW adapters, real strong pins, pre-trace admission, exclusive claims, revalidation/clear, weak quarantine, resurrection/new-node/finalizer-error abort, real OOM, host freeze and thread-join tails. V2 prepares and revalidates before each semantic duty, including new resources created by a finalizer. Custom class/closure/native callback policies and actual module retirement are not exercised by this gate. Not full language or Linux/release verification.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or @($rows | Where-Object {!$_.Complete}).Count){throw 'Ownership-plan batch collected failures'}
} finally {Pop-Location}

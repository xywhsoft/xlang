param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
$environmentBefore=@{}
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve network lifetime evidence'}
    $publicPath=Join-Path $PublicDirectory 'build.json'
    $public=Get-Content -LiteralPath $publicPath -Raw|ConvertFrom-Json
    if(!$public.SourceXrt-or$public.RuntimeOnly){throw 'Current-source compiler and runtime required'}
    $cases=@(Get-Content -LiteralPath 'test/cases/xlir_network_lifetime_cases.json' -Raw|ConvertFrom-Json)
    if(!$cases.Count-or@($cases|ForEach-Object Fixture|Sort-Object -Unique).Count-ne$cases.Count){throw 'Cases must be explicit and unique'}
    $compiler=(Get-Command gcc).Source
    $hostPath=[IO.Path]::GetFullPath('test/cases/xlir_network_lifetime_host.c')
    $files=Get-Item -LiteralPath (@($PSCommandPath,$publicPath,$compiler,$hostPath,
        'test/cases/xlir_network_lifetime_cases.json','src/driver/xl6_xlc_shell.c',
        'tools/stage_xlir_artifact_runtime_sources.ps1',
        'release/examples/fixtures/tls_cert.pem','release/examples/fixtures/tls_key.pem')+
        @($cases|ForEach-Object {"test/cases/$($_.Fixture).xl"}))
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($r in $inputs){if((Get-FileHash -LiteralPath $r.Path).Hash-ne$r.Sha256){throw "Stale network input: $($r.Path)"}}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    $driverDirectory=Join-Path $outputPath 'driver'
    New-Item -ItemType Directory -Path $driverDirectory|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$driverDirectory/xl.dll"
    $objects=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
    $includes=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $flags=@('-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-I.','-Irelease','-Ilib/tcc',"-I$includes")
    $libs=@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $cli=Join-Path $driverDirectory 'xl.exe'
    & $compiler @flags src/driver/xl6_xlc_shell.c @objects @libs -o $cli *> "$driverDirectory/build.log"
    $cliBuild=$LASTEXITCODE
    $profile=[ordered]@{
        XL6_TCC_DLL=[IO.Path]::GetFullPath('release/xl.dll')
        XL6_TCC_ROOT=[IO.Path]::GetFullPath('res/tcc/win64/x86_64')
        XL6_TCC_LIB=[IO.Path]::GetFullPath('res/tcc/win64/x86_64/lib')
        XL6_TCC_INCLUDE=[IO.Path]::GetFullPath('lib/tcc/include')
        XL6_TCC_WIN_INCLUDE=[IO.Path]::GetFullPath('res/tcc/win64/x86_64/include')
        XL6_TCC_WINAPI_INCLUDE=[IO.Path]::GetFullPath('res/tcc/win64/x86_64/include/winapi')
        XL6_TCC_LIBTCC1=[IO.Path]::GetFullPath('res/tcc/win64/x86_64/lib/libtcc1.a')
    }
    foreach($key in $profile.Keys){
        if(!(Test-Path -LiteralPath $profile[$key])){throw "Missing explicit development provider $key"}
        $environmentBefore[$key]=[Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$profile[$key],'Process')
    }
    function Invoke-NetworkProcess([string]$Binary,[string[]]$Arguments,[string]$Working,[string]$Prefix,[string]$Marker){
        $exitCode=$null;$completed=$false
        if(Test-Path -LiteralPath $Binary){
            $start=[Diagnostics.ProcessStartInfo]::new()
            $start.FileName=$Binary;$start.WorkingDirectory=$Working
            $start.UseShellExecute=$false;$start.CreateNoWindow=$true
            $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
            foreach($argument in $Arguments){$start.ArgumentList.Add($argument)}
            $process=[Diagnostics.Process]::new();$process.StartInfo=$start
            try {
                [void]$process.Start()
                $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
                if($process.WaitForExit(45000)){$exitCode=$process.ExitCode}
                else{$process.Kill($true);$process.WaitForExit();$exitCode='timeout-45s'}
                $stdoutText=$stdout.GetAwaiter().GetResult();$stderrText=$stderr.GetAwaiter().GetResult()
                [IO.File]::WriteAllText("$Prefix.log",$stdoutText,[Text.UTF8Encoding]::new($false))
                [IO.File]::WriteAllText("$Prefix.err.log",$stderrText,[Text.UTF8Encoding]::new($false))
                $completed=[string]::IsNullOrEmpty($Marker)-or$stdoutText.Contains($Marker)
            } finally {$process.Dispose()}
        }
        [pscustomobject]@{Exit=$exitCode;CompletionMarker=$completed;Complete=($exitCode-eq0-and$completed);
            Executable=$Binary;Arguments=$Arguments;WorkingDirectory=$Working;Log="$Prefix.log";ErrorLog="$Prefix.err.log"}
    }
    $boundaryErrors=@();$results=@();$staged=$null
    foreach($source in @('src/cgen/xl6_cgen_net_transport.c','src/cgen/xl6_cgen_websocket_stream.c')){
        $text=Get-Content -LiteralPath $source -Raw
        if($text-match 'xrtEventDestroy\(&\w+->Done\)'){$boundaryErrors+="Embedded event storage is freed: $source"}
    }
    if($cliBuild-eq0){
        $identity=[pscustomobject]@{Driver=$cli;DriverSha256=(Get-FileHash -LiteralPath $cli).Hash}
        [IO.File]::WriteAllText("$driverDirectory/build.json",($identity|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
        & tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $driverDirectory *> "$driverDirectory/stage.log"
        $staged=Get-Content -LiteralPath "$driverDirectory/artifact_sources.json" -Raw|ConvertFrom-Json
        $inputs+=@($staged.Files)
        foreach($case in $cases){
            $source=[IO.Path]::GetFullPath("test/cases/$($case.Fixture).xl")
            $caseDirectory=Join-Path $outputPath $case.Fixture
            New-Item -ItemType Directory -Path $caseDirectory,"$caseDirectory/build/test","$caseDirectory/release/examples/fixtures"|Out-Null
            Copy-Item -LiteralPath 'release/examples/fixtures/tls_cert.pem','release/examples/fixtures/tls_key.pem' -Destination "$caseDirectory/release/examples/fixtures"
            Copy-Item -LiteralPath release/xl.dll -Destination "$caseDirectory/xl.dll"
            $jit=Invoke-NetworkProcess $cli @('run',$source,'--inc-path',$includes) $caseDirectory "$caseDirectory/jit" $case.Marker
            $executable=Join-Path $caseDirectory 'program.exe'
            $build=Invoke-NetworkProcess $cli @('build',$source,'--inc-path',$includes,'-o',$executable,'--save-temps') $rootPath "$caseDirectory/build" ''
            $runs=@(foreach($round in 1..3){Invoke-NetworkProcess $executable @() $caseDirectory "$caseDirectory/exe-$round" $case.Marker})
            $generated=$null;$hostBuild=$null;$hostRun=$null;$events=@();$summary=$null;$rejected=@()
            $log=[IO.File]::ReadAllText($build.ErrorLog)
            if($log-match 'temporary files kept: ([^\r\n]+)'){
                $generated=Join-Path $Matches[1] 'module.c'
                if(Test-Path -LiteralPath $generated){
                    $inputs+=[pscustomobject]@{Path=$generated;Sha256=(Get-FileHash -LiteralPath $generated).Hash}
                    $define='-DXLIR_NETWORK_GENERATED_C="'+$generated.Replace('\','/')+'"'
                    $hostFlags=$flags
                    if($case.BufferCommands){$hostFlags+='-DXLIR_NETWORK_BUFFER_COMMAND_PROBE'}
                    & $compiler @hostFlags $define $hostPath @objects @libs -o "$caseDirectory/lifetime.exe" *> "$caseDirectory/host-build.log"
                    $hostBuild=$LASTEXITCODE
                    if($hostBuild-eq0){
                        $hostRun=Invoke-NetworkProcess "$caseDirectory/lifetime.exe" @() $caseDirectory "$caseDirectory/lifetime" ''
                        $events=@(Get-Content -LiteralPath $hostRun.Log|Where-Object {$_-match '^\{"(?:Cycle|RejectedPosts)'}|ConvertFrom-Json)
                        $summary=@($events|Where-Object {$_.PSObject.Properties.Name-contains'Cycles'})|Select-Object -Last 1
                        $rejected=@($events|Where-Object {$_.PSObject.Properties.Name-contains'RejectedPosts'})
                    }
                }
            }
            $cycleRows=@($events|Where-Object {$_.PSObject.Properties.Name-contains'Cycle'})
            $memoryComplete=$hostBuild-eq0-and$hostRun.Complete-and$summary.Complete-and$cycleRows.Count-eq5-and
                !@($cycleRows|Where-Object {!$_.MemoryErrorsUnchanged-or!$_.ResourcesClosed-or$_.Exit-ne0}).Count-and
                (!$case.BufferCommands-or($rejected.Count-eq1-and$rejected[0].Complete-and$rejected[0].RejectedPosts-eq16))
            $results+=[pscustomobject]@{Fixture=$case.Fixture;Jit=$jit;Build=$build;Executions=$runs;Generated=$generated;
                HostBuild=$hostBuild;HostRun=$hostRun;Events=$events;Summary=$summary;MemoryComplete=[bool]$memoryComplete;
                Complete=($jit.Complete-and$build.Complete-and!@($runs|Where-Object {!$_.Complete}).Count-and$memoryComplete)}
            Write-Output "$($case.Fixture) JIT=$($jit.Exit) AOT=$($build.Exit) EXE=$(@($runs|ForEach-Object Exit)-join',') lifetime=$($hostRun.Exit) cycles=$($cycleRows.Count)"
        }
    }
    $unchanged=!@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count
    $complete=$unchanged-and$cliBuild-eq0-and!$boundaryErrors.Count-and$results.Count-eq$cases.Count-and!@($results|Where-Object {!$_.Complete}).Count
    $report=[pscustomobject]@{Complete=$complete;Inputs=$inputs;InputsUnchanged=$unchanged;CliBuild=$cliBuild;
        ProviderProfile=$profile;BoundaryErrors=$boundaryErrors;Results=$results;Rounds=3;CyclesPerProgram=5;RejectedPostsPerSelectedProgram=16;
        Scope='Current-source disk-provider JIT/AOT, three isolated EXE runs and five real saved-AOT public main lifecycles per program. Memory-debug accounting includes terminal host-thread cleanup and real stopped-worker rejection. Resident code only; not mixed graph or dynamic unloading, shared release, all protocols or platform acceptance.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
    if(!$complete){throw 'Network lifetime failures retained for grouped repair'}
} finally {
    foreach($key in $environmentBefore.Keys){[Environment]::SetEnvironmentVariable($key,$environmentBefore[$key],'Process')}
    Pop-Location
}

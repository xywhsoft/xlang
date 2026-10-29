param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve wait oracle evidence'}
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $runtime=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$public.SourceXrt-or$runtime.Count-ne1){throw 'Current source runtime required'}
    $case='test/cases/xlir_future_wait_oracle.c'
    $files=Get-Item -LiteralPath $PSCommandPath,$case,$Tcc,(Get-Command gcc).Source,'C:/Windows/System32/ucrtbase.dll'
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale wait oracle input: $($row.Path)"}}
    New-Item -ItemType Directory -Path $out|Out-Null
    $closure=Get-XlirResidentClosure -Root $rootPath -Directory "$out/resident-closure" -Sources @('src/runtime/xl6_future_wait.c') -NativeProvider 'tools/xrt_single_header_impl.c'
    # The oracle includes this exact production unit to observe its private
    # solver. Compile every transitive provider, and the solver only once.
    $resident=@($closure.Sources|Where-Object {$_-ne'src/runtime/xl6_future_wait.c'})
    $inputs+=@($closure.Inputs)
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $includes=@('-I.','-Irelease','-Irelease/include','-DXL_STATIC_EMBED');$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $marker='Future wait oracle: 66064 exhaustive graphs, 197648 root decisions, ordered duplicates, ALL/ANY open alternatives and feeders; 96-node/768-edge growth, 16 allocation budgets, 8 exact failures; memory balanced'
    $lanes=@()
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        $binary="$out/oracle-$compiler.exe"
        if($compiler-eq'tcc'){$build=Invoke-XlirTccToLog $Tcc (@('-std=c11')+$includes+@($case)+$resident+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)) "$binary.build.log"}
        else{$build=Invoke-XlirGccToLog ($strict+@("-$($compiler.Substring(4))")+$includes+@($case)+$resident+@($runtime[0].Path)+$libs+@('-o',$binary)) "$binary.build.log"}
        $run=$null;$complete=$false
        if($build-eq0){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$binary.out.log" -RedirectStandardError "$binary.err.log"
            $null = $process.Handle
            if($process.WaitForExit(45000)){$process.Refresh();$run=$process.ExitCode}else{$process.Kill();$process.WaitForExit();$run='timeout-45s'}
            $process.Dispose();$complete=$run-eq0-and[bool](Select-String -LiteralPath "$binary.out.log" -SimpleMatch $marker -Quiet)
        }
        $lanes+=[pscustomobject]@{Name=$compiler;Build=$build;Run=$run;Complete=$complete;Binary=$binary}
        Write-Output "$compiler wait oracle build=$build run=$run complete=$complete"
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $artifacts=@(Get-ChildItem -LiteralPath $out -File|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    $report=[pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;Artifacts=$artifacts;Marker=$marker;ResidentClosure=$closure;
        Scope='Production solver included directly, independent least-completion fixed point plus dense transitive closure oracle. All one/two/three-node rule/adjacency/open-input graphs, duplicate slots, every root and bounded scratch-growth failures. Does not mock or certify provider/claim/cancellation state; separate generated aggregate module gate supplies those proofs. GCC O2 optimizes solver/resident runtime; its native XRT object is the source O0 build. TCC builds all from source.'}
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if(!$unchanged-or$lanes.Count-ne3-or@($lanes|Where-Object {!$_.Complete}).Count){throw 'Wait oracle failures preserved'}
}finally{Pop-Location}

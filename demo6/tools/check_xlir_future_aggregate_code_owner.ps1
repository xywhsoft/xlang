param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve aggregate code owner evidence'}
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $runtime=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$public.SourceXrt-or$runtime.Count-ne1){throw 'Current source runtime required'}
    $resident=@($public.Sources|Where-Object {$_.StartsWith('src/runtime/')})+@('src/api/xl_future_bridge.c')
    $case='test/cases/xlir_future_aggregate_code_owner_runtime.c'
    $dllCase='test/cases/xlir_future_aggregate_code_owner_box.c'
    $files=Get-Item -LiteralPath $PSCommandPath,$case,$dllCase,'test/cases/xlir_future_aggregate_code_owner_box.h',
        $Tcc,(Get-Command gcc).Source,'C:/Windows/System32/ucrtbase.dll'
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale aggregate code owner input: $($row.Path)"}}
    New-Item -ItemType Directory -Path $out|Out-Null
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $includes=@('-I.','-Irelease','-DXL_STATIC_EMBED');$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $lanes=@();$marker='Aggregate code ownership: 100 real DLL parent/child lifetimes after Close, 100 cross-thread admission refusals, 500 scoped box calls, 300 fully admitted physical graphs, 100 legacy policy refusals; exact memory and no synthetic code roots'
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        $dll="$out/box-$compiler.dll";$binary="$out/owner-$compiler.exe"
        if($compiler-eq'tcc'){& $Tcc -std=c11 -shared @includes $dllCase -o $dll *> "$dll.build.log"}
        else{& gcc @strict "-$($compiler.Substring(4))" -shared @includes $dllCase -o $dll *> "$dll.build.log"}
        $dllExit=$LASTEXITCODE
        if($compiler-eq'tcc'){& $Tcc -std=c11 @includes $case @resident tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$binary.build.log"}
        else{& gcc @strict "-$($compiler.Substring(4))" @includes $case @resident $runtime[0].Path @libs -o $binary *> "$binary.build.log"}
        $build=$LASTEXITCODE;$run=$null;$complete=$false
        if($dllExit-eq0-and$build-eq0){
$process=Start-Process -FilePath $binary -ArgumentList @('"'+$dll+'"') -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$binary.out.log" -RedirectStandardError "$binary.err.log"
            $null = $process.Handle
            if($process.WaitForExit(45000)){$process.Refresh();$run=$process.ExitCode}else{$process.Kill();$process.WaitForExit();$run='timeout-45s'}
            $process.Dispose()
            $complete=$run-eq0-and[bool](Select-String -LiteralPath "$binary.out.log" -SimpleMatch $marker -Quiet)-and
                [bool](Select-String -LiteralPath "$binary.out.log" -SimpleMatch 'Aggregate owned mapper OOM: 128 budgets' -Quiet)-and
                [bool](Select-String -LiteralPath "$binary.out.log" -SimpleMatch 'Aggregate mapper code graph: 200 exact context-to-lease-to-owner chains' -Quiet)
        }
        $lanes+=[pscustomobject]@{Name=$compiler;DllBuild=$dllExit;Build=$build;Run=$run;Complete=$complete;Binary=$binary;Dll=$dll}
        Write-Output "$compiler aggregate code owner dll=$dllExit build=$build run=$run complete=$complete"
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $artifacts=@(Get-ChildItem -LiteralPath $out -File|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    $report=[pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$lanes;Artifacts=$artifacts;Marker=$marker;
        Scope='Actual resident bridge and DLL boxer parent/child scopes, after-Close same-thread inheritance and unrelated-thread refusal, complete physical adapter admission, legacy rejection, real unmapping, and owned-map array/direct OOM. GCC O2 optimizes resident code and boxer; native runtime object comes from strict O0 source build. Not language multi-input wait solving or generated module cycle retirement.'}
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    if(!$unchanged-or@($lanes|Where-Object {!$_.Complete}).Count){throw 'Aggregate code owner batch collected failures'}
}finally{Pop-Location}

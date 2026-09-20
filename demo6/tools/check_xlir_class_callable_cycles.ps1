param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve class/callable evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$build.SourceXrt -or $runtime.Count -ne 1){throw 'Current source runtime required'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_class_callable_plan_runtime.c')
    $inputs=@($build.Inputs)+@($build.Headers)+@($runtime)+@($files | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale class/callable inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $sources=@('test/cases/xlir_class_callable_plan_runtime.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c',
        'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_ownership_plan.c')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$rows=@()
    foreach($lane in @('gcc-O0','gcc-O2','tcc')){
        $binary="$outputPath/$lane.exe"
        if($lane -eq 'tcc'){& $Tcc -std=c11 -Wall -Werror -I. @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$lane.build.log"}
        else{& gcc -std=c11 -Wall -Wextra -Werror "-$($lane.Substring(4))" -I. @sources $runtime[0].Path @libs -o $binary *> "$outputPath/$lane.build.log"}
        $buildExit=$LASTEXITCODE;$run=$null;$complete=$false
        if(!$buildExit){
            & $binary *> "$outputPath/$lane.run.log";$run=$LASTEXITCODE
            $complete=$run -eq 0 -and [bool](Select-String -LiteralPath "$outputPath/$lane.run.log" -Pattern '^Class/callable plans: 450 exact physical graphs, 546 once-only receiver finalizers, 50 late duties, 96 preparation/commit probes with [1-9][0-9]* real failures, 50 native active pre-trace refusals; .* zero live delta$' -Quiet)
        }
        $rows+=[pscustomobject]@{Lane=$lane;Build=$buildExit;Run=$run;Complete=$complete}
        Write-Output "$lane build=$buildExit run=$run complete=$complete"
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    [IO.File]::WriteAllText("$outputPath/results.json",([pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$rows;
        Scope='Additive resident class/callable physical ownership and real native active admission; synthetic quiescent host. Does not replace original generated ten-mode module/object gates, all-transitive admission or Linux/release acceptance.'} | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or @($rows | Where-Object {!$_.Complete}).Count){throw 'Class/callable failures collected'}
} finally {Pop-Location}

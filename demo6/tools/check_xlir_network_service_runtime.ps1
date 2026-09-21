param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [ValidateRange(1,3600)][int]$RunTimeoutSeconds=180)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior network-service evidence'}
    $buildPath=Join-Path $PublicDirectory 'build.json'
    $build=Get-Content $buildPath -Raw|ConvertFrom-Json
    $runtime=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$build.SourceXrt-or$runtime.Count-ne 1){throw 'Current source native runtime required'}
    $gcc=(Get-Command gcc).Source;$tcc='E:/software/tcc/tcc.exe'
    $sources=@('test/cases/xlir_network_service_runtime.c','test/cases/xlir_network_service_contract.c')
    $files=Get-Item -LiteralPath (@($PSCommandPath,$buildPath,$gcc,$tcc,'C:/Windows/System32/ucrtbase.dll')+$sources)
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale service input $($row.Path)"}}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $rows=@();$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    foreach($name in @('gcc-O0','gcc-O2','tcc','ir-contract')){
        $binary="$outputPath/$name.exe";$timer=[Diagnostics.Stopwatch]::StartNew()
        if($name-eq'ir-contract'){
            $objects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
            & $gcc -std=c11 -O0 -Wall -Wextra -Werror -Wno-unused-function -DXL_STATIC_EMBED -I. $sources[1] @objects release/xl.dll @libs -o $binary *> "$outputPath/$name.build.log"
        } elseif($name-eq'tcc'){
            & $tcc -std=c11 -Wall -Werror -I. $sources[0] src/runtime/xl6_network_service.c tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"
        } else {
            & $gcc -std=c11 "-$($name.Substring(4))" -Wall -Wextra -Werror -I. $sources[0] src/runtime/xl6_network_service.c $runtime[0].Path @libs -o $binary *> "$outputPath/$name.build.log"
        }
        $buildExit=$LASTEXITCODE;$run=$null
        if(!$buildExit){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -WorkingDirectory $rootPath -PassThru -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
            $null = $process.Handle
            if(!$process.WaitForExit($RunTimeoutSeconds*1000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
            $process.Dispose()
        }
        $timer.Stop();$rows+=[pscustomobject]@{Name=$name;Build=$buildExit;Run=$run;Complete=$buildExit-eq 0-and$run-eq 0;Seconds=[Math]::Round($timer.Elapsed.TotalSeconds,3)}
        Write-Output "$name build=$buildExit run=$run"
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq 0
    $report=[pscustomobject]@{Complete=$unchanged-and@($rows|Where-Object {!$_.Complete}).Count-eq 0;Inputs=$inputs;InputsUnchanged=$unchanged;RunTimeoutSeconds=$RunTimeoutSeconds;Results=$rows;
        Scope='Resident service lifecycle and verified IR contract only. No complete native service ownership graph or release acceptance claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$report.Complete){throw 'Collected network-service contract/runtime failures'}
} finally {Pop-Location}

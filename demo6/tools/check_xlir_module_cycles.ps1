param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior module-cycle evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$build.SourceXrt -or $runtime.Count -ne 1){throw 'Current source runtime required'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_module_cycle_transactions.c','test/cases/xlir_closure_layout_plan_runtime.c',
        'test/cases/xlir_module_ownership.xl','test/cases/xlir_module_ownership_artifact.c',
        'test/cases/xlir_one_shot_tcc_host.c','test/cases/xlir_async_module_lifetime_host.c')
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale module-cycle inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $outputPath 'xl.dll')
    $runtimeInclude=Join-Path $outputPath '__xl_runtime'
    New-Item -ItemType Directory -Path $runtimeInclude | Out-Null
    foreach($header in $build.Headers | Where-Object Path -match '[/\\]__xl_runtime[/\\]'){
        Copy-Item -LiteralPath $header.Path -Destination (Join-Path $runtimeInclude ([IO.Path]::GetFileName($header.Path)))
    }
    $prefix=Join-Path $outputPath 'xlir_module_ownership.generated'
    & $ContractDriver test/cases/xlir_module_ownership.xl "$prefix.c" "$prefix.h" "$prefix.json" --script *> "$outputPath/emit.log"
    if($LASTEXITCODE){throw 'Cycle artifact emission failed'}
    $rows=@();$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-I.')
    $resident=@('src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_code_lease.c','src/runtime/xl6_ownership_plan.c')
    foreach($lane in @('gcc-O0','gcc-O2','tcc')){
        $name="layout-$lane";$binary="$outputPath/$name.exe"
        if($lane -eq 'tcc'){& $Tcc -std=c11 -Wall -Werror -I. test/cases/xlir_closure_layout_plan_runtime.c @resident tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"}
        else{& gcc @flags "-$($lane.Substring(4))" test/cases/xlir_closure_layout_plan_runtime.c @resident $runtime[0].Path @libs -o $binary *> "$outputPath/$name.build.log"}
        $buildExit=$LASTEXITCODE;$run=$null;$complete=$false
        if(!$buildExit){
            & $binary *> "$outputPath/$name.run.log";$run=$LASTEXITCODE
            $complete=$run -eq 0 -and [bool](Select-String -LiteralPath "$outputPath/$name.run.log" -Pattern '^Resident layout plans: 400 .* 496 physical owners freed; 500 value bridges with 200 faults; .* zero live delta$' -Quiet)
        }
        $rows+=[pscustomobject]@{Lane=$name;Build=$buildExit;Run=$run;Complete=$complete}
        Write-Output "$name build=$buildExit run=$run complete=$complete"
    }
    $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $excluded} | ForEach-Object Path)
    $hostFlags=$flags+@('-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-pointer-sign',
        '-Wno-format-truncation','-Wno-missing-field-initializers','-Irelease','-Ilib/tcc',"-I$outputPath",'-DJOINT_SOURCE_XRT',
        (New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/')))
    foreach($level in @(0,2)){
        $name="module-O$level";$binary="$outputPath/$name.exe"
        & gcc @hostFlags "-O$level" test/cases/xlir_module_cycle_transactions.c @dependencies release/xl.dll @libs -o $binary *> "$outputPath/$name.build.log"
        $buildExit=$LASTEXITCODE;$run=$null;$complete=$false
        if(!$buildExit){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
            $null = $process.Handle
            if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
            $process.Dispose()
            $complete=$run -eq 0 -and [bool](Select-String -LiteralPath "$outputPath/$name.out.log" -Pattern '^Module cycle transactions: 221 actual TCC lifetimes, .* 96 creation/commit allocation probes, [1-9][0-9]* real failures; MEM_FREE and zero live delta$' -Quiet)
            $complete=$complete -and [bool](Select-String -LiteralPath "$outputPath/$name.out.log" -SimpleMatch -Pattern 'V2 preparation inspection: 50 real TCC retirement snapshot allocation failures, no semantic failure latch, exact graph preservation, successful retry, MEM_FREE and zero live delta' -Quiet)
        }
        $rows+=[pscustomobject]@{Lane=$name;Build=$buildExit;Run=$run;Complete=$complete}
        Write-Output "$name build=$buildExit run=$run complete=$complete"
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Lanes=$rows;
        Scope='Additive acceptance: actual module cycle transactions and resident closed capture layouts. Original 19 gates and all populations remain mandatory. No class/native callable collector, Linux, release or default backend claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or @($rows | Where-Object {!$_.Complete}).Count){throw 'Module-cycle batch collected failures'}
} finally {Pop-Location}

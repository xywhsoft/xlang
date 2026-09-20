param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior slot-transfer evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$public.SourceXrt -or $runtime.Count -ne 1){throw 'Current source runtime required'}
    $sources=@('test/cases/xlir_slot_transfer_runtime.c','test/cases/xlir_slot_transfer_host.c',
        'src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$ContractDriver,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_slot_transfer_contract.c','test/cases/xlir_slot_transfers.xl',
        'test/cases/xlir_frame_ownership.xl','test/cases/xlir_generator_ownership.xl',
        'test/cases/xlir_cell_allocation_outcome.xl','test/cases/xlir_module_ownership.xl')+@(Get-Item -LiteralPath $sources)
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale slot-transfer inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $marker='Slot transfer IR: 5 fixtures, 20 rejected C/header-gated mutations, explicit text/JSON ABI; ordinary globals, async/generator frames, cells and module owners'
    $emission=@();$deterministic=$true
    foreach($repeat in @('','.repeat')){
        $prefix="$outputPath/xlir_slot_transfers$repeat"
        & $ContractDriver "$prefix.c" "$prefix.bind.h" "$prefix.json" *> "$prefix.emit.log"
        $exitCode=$LASTEXITCODE
        $complete=[regex]::Matches([IO.File]::ReadAllText("$prefix.emit.log"),[regex]::Escape($marker)).Count -eq 1
        $emission += [pscustomobject]@{Pass=$repeat;Exit=$exitCode;Complete=$complete}
    }
    foreach($suffix in @('c','bind.h','json')){
        if(!(Test-Path "$outputPath/xlir_slot_transfers.$suffix") -or !(Test-Path "$outputPath/xlir_slot_transfers.repeat.$suffix") -or
            (Get-FileHash "$outputPath/xlir_slot_transfers.$suffix").Hash -ne (Get-FileHash "$outputPath/xlir_slot_transfers.repeat.$suffix").Hash){$deterministic=$false}
    }
    $includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $includes=@("-I$includePath","-I$outputPath",'-I.')
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@()
    $pattern='Slot transfers: 100 generated global freezes, 100 frozen physical handoffs, 100 armed-OOM allocation-free commits, 20000 racing handoffs, ([1-9][0-9]*) frozen graph probes; exact active/payload/ref/allocator balance, no native root registration'
    foreach($name in @('gcc-O0','gcc-O2','tcc')){
        $binary="$outputPath/$name.exe"
        if($name -eq 'tcc'){
            & $Tcc -std=c11 @includes @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"
        }else{
            & gcc @strict "-$($name.Substring(4))" @includes @sources $runtime[0].Path @libs -o $binary *> "$outputPath/$name.build.log"
        }
        $buildExit=$LASTEXITCODE;$runExit=$null;$complete=$false;$probes=0
        if($buildExit -eq 0){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
            $null = $process.Handle
            if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$runExit=-999}
            else{$process.Refresh();$runExit=$process.ExitCode}
            $process.Dispose()
            $matches=[regex]::Matches([IO.File]::ReadAllText("$outputPath/$name.out.log"),$pattern)
            $complete=$runExit -eq 0 -and $matches.Count -eq 1
            if($complete){$probes=[int]$matches[0].Groups[1].Value}
        }
        $lanes += [pscustomobject]@{Name=$name;Build=$buildExit;Run=$runExit;Complete=$complete;FrozenGraphProbes=$probes}
        Write-Output "$name build=$buildExit run=$runExit complete=$complete probes=$probes"
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;IrMarker=$marker;
        PerProgram=[pscustomobject]@{GeneratedGlobalFreezes=100;FrozenHandoffs=100;ArmedOomCommits=100;RacingHandoffs=20000};
        Scope='Verified ordered slot ABI, exact generated global function isolated from host/API entry leases, native physical slots with real resident cell/value children, quiescent snapshots and exact balance. Concurrent-variable locking, arbitrary native writes, whole frame/node lifetime certification and module cycle retirement remain separate.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count){throw 'Slot transfer batch collected failures'}
}finally{Pop-Location}

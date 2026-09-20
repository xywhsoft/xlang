param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Choose fresh class-storage evidence' }
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if (!$build.SourceXrt -or $runtime.Count -ne 1) { throw 'A current source runtime is required' }
    $files=@(Get-Item $PSCommandPath,test/cases/xlir_class_storage.xl,test/cases/xlir_class_storage_contract.c,
        test/cases/xlir_class_storage_dll.c,test/cases/xlir_class_storage_dll.h,
        test/cases/xlir_class_storage_ir_contract.c,test/cases/xlir_class_storage_artifact.c,
        test/cases/xlir_class_storage_tcc_host.c,test/cases/xlir_one_shot_tcc_host.c,
        test/cases/xlir_async_module_lifetime_host.c)
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale class-storage inputs' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    New-Item -ItemType Directory -Path "$outputPath/__xl_runtime" | Out-Null
    foreach ($header in $build.Headers) {
        if ($header.Path -match '[/\\]__xl_runtime[/\\]') {
            Copy-Item -LiteralPath $header.Path -Destination (Join-Path "$outputPath/__xl_runtime" ([IO.Path]::GetFileName($header.Path)))
        }
    }
    & $ContractDriver "$outputPath/ir.c" "$outputPath/ir.h" *> "$outputPath/ir.log"
    $irExit=$LASTEXITCODE
    $irComplete=[bool](Select-String -LiteralPath "$outputPath/ir.log" -SimpleMatch 'Class storage IR: 4 prepared allocations, 4 exact commits, 2 lifecycle field bindings, 24 rejected Typed/Lowered/C/header mutations' -Quiet)
    $includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $emission=@()
    foreach ($phase in @('emit','repeat')) {
        $prefix="$outputPath/class-$phase"
        & $build.Driver test/cases/xlir_class_storage.xl emit $prefix *> "$prefix.log"
        $emission+=[pscustomobject]@{Phase=$phase;Exit=$LASTEXITCODE;
            Complete=[bool](Select-String -LiteralPath "$prefix.log" -SimpleMatch 'public source emit complete' -Quiet)}
    }
    $deterministic=$true
    foreach ($extension in @('c','h','json')) {
        $first="$outputPath/class-emit.$extension"; $repeat="$outputPath/class-repeat.$extension"
        if (!(Test-Path $first) -or !(Test-Path $repeat) -or (Get-FileHash $first).Hash -ne (Get-FileHash $repeat).Hash) { $deterministic=$false }
        if (Test-Path $first) { Copy-Item -LiteralPath $first -Destination "$outputPath/xlir_class_storage.generated.$extension" }
    }
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-I.',"-I$includePath","-I$outputPath")
    $libs=@('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32')
    $hostFlags=@('-Irelease','-Ilib/tcc','-DJOINT_SOURCE_XRT','-ffunction-sections','-fdata-sections',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-pointer-sign',
        '-Wno-format-truncation','-Wno-missing-field-initializers',
        ('-DJOINT_TCC_INCLUDE_DIRECTORY="'+$outputPath.Replace('\','/')+'"'))
    $embedded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $embedded} | ForEach-Object Path)
    $lanes=@()
    foreach ($level in @(0,2)) {
        $dll="$outputPath/generated-O$level.dll"
        & gcc @strict "-O$level" -shared test/cases/xlir_class_storage_dll.c -o $dll *> "$outputPath/dll-O$level.log"
        if ($LASTEXITCODE -ne 0) { throw 'Class storage DLL build failed' }
        foreach ($kind in @('native','language-tcc')) {
            $lane="$outputPath/$kind-O$level"
            if ($kind -eq 'native') {
                & gcc @strict "-O$level" test/cases/xlir_class_storage_contract.c src/runtime/xl6_code_finalizer.c src/runtime/xl6_code_lease.c $runtime[0].Path @libs -o "$lane.exe" *> "$lane.build.log"
            } else {
                & gcc @strict @hostFlags "-O$level" test/cases/xlir_class_storage_tcc_host.c @dependencies release/xl.dll '-Wl,--gc-sections' @libs -o "$lane.exe" *> "$lane.build.log"
            }
            $buildExit=$LASTEXITCODE; $runExit=$null; $complete=$false
            if ($buildExit -eq 0) {
                $options=@{FilePath="$lane.exe";WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;
                    RedirectStandardOutput="$lane.out.log";RedirectStandardError="$lane.err.log"}
                if ($kind -eq 'native') { $options.ArgumentList=@('"'+$dll+'"') }
                $process=Start-Process @options
                if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $runExit=-999 }
                else { $process.Refresh(); $runExit=$process.ExitCode }
                $process.Dispose()
                $marker=if ($kind -eq 'native') {'Resident class storage: 225 real DLL lifetimes, native aliases/cursors, 25 destructor-free code owners, 25 abandoned constructions, 25 concurrent alias cohorts, 25 inherited children, exact graph/unmap and allocator balance; 125 actual allocation refusals'}
                    else {'Generated class storage: 175 real TCC close/finalize/unmap lifetimes, 25 destructor-free owners, 75 committed and 75 failed early publications, native Retain/Clone/cursors, actual module edges, no explicit code guard, exact allocator balance'}
                $complete=[bool](Select-String -LiteralPath "$lane.out.log" -SimpleMatch $marker -Quiet)
            }
            $lanes+=[pscustomobject]@{Name="$kind-O$level";Build=$buildExit;Run=$runExit;Complete=$complete}
            Write-Output "$kind-O$level build=$buildExit run=$runExit complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        IrExit=$irExit;IrComplete=$irComplete;Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;
        Scope='Prepared class code ownership before fields/create, exact IR commit, successful/failed native self publication and resident finalization. Original module cycles/object escape gates remain independent; no production graph snapshot/claim/commit, full outer method adapters, release/VFS/Linux or legacy retirement completion.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$deterministic -or $irExit -ne 0 -or !$irComplete -or
        @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) { throw 'Class storage collected failures; inspect all lanes' }
} finally { Pop-Location }

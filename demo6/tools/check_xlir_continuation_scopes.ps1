param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Choose fresh continuation-scope evidence' }
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if (!$build.SourceXrt -or $runtime.Count -ne 1) { throw 'A current source runtime is required' }
    $files=@(Get-Item $PSCommandPath,test/cases/xlir_continuation_scope_contract.c,
        test/cases/xlir_continuation_scope_dll.c,test/cases/xlir_continuation_scope_dll.h,
        test/cases/xlir_resident_closure_dll.c,test/cases/xlir_resident_closure_dll.h,
        test/cases/xlir_continuation_closure.xl,test/cases/xlir_continuation_closure_artifact.c,
        test/cases/xlir_continuation_closure_host.c,test/cases/xlir_one_shot_tcc_host.c,
        test/cases/xlir_async_module_lifetime_host.c)
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale continuation-scope inputs' }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    New-Item -ItemType Directory -Path "$outputPath/__xl_runtime" | Out-Null
    foreach ($header in $build.Headers) {
        if ($header.Path -match '[/\\]__xl_runtime[/\\]') {
            Copy-Item -LiteralPath $header.Path -Destination (Join-Path "$outputPath/__xl_runtime" ([IO.Path]::GetFileName($header.Path)))
        }
    }
    $includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $emission=@()
    foreach ($phase in @('emit','repeat')) {
        $prefix="$outputPath/closure-$phase"
        & $build.Driver test/cases/xlir_continuation_closure.xl emit $prefix *> "$prefix.log"
        $emission+=[pscustomobject]@{Phase=$phase;Exit=$LASTEXITCODE;
            Complete=[bool](Select-String -LiteralPath "$prefix.log" -SimpleMatch 'public source emit complete' -Quiet)}
    }
    $deterministic=$true
    foreach ($extension in @('c','h','json')) {
        $first="$outputPath/closure-emit.$extension"; $repeat="$outputPath/closure-repeat.$extension"
        if (!(Test-Path $first) -or !(Test-Path $repeat) -or (Get-FileHash $first).Hash -ne (Get-FileHash $repeat).Hash) { $deterministic=$false }
        if (Test-Path $first) { Copy-Item -LiteralPath $first -Destination "$outputPath/xlir_continuation_closure.generated.$extension" }
    }
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-I.',"-I$includePath","-I$outputPath")
    $libs=@('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32')
    $hostFlags=@('-Irelease','-Ilib/tcc','-DJOINT_SOURCE_XRT','-ffunction-sections','-fdata-sections',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-pointer-sign',
        '-Wno-format-truncation','-Wno-missing-field-initializers',
        (New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/')))
    # Embedded API/path/console definitions are supplied by the original host;
    # the rest come from exactly the current public-source build, once each.
    $embedded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $embedded} | ForEach-Object Path)
    $lanes=@()
    foreach ($level in @(0,2)) {
        $dll="$outputPath/generated-O$level.dll"
        & gcc @strict "-O$level" -shared test/cases/xlir_continuation_scope_dll.c -o $dll *> "$outputPath/dll-O$level.log"
        if ($LASTEXITCODE -ne 0) { throw 'Continuation scope DLL build failed' }
        foreach ($kind in @('native','language-tcc')) {
            $lane="$outputPath/$kind-O$level"
            if ($kind -eq 'native') {
                & gcc @strict "-O$level" test/cases/xlir_continuation_scope_contract.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_code_lease.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c $runtime[0].Path @libs -o "$lane.exe" *> "$lane.build.log"
            } else {
                & gcc @strict @hostFlags "-O$level" test/cases/xlir_continuation_closure_host.c @dependencies release/xl.dll '-Wl,--gc-sections' @libs -o "$lane.exe" *> "$lane.build.log"
            }
            $buildExit=$LASTEXITCODE; $runExit=$null; $complete=$false
            if ($buildExit -eq 0) {
                $options=@{FilePath="$lane.exe";WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;
                    RedirectStandardOutput="$lane.out.log";RedirectStandardError="$lane.err.log"}
                if ($kind -eq 'native') { $options.ArgumentList=@('"'+$dll+'"') }
$process=Start-Process @options
                $null = $process.Handle
                if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $runExit=-999 }
                else { $process.Refresh(); $runExit=$process.ExitCode }
                $process.Dispose()
                $marker=if ($kind -eq 'native') {'Continuation scopes: 100 real DLL lifetimes, 200 run/release callback pairs, 350 inherited closures, 100 allocation-free successor abort/recommit handoffs, 25 four-native-thread drains, 100 actual root-lease OOM refusals and 100 capacity rollbacks; graph, queue-credit, diagnostics and allocator balance'}
                    else {'Generated continuation closures: 100 real TCC close/resume/closure/invoke/unmap lifetimes, 50 Future-held and 50 extracted native references, no explicit owner guard, exact allocator balance'}
                $complete=[bool](Select-String -LiteralPath "$lane.out.log" -SimpleMatch $marker -Quiet)
            }
            $lanes+=[pscustomobject]@{Name="$kind-O$level";Build=$buildExit;Run=$runExit;Complete=$complete}
            Write-Output "$kind-O$level build=$buildExit run=$runExit complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;
        Scope='Real shared ticket lease and resident run/release scopes; inherited closure creation after Close, not new execution admission. Original object/module cycle gates remain independent and red; no production graph snapshot/claim/commit, automatic class code owner or complete outer-adapter claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if (!$unchanged -or !$deterministic -or @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) { throw 'Continuation scopes collected failures; inspect all lanes' }
} finally { Pop-Location }

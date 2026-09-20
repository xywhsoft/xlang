param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$IrContractDriver = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $rootPath
try {
    if (Test-Path -LiteralPath $Directory) { throw 'Use a new module ownership evidence directory' }
    New-Item -ItemType Directory -Path $Directory | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $Directory 'xl.dll')
    $build = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if (!$build.SourceXrt) { throw 'Current source runtime is required' }
    $runtime = @($build.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
    if ($runtime.Count -ne 1) { throw 'Source runtime object is missing or ambiguous' }
    $inputs = @($build.Inputs) + @($build.Objects) + @($build.Headers)
    $cases = @('test/cases/xlir_module_ownership.xl','test/cases/xlir_module_ownership_artifact.c',
        'test/cases/xlir_module_ownership_tcc_host.c','test/cases/xlir_one_shot_tcc_host.c',
        'test/cases/xlir_async_module_lifetime_host.c',$ContractDriver,$PSCommandPath)
    $inputs += @($cases | ForEach-Object { [pscustomobject]@{ Path=(Resolve-Path -LiteralPath $_).Path; Sha256=(Get-FileHash -LiteralPath $_).Hash } })
    if ($IrContractDriver) {
        $contractBuild = Get-Content -LiteralPath (Join-Path (Split-Path -Parent $IrContractDriver) 'build.json') -Raw | ConvertFrom-Json
        $inputs += @($contractBuild.Inputs) + @($contractBuild.Drivers)
    }
    if (@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Stale module ownership inputs' }
    $irContractExit = $null
    if ($IrContractDriver) {
        & $IrContractDriver *> (Join-Path $Directory 'ir-contract.log')
        $irContractExit = $LASTEXITCODE
    }
    foreach ($pass in @('generated','repeat')) {
        $prefix = Join-Path $Directory $pass
        & $ContractDriver test/cases/xlir_module_ownership.xl "$prefix.c" "$prefix.h" "$prefix.json" --script *> (Join-Path $Directory "$pass.log")
        if ($LASTEXITCODE) { throw "Module ownership $pass emission failed" }
    }
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash (Join-Path $Directory "generated.$extension")).Hash -ne
            (Get-FileHash (Join-Path $Directory "repeat.$extension")).Hash) { throw "Nondeterministic $extension" }
        Copy-Item -LiteralPath (Join-Path $Directory "generated.$extension") -Destination (Join-Path $Directory "xlir_module_ownership.generated.$extension")
    }
    $includePath = [IO.Path]::GetFullPath($Directory)
    $runtimeInclude = Join-Path $includePath '__xl_runtime'
    New-Item -ItemType Directory -Path $runtimeInclude | Out-Null
    foreach ($header in $build.Headers) {
        if ($header.Path -match '[/\\]__xl_runtime[/\\]') {
            Copy-Item -LiteralPath $header.Path -Destination (Join-Path $runtimeInclude ([IO.Path]::GetFileName($header.Path)))
        }
    }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',
        "-I$includePath",'-I.','-Irelease','-Ilib/tcc','-DJOINT_SOURCE_XRT','-ffunction-sections','-fdata-sections',
        (New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $includePath.Replace('\','/')))
    $results = @()
    # O0 keeps unused public API bodies in this white-box host. Supply their
    # complete current-source dependencies rather than relying on optimization
    # to erase unresolved compiler services. Definitions embedded by the host
    # and the runtime TUs compiled below must not be linked a second time.
    $embedded = @('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o',
        'xl6_continuation_queue.o','xl6_continuation_executor.o','xl6_continuation_future.o',
        'xl6_code_lease.o','xl6_closure.o','xl6_cell.o','xl6_slot.o','xl6_frame_storage.o','xl6_continuation_frame.o','xl6_future_wait.o','xl6_task.o','xl6_capture.o','xl6_value_iterator.o','xl6_future_callback.o','xl6_generator.o','xl6_future_payload.o','xrt_single_header_impl.o')
    $dependencies = @($build.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -notin $embedded } | ForEach-Object Path)
    foreach ($level in @(0,2)) {
        $prefix = Join-Path $Directory "gcc-o$level"
        & gcc @flags "-O$level" -c test/cases/xlir_module_ownership_tcc_host.c -o "$prefix.o" *> "$prefix.compile.log"
        $code = $LASTEXITCODE
        if (!$code) {
            & gcc @flags "-O$level" "$prefix.o" src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_code_lease.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c @dependencies $runtime[0].Path release/xl.dll '-Wl,--gc-sections' -lWs2_32 -lIPHLPAPI -lShell32 -lOle32 -lAdvapi32 -o "$prefix.exe" *> "$prefix.link.log"
            $code = $LASTEXITCODE
        }
        if (!$code) {
$process = Start-Process -FilePath ([IO.Path]::GetFullPath("$prefix.exe")) -WorkingDirectory $rootPath -WindowStyle Hidden -PassThru -RedirectStandardOutput "$prefix.stdout.log" -RedirectStandardError "$prefix.stderr.log"
            $null = $process.Handle
            if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $code = -999 }
            else { $process.Refresh(); $code = $process.ExitCode }
        }
        $results += [pscustomobject]@{ Lane="gcc-o$level-host-tcc-module"; Exit=$code; Prefix=$prefix }
        Write-Output "Module ownership GCC O$level / real TCC: exit=$code"
    }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{ RecordedAt=(Get-Date).ToString('o'); Inputs=$inputs; InputsUnchanged=$unchanged; Results=$results; IrContractExit=$irContractExit;
        Scope='Current-source module/global ownership views and copied code-owner capability; explicit shared code lease and accepted ticket handoff; actual TCC page deletion and allocation balance. Test provides whole-graph quiescence. Automatic escaped-object pin, cycle collection and production safepoints remain open.' }
    [IO.File]::WriteAllText([IO.Path]::GetFullPath((Join-Path $Directory 'results.json')), ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (!$unchanged -or ($IrContractDriver -and $irContractExit -ne 0) -or @($results | Where-Object Exit -ne 0).Count) { throw 'Module ownership batch collected failures; inspect evidence' }
} finally { Pop-Location }

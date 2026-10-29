param([string]$Root = '', [string]$ContractDirectory = 'build/test/xlir_launch_v124_contracts',
    [switch]$BuildContracts)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $names = @('terminal_outcome','frame_completion','result_commit','cancel_watch')
    $manifestPath = Join-Path $ContractDirectory 'build.json'
    if ($BuildContracts -or !(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        if (Test-Path -LiteralPath $ContractDirectory) {
            throw 'Shared contract directory exists without a reusable manifest; choose a fresh directory'
        }
        $drivers = @($names | ForEach-Object { "test/cases/xlir_async_$($_)_contract.c" })
        & tools/build_xlir_validation_driver.ps1 -BatchDrivers $drivers -BatchDirectory $ContractDirectory
        if ($LASTEXITCODE -ne 0) { throw 'Shared contract driver build failed' }
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if (@(@($manifest.Inputs) + @($manifest.Drivers) | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -or
        (Get-FileHash release/xl.dll).Hash -ne $manifest.RuntimeSha256) { throw 'Stale shared compiler inputs or drivers; rebuild the contract batch' }
    $batchInputs = @(@(Get-ChildItem src,lib,test/cases,tools -Recurse -File) + @(Get-Item release/xl.dll)) | Sort-Object FullName
    $batchRecords = @($batchInputs | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    # A verified shared batch may already be running another driver. Do not
    # overwrite its loaded DLL; identical content is safe to share read-only.
    $contractRuntime = Join-Path $ContractDirectory 'xl.dll'
    if (!(Test-Path -LiteralPath $contractRuntime) -or
        (Get-FileHash -LiteralPath $contractRuntime).Hash -ne $manifest.RuntimeSha256) {
        Copy-Item -LiteralPath release/xl.dll -Destination $contractRuntime -Force
    }
    if ((Get-FileHash -LiteralPath $contractRuntime).Hash -ne $manifest.RuntimeSha256) {
        throw 'Shared compiler runtime does not match the verified build'
    }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Ibuild/test')
    $results = @()
    & gcc @flags -c test/cases/xlir_async_source_transport.c -o build/test/xlir_async_source_batch.transport.o
    if ($LASTEXITCODE -ne 0) { throw 'Source batch Watch observation build failed' }
    foreach ($name in $names) {
        $prefix = "build/test/xlir_async_$name"
        $driver = Join-Path $ContractDirectory "xlir_async_$($name)_contract.exe"
        $entry = [ordered]@{Name=$name;Contract=$null;Repeat=$null;Deterministic=$false;Build=$null;Run=$null;
            DriverSha256=(Get-FileHash $driver).Hash;ContractLog="$prefix.source.contract.log";
            BuildLog="$prefix.source.build.log";RunLog="$prefix.source.run.log";RunErrorLog="$prefix.source.run.err.log"}
        & $driver "$prefix.c" "$prefix.h" "$prefix.json" *> $entry.ContractLog
        $entry.Contract = $LASTEXITCODE
        if ($entry.Contract -eq 0) {
            & $driver "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json" *> "$prefix.source.repeat.log"
            $entry.Repeat = $LASTEXITCODE
            $entry.Deterministic = $entry.Repeat -eq 0
            foreach ($suffix in @('c','h','json','h.bind.h')) {
                if (!(Test-Path "$prefix.$suffix") -or !(Test-Path "$prefix.repeat.$suffix") -or
                    (Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) {
                    $entry.Deterministic = $false
                }
            }
            if ($entry.Deterministic) {
                & gcc @flags "test/cases/xlir_async_$($name)_runtime.c" "test/cases/xlir_async_$($name)_host.c" `
                    src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c `
                    build/test/xlir_async_source_batch.transport.o test/cases/xlir_async_source_sync.c `
                    src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_task_frame.c src/runtime/xl6_task_pool.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll -o "$prefix.source.exe" *> $entry.BuildLog
                $entry.Build = $LASTEXITCODE
                if ($entry.Build -eq 0) {
                    $process = Start-Process -FilePath (Join-Path (Get-Location) "$prefix.source.exe") -WindowStyle Hidden -PassThru `
                        -RedirectStandardOutput $entry.RunLog -RedirectStandardError $entry.RunErrorLog
                    if ($process.WaitForExit(30000)) { $entry.Run = $process.ExitCode }
                    else { $process.Kill(); $process.WaitForExit(); $entry.Run = 'timeout-30s' }
                    $process.Dispose()
                }
            }
        }
        $results += [pscustomobject]$entry
        Write-Output "$name contract=$($entry.Contract) deterministic=$($entry.Deterministic) build=$($entry.Build) run=$($entry.Run)"
    }
    $unchanged = @($batchRecords | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
    $report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;InputsUnchanged=$unchanged;
        Inputs=$batchRecords;CompilerBuild=(Join-Path $ContractDirectory 'build.json');
        RuntimeSha256=(Get-FileHash release/xl.dll).Hash;
        Scope='Current-source contract drivers and dispatcher/runtime support; permanently resident generated test hosts. No release rebuild, C-host JIT/AOT, public frontend or unloaded-object admission claim.'}
    [IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_async_source_batch.results.json'), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($results | Where-Object { $_.Contract -ne 0 -or !$_.Deterministic -or $_.Build -ne 0 -or $_.Run -ne 0 }).Count) {
        throw 'All async contract families collected; source-batch failures remain'
    }
} finally { Pop-Location }

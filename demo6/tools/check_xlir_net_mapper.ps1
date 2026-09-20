param([Parameter(Mandatory=$true)][string]$SourceDriver, [string]$Root = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_net_mapper'
    $files = @(@(Get-ChildItem src,lib,test/cases,tools -Recurse -File) + @(Get-Item $SourceDriver,release/xl.dll)) | Sort-Object FullName -Unique
    $inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    & $SourceDriver test/cases/stdlib_net_fast_path.xl "$prefix.c" "$prefix.h" "$prefix.json" *> "$prefix.generate.log"
    if ($LASTEXITCODE -ne 0) { throw 'Mapper verified source generation failed' }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I.','-Ilib/xrt','-Ibuild/test')
    $buildExit=Invoke-XlirNativeToLog gcc ($flags+@('-include',"$prefix.c",'-c','test/cases/xlir_net_mapper.c','-o',"$prefix.o")) "$prefix.build.log"
    if ($buildExit -ne 0) { throw 'Mapper strict C build failed' }
    $linkSources=@("$prefix.o",'src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
        'src/runtime/xl6_continuation_future.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c','release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-o',"$prefix.exe")
    $linkExit=Invoke-XlirNativeToLog gcc ($flags+$linkSources) "$prefix.link.log"
    if ($linkExit -ne 0) { throw 'Mapper resident link failed' }
$process = Start-Process -FilePath (Join-Path (Get-Location) "$prefix.exe") -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$prefix.run.log" -RedirectStandardError "$prefix.run.err.log"
    $null = $process.Handle
    if ($process.WaitForExit(45000)) { $code = $process.ExitCode }
    else { $process.Kill(); $process.WaitForExit(); $code = 'timeout-45s' }
    $process.Dispose()
    $unchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
    $report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Exit=$code;InputsUnchanged=$unchanged;Inputs=$inputs;
        DriverSha256=(Get-FileHash $SourceDriver).Hash;RuntimeSha256=(Get-FileHash release/xl.dll).Hash;
        Scope='Resident generated mapper ABI: exact per-transaction ownership/fault audit after native packet/address preparation and network shutdown. Not whole-network allocation accounting or code unloading.'}
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.results.json"), ($report | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Get-Content "$prefix.run.log"
    if (!$unchanged -or $code -ne 0) { throw 'Mapper ownership audit failed' }
} finally { Pop-Location }

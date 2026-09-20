param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver,
    [ValidateRange(1,100)][int]$Rounds = 3)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $inputFiles = @(@(Get-ChildItem src,lib,test/cases,tools -Recurse -File) + @(Get-Item release/xl.dll,$SourceDriver)) | Sort-Object FullName -Unique
    $inputs = @($inputFiles | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I.','-Ilib/xrt','-Ibuild/test')
    $support = @('src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
        'src/runtime/xl6_continuation_future.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
    $results = @()
    foreach ($name in @('stdlib_net_fast_path','stdlib_net_send_file','stdlib_tls_server_loopback',
        'xlir_net_resolver_standalone','xlir_net_datagram_boundaries')) {
        $prefix = "build/test/xlir_network_source_$name"
        $entry = [ordered]@{Fixture=$name;Generation=$null;Repeat=$null;Deterministic=$false;Build=$null;Run=$null;Runs=@();
            MissingSymbols=@();MissingTargets=@();
            GenerationLog="$prefix.generate.log";BuildLog="$prefix.build.log";RunLog="$prefix.run.log";RunErrorLog="$prefix.run.err.log"}
        & $SourceDriver "test/cases/$name.xl" "$prefix.c" "$prefix.h" "$prefix.json" *> $entry.GenerationLog
        $entry.Generation = $LASTEXITCODE
        if ($entry.Generation -eq 0) {
            & $SourceDriver "test/cases/$name.xl" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json" *> "$prefix.repeat.log"
            $entry.Repeat = $LASTEXITCODE; $entry.Deterministic = $entry.Repeat -eq 0
            foreach ($ext in @('c','h','json')) {
                if (!(Test-Path "$prefix.repeat.$ext") -or (Get-FileHash "$prefix.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { $entry.Deterministic = $false }
            }
            if ($entry.Deterministic) {
                & gcc @flags -include "$prefix.c" -c test/cases/xlir_network_source_host.c -o "$prefix.host.o" *> $entry.BuildLog
                $entry.Build = $LASTEXITCODE
                if ($entry.Build -eq 0) {
                    & gcc @flags "$prefix.host.o" @support release/xl.dll -lWs2_32 -lIPHLPAPI -lShell32 -o "$prefix.exe" *>> $entry.BuildLog
                    $entry.Build = $LASTEXITCODE
                }
                if ($entry.Build -eq 0) {
                    $marker = switch ($name) {
                        'stdlib_net_fast_path' { 'stdlib net fast path ok' }
                        'stdlib_net_send_file' { 'stdlib net send file ok' }
                        'stdlib_tls_server_loopback' { 'tls-server-loopback-ok' }
                        'xlir_net_resolver_standalone' { 'xlir resolver standalone ok' }
                        'xlir_net_datagram_boundaries' { 'xlir datagram boundaries ok' }
                    }
                    foreach ($round in 1..$Rounds) {
                        $log = "$prefix.round$round.log"; $errorLog = "$prefix.round$round.err.log"
                        $process = Start-Process -FilePath (Join-Path (Get-Location) "$prefix.exe") -WindowStyle Hidden -PassThru `
                            -RedirectStandardOutput $log -RedirectStandardError $errorLog
                        if ($process.WaitForExit(45000)) { $exitCode = $process.ExitCode }
                        else { $process.Kill(); $process.WaitForExit(); $exitCode = 'timeout-45s' }
                        $process.Dispose()
                        $complete = ([string](Get-Content $log -Raw)).Contains($marker)
                        $entry.Runs += [pscustomobject]@{Round=$round;Exit=$exitCode;CompletionMarker=$complete;Log=$log;ErrorLog=$errorLog}
                    }
                    $entry.Run = if (@($entry.Runs | Where-Object { $_.Exit -ne 0 -or !$_.CompletionMarker }).Count) { 1 } else { 0 }
                } else {
                    # Diagnosis only: names come from the actual linker log,
                    # never serve as Cgen's component selection mechanism.
                    $entry.MissingSymbols = @([regex]::Matches((Get-Content $entry.BuildLog -Raw), "undefined reference to .([^']+)'") |
                        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
                    $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
                    $entry.MissingTargets = @($ir.callTargets | Where-Object { $_.cName -in $entry.MissingSymbols } |
                        Select-Object cName,runtimeBridgeName,reachability,returnType,errorPhysicalAbiName,parameterTypes,parameterPhysicalAbis)
                }
            }
        }
        $results += [pscustomobject]$entry
        Write-Output "$name generated=$($entry.Generation) deterministic=$($entry.Deterministic) build=$($entry.Build) run=$($entry.Run)"
    }
    $unchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
    $report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Inputs=$inputs;InputsUnchanged=$unchanged;Rounds=$Rounds;
        DriverSha256=(Get-FileHash $SourceDriver).Hash;RuntimeSha256=(Get-FileHash release/xl.dll).Hash;
        Scope='Current-source generated loopback fixtures and source continuation runtime, resident executable. No public frontend, unloading, allocation audit or same-source release claim.'}
    [IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_network_source_batch.results.json'), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($results | Where-Object { $_.Generation -ne 0 -or !$_.Deterministic -or $_.Build -ne 0 -or $_.Run -ne 0 }).Count) { throw 'All network fixtures collected; failures remain' }
} finally { Pop-Location }

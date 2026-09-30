param([Parameter(Mandatory=$true)][string]$Directory, [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$manifest = Get-Content (Join-Path $Directory 'build.json') -Raw | ConvertFrom-Json
if (@(@($manifest.Inputs) + @($manifest.Headers) | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -or
    (Get-FileHash $manifest.Driver).Hash -ne $manifest.DriverSha256) { throw 'Stale public-source compiler inputs' }
$files = @(@(Get-ChildItem src,lib,test/cases,tools,res/tcc -Recurse -File) +
    @(Get-Item release/xl.dll,$manifest.Driver) + @($manifest.Headers | ForEach-Object { Get-Item -LiteralPath $_.Path })) | Sort-Object FullName -Unique
$inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$caseDirectory = 'build/test/xlir_public_source_cases'
$reportPath = 'build/test/xlir_public_source_batch.results.json'
if ($OutputDirectory) {
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose fresh public-source evidence' }
    $caseDirectory = Join-Path $OutputDirectory 'cases'
    $reportPath = Join-Path $OutputDirectory 'results.json'
}
New-Item -ItemType Directory -Force -Path $caseDirectory | Out-Null
$results = @()
function Invoke-Phase([string]$source, [string]$mode, [string]$prefix, [string]$logPrefix, [string]$sidecar = '') {
$arguments = @($source,$mode,$prefix)
if ($sidecar) { $arguments += $sidecar }
$process = Start-Process -FilePath $manifest.Driver -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$logPrefix.log" -RedirectStandardError "$logPrefix.err.log"
    $null = $process.Handle
    if ($process.WaitForExit(45000)) { $code = $process.ExitCode }
    else { $process.Kill(); $process.WaitForExit(); $code = 'timeout-45s' }
    $process.Dispose()
    $complete = [bool](Select-String -LiteralPath "$logPrefix.log" -SimpleMatch "public source $mode complete" -Quiet)
    return [pscustomobject]@{Exit=$code;Complete=$complete;Log="$logPrefix.log";ErrorLog="$logPrefix.err.log"}
}
foreach ($name in @('cgen_basic','import_source_use','import_source_public_types','cgen_optional_nullable_function_default',
    'xlir_default_argument_unwind','xlir_default_argument_allocation',
    'cgen_try_finally_pending_async','stdlib_net_fast_path','stdlib_net_send_file','stdlib_tls_server_loopback',
    'xlir_net_resolver_standalone','xlir_net_datagram_boundaries',
    'xlir_constant_loop_completion','xlir_async_void_suspension')) {
    $prefix = Join-Path $caseDirectory $name; $source = "test/cases/$name.xl"
    $sidecar = if ($name -eq 'xlir_async_void_suspension') { 'test/cases/xlir_async_void_suspension_native.c' } else { '' }
    $entry = [ordered]@{Fixture=$name;Emit=$null;Repeat=$null;Deterministic=$false;Run=$null;Module=$null}
    $entry.Emit = Invoke-Phase $source 'emit' $prefix "$prefix.emit" $sidecar
    if ($entry.Emit.Exit -eq 0 -and $entry.Emit.Complete) {
        $entry.Repeat = Invoke-Phase $source 'emit' "$prefix.repeat" "$prefix.repeat.emit" $sidecar
        $entry.Deterministic = $entry.Repeat.Exit -eq 0 -and $entry.Repeat.Complete
        foreach ($suffix in @('c','h','json')) {
            if (!(Test-Path "$prefix.$suffix") -or !(Test-Path "$prefix.repeat.$suffix") -or
                (Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { $entry.Deterministic = $false }
        }
    }
    # Exercise public execution independently, even if another API phase fails.
    $entry.Run = Invoke-Phase $source 'run' $prefix "$prefix.run" $sidecar
    $entry.Module = Invoke-Phase $source 'module' $prefix "$prefix.module" $sidecar
    $results += [pscustomobject]$entry
    Write-Output "$name emit=$($entry.Emit.Exit) deterministic=$($entry.Deterministic) run=$($entry.Run.Exit) module=$($entry.Module.Exit)"
}
$negative = @()
foreach ($name in @('xlir_constant_loop_break_missing_return',
                    'xlir_constant_loop_false_missing_return',
                    'xlir_dynamic_loop_missing_return')) {
    $prefix = Join-Path $caseDirectory $name
    $check = Invoke-Phase "test/cases/$name.xl" 'run' $prefix "$prefix.run"
    $diagnostic = [bool](Select-String -LiteralPath $check.ErrorLog -SimpleMatch 'XLIR non-void function has reachable unterminated block' -Quiet)
    $negative += [pscustomobject]@{Fixture=$name;Run=$check;ExpectedDiagnostic=$diagnostic}
    Write-Output "$name reject=$($check.Exit) diagnostic=$diagnostic"
}
$recoveryPrefix = Join-Path $caseDirectory 'recovery'
$recovery = Invoke-Phase 'unused' 'recovery' $recoveryPrefix $recoveryPrefix
Write-Output "context recovery exit=$($recovery.Exit) complete=$($recovery.Complete)"
$unchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Negative=$negative;Inputs=$inputs;InputsUnchanged=$unchanged;
    CompilerManifest=(Join-Path $Directory 'build.json');DriverSha256=$manifest.DriverSha256;Recovery=$recovery;
    Scope=$manifest.Scope;PublicApis=@('xlcAddSourceFile','xlcCompile','xlcEmitC','xlcEmitHeader','xlcEmitLoweredIr','xlcRun','xlcRelocateModule','xlRunModuleMain','xlDestroyModule')}
[IO.File]::WriteAllText([IO.Path]::GetFullPath($reportPath), ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
if (!$unchanged -or $recovery.Exit -ne 0 -or !$recovery.Complete -or
    @($negative | Where-Object { $_.Run.Exit -ne 1 -or !$_.ExpectedDiagnostic }).Count -or
    @($results | Where-Object { !$_.Deterministic -or $_.Run.Exit -ne 0 -or !$_.Run.Complete -or $_.Module.Exit -ne 0 -or !$_.Module.Complete }).Count) { throw 'All public-source phases collected; failures remain' }

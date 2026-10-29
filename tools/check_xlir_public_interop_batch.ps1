param([Parameter(Mandatory=$true)][string]$Directory, [switch]$PackageBatch)
$ErrorActionPreference = 'Stop'
$manifestPath = Join-Path $Directory 'build.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
if (@(@($manifest.Inputs) + @($manifest.Headers) | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -or
    (Get-FileHash $manifest.Driver).Hash -ne $manifest.DriverSha256) { throw 'Stale public-source compiler inputs' }
$files = @(@(Get-ChildItem src,lib,test/cases,test/fixtures,tools,res/tcc -Recurse -File) +
    @(Get-Item release/xl.dll,$manifest.Driver) + @($manifest.Headers | ForEach-Object { Get-Item -LiteralPath $_.Path })) | Sort-Object FullName -Unique
$inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$outDir = if ($PackageBatch) { 'build/test/xlir_public_package_cases' } else { 'build/test/xlir_public_interop_cases' }
New-Item -ItemType Directory -Force -Path $outDir,build/test/xlir_public_temps | Out-Null
function Invoke-Phase($case, [string]$mode, [string]$prefix) {
    $arguments = @("test/cases/$($case.Source).xl",$mode,$prefix) + @($case.Hosts | ForEach-Object { "test/cases/$_.c" })
$process = Start-Process -FilePath $manifest.Driver -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$prefix.$mode.log" -RedirectStandardError "$prefix.$mode.err.log"
    $null = $process.Handle
    if ($process.WaitForExit(45000)) { $code = $process.ExitCode }
    else { $process.Kill(); $process.WaitForExit(); $code = 'timeout-45s' }
    $process.Dispose()
    $complete = [bool](Select-String -LiteralPath "$prefix.$mode.log" -SimpleMatch "public source $mode complete" -Quiet)
    $diagnostic = $false
    if ($case.Reject) {
        $message = [string](Get-Content "$prefix.$mode.err.log" -Raw)
        $diagnostic = $message -match 'incompatible|conflicting' -and $message -match [regex]::Escape($case.Symbol)
    }
    return [pscustomobject]@{Exit=$code;Complete=$complete;ExpectedDiagnostic=$diagnostic;Log="$prefix.$mode.log";ErrorLog="$prefix.$mode.err.log"}
}
$cases = @(
    @{Name='bool';Source='extern_bool';Hosts=@('extern_bool_host')},
    @{Name='function_value';Source='cgen_extern_function_value';Hosts=@('cgen_extern_function_value_host')},
    @{Name='extern_variable_multi_c';Source='extern_var';Hosts=@('extern_var_host','cgen_extern_function_value_host')},
    @{Name='native_string_copy';Source='cgen_extern_string_return';Hosts=@('xlir_public_native_host')},
    @{Name='native_handle_destroy';Source='cgen_native_class_bind_point_basic';Hosts=@('xlir_public_native_host')},
    @{Name='package_c';Source='import_package_use';Hosts=@()},
    @{Name='package_class';Source='import_package_class_handle_use';Hosts=@()},
    @{Name='package_multi_source';Source='import_package_multi_source_scope';Hosts=@()},
    @{Name='package_include';Source='import_package_include_dir';Hosts=@()},
    @{Name='reject_int_width';Source='c_abi_mismatch_extern_header';Hosts=@('c_abi_mismatch_extern_header_host');Reject=$true;Symbol='xl6_bad_native_add'},
    @{Name='reject_destructor_return';Source='cgen_extern_string_return';Hosts=@('cgen_extern_string_return_host');Reject=$true;Symbol='native_text_close'},
    @{Name='added_c_scalar_types';Source='c_abi_types';Hosts=@('c_abi_types_host','xlir_public_native_host');Added=$true},
    @{Name='added_callback_value_result';Source='xlir_public_callback_byref';Hosts=@('xlir_public_callback_byref_host');Added=$true},
    @{Name='added_reject_global_width';Source='extern_var';Hosts=@('xlir_public_bad_global_host');Reject=$true;Symbol='host_counter';Added=$true}
)
if ($PackageBatch) {
    $cases = @(
        @{Name='package_c';Source='import_package_use';Hosts=@()},
        @{Name='package_include';Source='import_package_include_dir';Hosts=@()},
        @{Name='package_multi_c_value_result';Source='xlir_pkg_valid';Hosts=@()},
        @{Name='package_reject_width';Source='xlir_pkg_bad_width';Hosts=@();Reject=$true;Symbol='xlir_pkg_bad_add'},
        @{Name='package_reject_global';Source='xlir_pkg_bad_global';Hosts=@();Reject=$true;Symbol='xlir_pkg_bad_count'},
        @{Name='package_reject_destroy';Source='xlir_pkg_bad_destroy';Hosts=@();Reject=$true;Symbol='xlir_pkg_box_close'},
        @{Name='added_package_and_sidecar';Source='xlir_pkg_mixed';Hosts=@('cgen_extern_function_value_host');Added=$true},
        @{Name='added_package_bad_sidecar';Source='xlir_pkg_mixed';Hosts=@('xlir_pkg_mixed_bad_host');Reject=$true;Symbol='native_add_i64';Added=$true}
    )
}
$oldPackagePath = [Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH','Process')
$results = @()
try {
    # Process-local fixture discovery; restore even if the runner fails.
    $env:XLANG_PACKAGE_PATH = (Resolve-Path test/fixtures).Path
    foreach ($case in $cases) {
        $prefix = "$outDir/$($case.Name)"
        $entry = [ordered]@{Fixture=$case.Name;Source=$case.Source;Hosts=$case.Hosts;Added=[bool]$case.Added;Reject=[bool]$case.Reject;Emit=$null;Repeat=$null;Deterministic=$false;HeaderExit=$null;Run=$null;Module=$null;Passed=$false}
        $entry.Emit = Invoke-Phase $case 'emit' $prefix
        if ($entry.Emit.Exit -eq 0 -and $entry.Emit.Complete) {
            $entry.Repeat = Invoke-Phase $case 'emit' "$prefix.repeat"
            $entry.Deterministic = $entry.Repeat.Exit -eq 0 -and $entry.Repeat.Complete
            foreach ($suffix in @('c','h','json')) {
                if (!(Test-Path "$prefix.$suffix") -or !(Test-Path "$prefix.repeat.$suffix") -or
                    (Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { $entry.Deterministic = $false }
            }
            & gcc -std=c11 -Wall -Wextra -Werror -x c -fsyntax-only "$prefix.h" *> "$prefix.header.log"
            $entry.HeaderExit = $LASTEXITCODE
        }
        $entry.Run = Invoke-Phase $case 'run' $prefix
        $entry.Module = Invoke-Phase $case 'module' $prefix
        $entry.Passed = $entry.Deterministic -and $entry.HeaderExit -eq 0
        foreach ($phase in @($entry.Run,$entry.Module)) {
            if ($case.Reject) { $entry.Passed = $entry.Passed -and $phase.Exit -eq 1 -and !$phase.Complete -and $phase.ExpectedDiagnostic }
            else { $entry.Passed = $entry.Passed -and $phase.Exit -eq 0 -and $phase.Complete }
        }
        $results += [pscustomobject]$entry
        Write-Output "$($case.Name) deterministic=$($entry.Deterministic) run=$($entry.Run.Exit) module=$($entry.Module.Exit) passed=$($entry.Passed)"
    }
} finally {
    if ($null -eq $oldPackagePath) { Remove-Item Env:XLANG_PACKAGE_PATH -ErrorAction SilentlyContinue }
    else { [Environment]::SetEnvironmentVariable('XLANG_PACKAGE_PATH',$oldPackagePath,'Process') }
}
$headersOnly = @()
$headerSources = if ($PackageBatch) { @() } else { @('xlir_async_module_lifetime','xlir_foreign_function_value_abi') }
foreach ($source in $headerSources) {
    $case = @{Source=$source;Hosts=@()}; $prefix = "$outDir/header_$source"
    $emitted = Invoke-Phase $case 'emit' $prefix
    $repeated = Invoke-Phase $case 'emit' "$prefix.repeat"
    $same = $emitted.Exit -eq 0 -and $emitted.Complete -and $repeated.Exit -eq 0 -and $repeated.Complete
    $headerExit = $null
    if ($same) {
        foreach ($suffix in @('c','h','json')) {
            if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { $same = $false }
        }
        & gcc -std=c11 -Wall -Wextra -Werror -x c -fsyntax-only "$prefix.h" *> "$prefix.header.log"
        $headerExit = $LASTEXITCODE
    }
    $headersOnly += [pscustomobject]@{Fixture=$source;Emit=$emitted;Repeat=$repeated;Deterministic=$same;HeaderExit=$headerExit;Passed=($same -and $headerExit -eq 0)}
    Write-Output "header-only $source deterministic=$same header=$headerExit"
}
$unchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Results=$results;Inputs=$inputs;InputsUnchanged=$unchanged;
    CompilerManifest=$manifestPath;DriverSha256=$manifest.DriverSha256;Scope=$manifest.Scope;HeadersOnly=$headersOnly;
    PackagePathRestored=([Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH','Process') -eq $oldPackagePath)}
$reportPath = if ($PackageBatch) { 'build/test/xlir_public_package_batch.results.json' } else { 'build/test/xlir_public_interop_batch.results.json' }
[IO.File]::WriteAllText((Join-Path (Get-Location) $reportPath), ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
if (!$unchanged -or !$report.PackagePathRestored -or @($results | Where-Object { !$_.Passed }).Count -or
    @($headersOnly | Where-Object { !$_.Passed }).Count) { throw 'All public interop phases collected; failures remain' }

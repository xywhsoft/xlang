param([string]$Root = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if (!$Directory) { $Directory = Join-Path $Root ('build/acceptance/core-' + [Guid]::NewGuid().ToString('N')) }
$Directory = [IO.Path]::GetFullPath($Directory)
if (Test-Path -LiteralPath $Directory) { throw 'Use a fresh directory; preserve previous batch evidence' }
[void][IO.Directory]::CreateDirectory($Directory)
$publicDirectory = Join-Path $Directory 'public-source'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build_xlir_public_source_driver.ps1') -Directory $publicDirectory
if ($LASTEXITCODE -ne 0) { throw 'Current-source driver build failed before grouped regression' }
$guards = @(
    'check_docs_index.ps1', 'check_cgenrule_docs.ps1', 'check_tool_wiring.ps1',
    'check_source_wiring.ps1', 'check_xrt_snapshot.ps1', 'check_xrt_header_abi.ps1',
    'check_xlir_contract.ps1', 'check_cli_temp_cleanup.ps1',
    'check_xlir_cgen_determinism.ps1', 'check_xlir_cgen_runtime_matrix.ps1',
    'check_xlir_extern_global_boundary.ps1', 'check_xlir_null_abi_boundary.ps1',
    'check_xlir_callsite_c_vararg.ps1', 'check_xlir_native_handle_bind.ps1',
    'check_xlir_value_initializer.ps1', 'check_xlir_product_result.ps1',
    'check_xlir_named_function_adapter.ps1', 'check_xlir_collection_member_intrinsics.ps1',
    'check_xlir_collection_algebra.ps1', 'check_xlir_xvalue_array_iterator_projection.ps1',
    'check_xlir_default_backend_boundary.ps1', 'check_xlir_checked_time_conversion.ps1',
    'check_xlir_checked_float_conversion.ps1', 'check_xlir_string_conversion_abi.ps1',
    'check_xlir_async_source_lease.ps1', 'check_xlir_future_payload.ps1'
)
# Freeze compiler sources and released hosts. Guards may regenerate ignored
# test artifacts, but must not silently mix two compiler/ABI revisions.
$files = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h') | ForEach-Object { Get-Item (Join-Path $Root $_) }
$files += @(Get-ChildItem (Join-Path $Root 'src') -Recurse -File)
$files += @(Get-ChildItem (Join-Path $Root 'tools') -File -Filter '*.ps1')
$files += @(Get-ChildItem (Join-Path $Root 'test/cases') -File)
$files += @(Get-Item (Join-Path $Root 'test/suites/common.bat'))
$inputs = @($files | ForEach-Object { [ordered]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$report = [ordered]@{Scope='Focused grouped repair regression, not full RC0';StartedUtc=[DateTime]::UtcNow.ToString('o');
    Status='running';Inputs=$inputs;InputsUnchanged=$null;Results=@()}
function Save-Report {
    [IO.File]::WriteAllText((Join-Path $Directory 'results.json'), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
}
Save-Report
Push-Location $Root
try {
    foreach ($guard in $guards) {
        $log = Join-Path $Directory ($guard + '.log')
        # Windows PowerShell wraps native stderr as ErrorRecords. Preserve it as
        # log data and observe the child's actual exit code, then keep collecting.
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot $guard))
            if ($guard -eq 'check_xlir_default_backend_boundary.ps1') {
                $arguments += @('-PublicDirectory', $publicDirectory, '-Directory', (Join-Path $Directory 'default-backend'))
            }
            & powershell.exe @arguments *> $log
            $code = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        $report.Results += [ordered]@{Guard=$guard;Exit=$code;Passed=($code -eq 0);Log=$log}
        Save-Report
        Write-Output "$guard exit=$code"
    }
    $report.InputsUnchanged = @($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
    $report.Status = if ($report.InputsUnchanged -and !@($report.Results | Where-Object { !$_.Passed }).Count) { 'passed' } else { 'failed' }
} finally {
    if ($report.Status -eq 'running') { $report.Status = 'interrupted' }
    $report.CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Save-Report
    Pop-Location
}
if ($report.Status -ne 'passed') { throw 'Grouped core regression failures retained; see results.json' }

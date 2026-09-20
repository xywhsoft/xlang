param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $headers = @('xl6_ownership_scope.h','xl6_ownership_plan.h','xl6_code_owner.h','xl6_code_lease.h','xl6_closure.h','xl6_cell.h','xl6_slot.h','xl6_frame_storage.h','xl6_task.h','xl6_capture.h','xl6_value_iterator.h','xl6_future_callback.h','xl6_future_aggregate.h','xl6_generator.h','xl6_continuation_queue.h',
        'xl6_continuation_executor.h','xl6_continuation_future.h','xl6_future_payload.h','xl6_network_service.h')
    $sources = @('xl6_ownership_plan.c','xl6_code_lease.c','xl6_closure.c','xl6_cell.c','xl6_slot.c','xl6_frame_storage.c','xl6_continuation_frame.c','xl6_future_wait.c','xl6_task.c','xl6_capture.c','xl6_value_iterator.c','xl6_future_callback.c','xl6_generator.c','xl6_code_finalizer.c','xl6_continuation_queue.c','xl6_continuation_executor.c',
        'xl6_continuation_future.c','xl6_future_payload.c','xl6_network_service.c')
    $failed = @()
    foreach ($platform in @('win64','linux')) { foreach ($product in @('runtime','xlc')) {
        $manifest = "tools/tcc_vfs_manifest_${platform}_${product}.txt"
        $entries = @([IO.File]::ReadAllLines($manifest) | Where-Object { $_.StartsWith('file|') } | ForEach-Object { $_.Replace('\','/') })
        foreach ($header in $headers) {
            $prefix = "file|include/__xl_runtime/$header|"
            $match = @($entries | Where-Object { $_.StartsWith($prefix) })
            if ($match.Count -ne 1 -or $match[0] -ne "$prefix%XL6_ROOT%/src/runtime/$header") { $failed += "$manifest : missing/duplicate/wrong $header" }
            if (!(Test-Path "src/runtime/$header" -PathType Leaf)) { $failed += "source header missing: $header" }
        }
    } }
    $windows = [IO.File]::ReadAllText('build_dll.bat').Replace('\','/')
    $linux = [IO.File]::ReadAllText('build_linux.sh')
    $api = [IO.File]::ReadAllText('src/api/xl_api.c')
    $embedded = [regex]::Match($api, '(?s)static int xlAddEmbeddedXlangRuntimeSources\(.*?static const char\* runtimeSources\[\] = \{(.*?)\};').Groups[1].Value
    foreach ($source in $sources) {
        $pattern = [regex]::Escape("src/runtime/$source")
        if ([regex]::Matches($windows, $pattern).Count -ne 2) { $failed += "DLL variants do not both contain $source" }
        if ([regex]::Matches($linux, $pattern).Count -ne 1) { $failed += "Linux source list missing/duplicate $source" }
        if ([regex]::Matches($embedded, $pattern).Count -ne 1) { $failed += "Embedded xlang runtime missing/duplicate $source" }
    }
    if ($failed.Count) { $failed | Write-Output; throw "Runtime packaging collected $($failed.Count) violations" }
} finally { Pop-Location }
Write-Output "Runtime packaging: $($headers.Count * 4) private-header mappings and $($sources.Count * 4) source-list placements verified; no Windows/Linux release build or execution claim."

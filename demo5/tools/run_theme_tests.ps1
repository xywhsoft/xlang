param(
    [Parameter(Mandatory = $true)]
    [string]$Theme,

    [switch]$Rebuild,

    [switch]$FreshRunner
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$binDir = Join-Path $root "release\bin"
$tmpDir = Join-Path $root "release\tmp"
$runner = Join-Path $binDir "xl5_test.exe"
$buildStampHeader = Join-Path $tmpDir "xl_test_build_stamp.h"

$themes = @{
    "task-future-channel" = @(
        "task_sync_",
        "task_param_runtime",
        "task_return_runtime",
        "future_",
        "parser.var_future_runtime",
        "sem.var_future_runtime",
        "lower.var_future_runtime",
        "cgen.var_future_runtime",
        "runtime.var_future_runtime",
        "parser.var_task_runtime",
        "sem.var_task_runtime",
        "lower.var_task_runtime",
        "cgen.var_task_runtime",
        "runtime.var_task_runtime",
        "runtime.channel_",
        "runtime.var_channel_methods_basic",
        "runtime.object_collect_channel_"
    )
    "future-task-graph" = @(
        "parser.var_future_runtime",
        "sem.var_future_runtime",
        "lower.var_future_runtime",
        "cgen.var_future_runtime",
        "runtime.var_future_runtime",
        "parser.var_task_runtime",
        "sem.var_task_runtime",
        "lower.var_task_runtime",
        "cgen.var_task_runtime",
        "runtime.var_task_runtime",
        "parser.task_param_runtime",
        "sem.task_param_runtime",
        "lower.task_param_runtime",
        "cgen.task_param_runtime",
        "runtime.task_param_runtime",
        "parser.task_return_runtime",
        "sem.task_return_runtime",
        "lower.task_return_runtime",
        "cgen.task_return_runtime",
        "runtime.task_return_runtime",
        "parser.future_relay_runtime",
        "sem.future_relay_runtime",
        "lower.future_relay_runtime",
        "cgen.future_relay_runtime",
        "runtime.future_relay_runtime",
        "parser.object_collect_channel_runtime",
        "sem.object_collect_channel_runtime",
        "lower.object_collect_channel_runtime",
        "cgen.object_collect_channel_runtime",
        "runtime.object_collect_channel_runtime",
        "parser.object_collect_task_runtime",
        "sem.object_collect_task_runtime",
        "lower.object_collect_task_runtime",
        "cgen.object_collect_task_runtime",
        "runtime.object_collect_task_runtime",
        "parser.object_collect_future_runtime",
        "sem.object_collect_future_runtime",
        "lower.object_collect_future_runtime",
        "cgen.object_collect_future_runtime",
        "runtime.object_collect_future_runtime",
        "runtime.object_collect_channel_cycles_basic",
        "runtime.channel_"
    )
    "object-custom-graph" = @(
        "parser.object_",
        "sem.object_",
        "cgen.object_",
        "runtime.object_runtime_basic",
        "runtime.object_runtime_lazy_init",
        "runtime.object_runtime_type_dispatch",
        "runtime.object_runtime_disposed_guard",
        "runtime.object_refcount_basic",
        "runtime.object_weak_ref_basic",
        "runtime.object_collect_cycles_basic",
        "runtime.object_collect_container_cycles_basic",
        "runtime.object_collect_closure_cycles_basic",
        "runtime.object_collect_channel_cycles_basic",
        "runtime.object_boxed_write_set_release",
        "runtime.object_boxed_write_set_stmt_expr",
        "runtime.object_boxed_write_set_inline_temp",
        "runtime.object_super_call_runtime",
        "runtime.object_class_methods_runtime",
        "runtime.object_dynamic_member_runtime",
        "runtime.object_index_runtime",
        "runtime.object_inherit_dispatch_runtime",
        "runtime.object_base_bind_dispatch_runtime",
        "runtime.object_base_assign_dispatch_runtime",
        "runtime.object_param_return_dispatch_runtime",
        "runtime.object_inherit_create_fields_runtime",
        "runtime.object_clone_runtime",
        "runtime.object_for_in_runtime",
        "runtime.object_var_type_query_runtime",
        "runtime.object_var_box_clone_runtime",
        "runtime.object_var_methods_runtime",
        "runtime.object_var_operator_runtime",
        "runtime.object_weak_runtime",
        "runtime.object_refcount_runtime",
        "runtime.object_collect_runtime",
        "runtime.object_collect_container_runtime",
        "runtime.object_collect_closure_runtime",
        "runtime.object_collect_generator_runtime",
        "runtime.object_collect_channel_runtime",
        "runtime.object_collect_task_runtime",
        "runtime.object_collect_future_runtime",
        "runtime.object_reference_runtime",
        "parser.custom_",
        "sem.custom_",
        "cgen.custom_",
        "runtime.custom_",
        "parser.type_reflect_class_runtime",
        "sem.type_reflect_class_runtime",
        "cgen.type_reflect_class_runtime",
        "runtime.type_reflect_class_runtime",
        "parser.var_generator_runtime",
        "sem.var_generator_runtime",
        "lower.var_generator_runtime",
        "cgen.var_generator_runtime",
        "runtime.var_generator_runtime"
    )
    "native-abi" = @(
        "binding_",
        "extern_",
        "link_",
        "sys_dl",
        "runtime.binding_",
        "runtime.native_"
    )
}

if (-not $themes.ContainsKey($Theme)) {
    Write-Error "Unknown theme '$Theme'. Available themes: $($themes.Keys -join ', ')"
}

if ($Rebuild) {
    Write-Host "[theme] rebuilding demo5..."
    cmd /c (Join-Path $root "build_gcc.bat")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path $runner)) {
    Write-Error "Runner not found: $runner"
}

if (-not (Test-Path $buildStampHeader)) {
    Write-Error "Build stamp header not found: $buildStampHeader"
}

function Get-ExpectedBuildStamp {
    param([string]$HeaderPath)

    $line = Select-String -Path $HeaderPath -Pattern '^#define XL_TEST_BUILD_ID "(.*)"$' | Select-Object -First 1
    if ($null -eq $line) {
        throw "Failed to parse build stamp header: $HeaderPath"
    }
    return $line.Matches[0].Groups[1].Value
}

function Get-RunnerBuildStamp {
    param([string]$RunnerPath)

    $output = & $RunnerPath "__codex_build_stamp_probe__" 2>&1
    if ($LASTEXITCODE -ne 1) {
        throw "Unexpected runner probe exit code $LASTEXITCODE from $RunnerPath"
    }
    $line = $output | Where-Object { $_ -like 'build_stamp=*' } | Select-Object -First 1
    if ([string]::IsNullOrEmpty($line)) {
        throw "Runner did not print build_stamp: $RunnerPath"
    }
    return $line.Substring("build_stamp=".Length)
}

$expectedBuildStamp = Get-ExpectedBuildStamp -HeaderPath $buildStampHeader
$execPath = $runner
if ($FreshRunner) {
    if (-not (Test-Path $tmpDir)) {
        New-Item -ItemType Directory -Path $tmpDir | Out-Null
    }
    $copied = $false
    for ($attempt = 1; $attempt -le 3; $attempt += 1) {
        $stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
        $candidate = Join-Path $tmpDir ("xl5_test_{0}_{1}.exe" -f ($Theme -replace "[^A-Za-z0-9_-]", "_"), $stamp)
        Copy-Item $runner $candidate -Force
        $candidateStamp = Get-RunnerBuildStamp -RunnerPath $candidate
        if ($candidateStamp -eq $expectedBuildStamp) {
            $execPath = $candidate
            $copied = $true
            break
        }
        Remove-Item $candidate -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 200
    }
    if (-not $copied) {
        Write-Error "Fresh runner build stamp mismatch after retries. expected=$expectedBuildStamp"
    }
} else {
    $runnerStamp = Get-RunnerBuildStamp -RunnerPath $execPath
    if ($runnerStamp -ne $expectedBuildStamp) {
        Write-Error "Runner build stamp mismatch. expected=$expectedBuildStamp actual=$runnerStamp"
    }
}

Write-Host "[theme] theme=$Theme"
Write-Host "[theme] runner=$execPath"
Write-Host "[theme] build_stamp=$expectedBuildStamp"

$aggregateSelector = "theme.$Theme"
Write-Host ""
Write-Host "[theme] selector=$aggregateSelector"
& $execPath $aggregateSelector
if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "[theme] aggregate selector passed for $Theme"
    exit 0
}
Write-Host "[theme] aggregate selector failed or was unstable for $Theme, falling back to per-selector execution (rc=$LASTEXITCODE)"

$failed = $false
foreach ($selector in $themes[$Theme]) {
    Write-Host ""
    Write-Host "[theme] selector=$selector"
    & $execPath $selector
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
        Write-Host "[theme] selector failed: $selector"
    }
}

if ($failed) {
    exit 1
}

Write-Host ""
Write-Host "[theme] all selectors passed for $Theme"

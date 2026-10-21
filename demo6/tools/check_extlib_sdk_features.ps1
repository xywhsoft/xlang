param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')

$work = Join-Path $Root 'build/test/extlib_sdk_features'
$hostRoot = Join-Path $work 'host'
[IO.Directory]::CreateDirectory($hostRoot) | Out-Null
$checks = [Collections.Generic.List[object]]::new()
$manifest = 'tools/tcc_vfs_manifest_win64_xlc.txt'
$virtualPaths = @(
    'include/__xl_runtime/xruntime_decl.h',
    'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h'
)
$assets = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $Root $manifest)) {
    $fields = $line.Split('|')
    if ($fields.Count -eq 3 -and $fields[0] -eq 'file' -and $fields[1] -in $virtualPaths) {
        if ($assets.ContainsKey($fields[1])) { throw "Duplicate VFS asset: $($fields[1])" }
        $assets[$fields[1]] = $fields[2].Replace('%XL6_ROOT%', $Root)
    }
}
if ($assets.Count -ne $virtualPaths.Count) { throw 'VFS declaration/implementation assets are incomplete' }

$paths = @(
    'release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h',
    'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'lib/xrt/xrt.snapshot.json', 'lib/xruntime/xruntime.snapshot.json',
    'test/cases/extlib_sdk_features.xl', 'test/cases/extlib_sdk_features_c.c',
    'test/fixtures/sdk_feature_probe/xlang.package.json',
    'test/fixtures/sdk_feature_probe/sdk_features.xl',
    'test/fixtures/sdk_feature_probe/sdk_features_native.c',
    'tools/check_extlib_sdk_features.ps1', 'tools/extlib_smoke_process.ps1',
    'tools/xlir_native_process.ps1', $manifest
)
function Snapshot {
    $result = [ordered]@{}
    foreach ($path in $paths) { $result[$path] = (Get-FileHash (Join-Path $Root $path)).Hash }
    return $result
}
$before = Snapshot
Copy-Item -LiteralPath (Join-Path $Root 'release/xl.exe') -Destination (Join-Path $hostRoot 'xl.exe') -Force
foreach ($virtualPath in $virtualPaths) {
    $target = Join-Path $hostRoot $virtualPath
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    Copy-Item -LiteralPath $assets[$virtualPath] -Destination $target -Force
}

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [bool]$ExpectOutput) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $hostRoot 90
    $checks.Add([ordered]@{name=$Name; program=$Program; arguments=$Arguments; result=$r})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr) $($r.stdout)" }
    if ($ExpectOutput -and ($r.stdout.Replace("`r", '') -cne "SDK selected features ok`n" -or $r.stderr.Length)) {
        throw "$Name output mismatch"
    }
    Write-Host "[sdk-features] ${Name}: PASS"
}

$xl = Join-Path $hostRoot 'xl.exe'
$oldPackagePath = $env:XLANG_PACKAGE_PATH
$outcome = 'failed'
$stable = $false
try {
    $env:XLANG_PACKAGE_PATH = Join-Path $Root 'test/fixtures'
    foreach ($language in @('c', 'xl')) {
        $source = if ($language -eq 'c') { 'test/cases/extlib_sdk_features_c.c' } else { 'test/cases/extlib_sdk_features.xl' }
        $source = Join-Path $Root $source
        $output = Join-Path $work "$language-aot.exe"
        Check "$language-jit" $xl @('run', $source) $true
        Check "$language-build" $xl @('build', $source, '-o', $output) $false
        Check "$language-aot" $output @() $true
    }
    $after = Snapshot
    foreach ($path in $before.Keys) {
        if ($after[$path] -cne $before[$path]) { throw "Input changed: $path" }
    }
    $stable = $true
    $outcome = 'passed'
} finally {
    $env:XLANG_PACKAGE_PATH = $oldPackagePath
    $result = [ordered]@{
        date=[DateTimeOffset]::Now.ToString('o'); outcome=$outcome
        scope='Frozen Windows CLI with current manifest-selected external SDK assets; C and xlang package JIT/AOT; percent codec, zlib and typed-array calls. Not a rebuilt or VFS-only release certification.'
        inputs=$before; inputsStable=$stable; checks=@($checks.ToArray())
    }
    [IO.File]::WriteAllText((Join-Path $work 'results.json'), ($result | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
}

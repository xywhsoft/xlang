param([string]$Root = '', [string]$RuntimeDirectory = '', [string]$WorkDirectory = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$work = if ($WorkDirectory) { [IO.Path]::GetFullPath($WorkDirectory) } else { Join-Path $Root 'build/test/extlib_file_runtime' }
[IO.Directory]::CreateDirectory($work) | Out-Null
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$fixture = Join-Path $Root 'test/cases/extlib_file_open_oom.c'
$includes = @('-I' + (Join-Path $Root 'lib/xrt'))
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror') + $includes
$checks = [Collections.Generic.List[object]]::new()
$inputs = [ordered]@{}
foreach ($path in @($gcc,$fixture,$PSCommandPath,$xl,
    (Join-Path $RuntimeDirectory 'xl.dll'),(Join-Path $RuntimeDirectory 'xlc.dll'),
    (Join-Path $Root 'lib/xrt/xrt.h'),(Join-Path $Root 'lib/xruntime/xruntime.h'),
    (Join-Path $Root 'lib/xrt/xrt.snapshot.json'),(Join-Path $Root 'lib/xruntime/xruntime.snapshot.json'),
    (Join-Path $Root 'tools/extlib_smoke_process.ps1'),(Join-Path $Root 'tools/xlir_native_process.ps1'))) {
    $inputs[$path] = (Get-FileHash -LiteralPath $path).Hash
}

# 每层都执行，不能用独立 C 或 AOT 的通过掩盖 DLL/JIT 的失败。
function Check([string]$Name, [string]$Program, [string[]]$Arguments, [string]$Directory = $Root) {
    $result = Invoke-ExtlibSmokeProcess $Program $Arguments $Directory 120
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;directory=$Directory;result=$result})
    Write-Host "[file-runtime] ${Name}: $($result.status)"
    return $result.status -eq 'pass'
}

$outcome = 'failed'
try {
    $binary = Join-Path $work 'source.exe'
    if (Check 'source-build' $gcc ($flags + @('-DXRT_IMPLEMENTATION',$fixture,'-lWs2_32','-lAdvapi32','-lm','-o',$binary))) {
        $null = Check 'source-runtime' $binary @()
    }
    foreach ($name in @('xl','xlc')) {
        $library = Join-Path $RuntimeDirectory "$name.dll"
        Copy-Item -LiteralPath $library -Destination (Join-Path $work "$name.dll") -Force
        $binary = Join-Path $work "dll-$name.exe"
        if (Check "$name-build" $gcc ($flags + @($fixture,$library,'-o',$binary))) {
            $null = Check "$name-runtime" $binary @()
        }
    }
    $options = @('--inc-path',(Join-Path $Root 'lib/xrt'),'--inc-path',(Join-Path $Root 'lib/xruntime'))
    $null = Check 'c-jit' $xl (@('run',$fixture) + $options)
    $binary = Join-Path $work 'aot.exe'
    if (Check 'c-aot-build' $xl (@('build',$fixture) + $options + @('-o',$binary))) {
        $null = Check 'c-aot-runtime' $binary @()
    }
    # 只携带二进制与用例，不能由仓库的外部头文件掩盖静态 VFS 中的旧实现。
    $standalone = Join-Path $work ('vfs-'+[Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($standalone) | Out-Null
    foreach ($name in @('xl.exe','xlc.dll')) {
        Copy-Item -LiteralPath (Join-Path $RuntimeDirectory $name) -Destination $standalone
    }
    Copy-Item -LiteralPath $fixture -Destination (Join-Path $standalone 'probe.c')
    $standaloneXl = Join-Path $standalone 'xl.exe'
    $before = @(Get-ChildItem -LiteralPath $standalone -Recurse | ForEach-Object FullName)
    $null = Check 'vfs-jit' $standaloneXl @('run','probe.c') $standalone
    $after = @(Get-ChildItem -LiteralPath $standalone -Recurse | ForEach-Object FullName)
    if (@(Compare-Object $before $after).Count) { throw 'VFS JIT created or removed filesystem artifacts' }
    if (Check 'vfs-aot-build' $standaloneXl @('build','probe.c','-o','probe.exe') $standalone) {
        $null = Check 'vfs-aot-runtime' (Join-Path $standalone 'probe.exe') @() $standalone
    }
    if (@($checks | Where-Object { $_.result.status -ne 'pass' }).Count -eq 0) { $outcome = 'passed' }
} finally {
    $changed = @($inputs.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $inputs[$_] })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=Get-Date -Format o;outcome=$outcome;
        scope='Same C file-open fault sweep against source, xl.dll, xlc.dll, C JIT/AOT, and standalone static VFS without external headers; all layers must pass';
        runtimeDirectory=$RuntimeDirectory;inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
}
if ($outcome -ne 'passed') { throw "File runtime gate failed; see $work/results.json" }

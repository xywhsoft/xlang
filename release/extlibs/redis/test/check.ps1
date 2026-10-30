param([switch]$Stress, [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('redis_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$failure = ''
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'tools/run_extlib_c.ps1', 'test/cases/extlib_redis.xl',
    'test/cases/xlir_extlib_redis_current_tcp.xl', 'release/examples/extlibs/redis.xl',
    'release/examples/extlibs/redis_c.c') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $release 'extlibs/redis') -Recurse -File |
    ForEach-Object FullName)
$inputs += @($gcc, $shell, (Join-Path $root 'tools/extlib_smoke_process.ps1'),
    (Join-Path $root 'tools/xlir_native_process.ps1'))
$hashes = @{}

function Run([string]$label, [string]$program, [string[]]$arguments,
    [int]$expectedExit = 0, [ValidateRange(1,3600)][int]$timeoutSeconds = 120) {
    if ($null -eq $arguments) { $arguments = @() }
    # C 运行器也是独立进程，不能沿用父 PowerShell 的旧 LASTEXITCODE。
    if ([IO.Path]::GetExtension($program) -ieq '.ps1') {
        $arguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
            '-File', $program) + $arguments
        $program = $shell
    }
    $directory = (Get-Location).Path
    $result = Invoke-ExtlibSmokeProcess $program $arguments $directory $timeoutSeconds
    $checks.Add([ordered]@{label=$label; program=$program; arguments=$arguments;
        directory=$directory; expectedExit=$expectedExit; result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $work 'report.json') -Encoding UTF8
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${label}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass', 'fail') -or $result.exitCode -ne $expectedExit) {
        throw "${label}: $($result.status) exit=$($result.exitCode), expected=$expectedExit`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[redis] ${label}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    if (-not (Test-Path -LiteralPath $cc1 -PathType Leaf)) { throw "GCC cc1 not found: $cc1" }
    $cc1 = (Resolve-Path -LiteralPath $cc1).Path
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'process diagnostics' (Join-Path $PSScriptRoot 'check_gate_diagnostics.ps1') @()
    $null = Run 'strict C syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt',
        'release/extlibs/redis/redis_native.c',
        'release/extlibs/redis/test/test_redis.c',
        'release/extlibs/redis/test/test_redis_alloc.c',
        'release/extlibs/redis/test/test_redis_threads.c',
        'release/extlibs/redis/test/test_example_lifetime.c')
    $null = Run 'C boundaries' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/extlibs/redis/test/test_redis.c')
    $null = Run 'C allocation failures' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/extlibs/redis/test/test_redis_alloc.c')
    $null = Run 'C threads' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/extlibs/redis/test/test_redis_threads.c')
    $null = Run 'C example ownership contract' $xl @('run',
        'release/extlibs/redis/test/test_example_lifetime.c')
    # 真正用 GCC 构建/运行 C 范例，不能把 C 运行器的 TCC 输出称为 GCC。
    $runtime = Join-Path $work 'xrt.o'
    $gccExample = Join-Path $work 'redis_gcc_example.exe'
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $null = Run 'GCC runtime build' $gcc ($flags + @('-DXRT_MODULE_MEMORY_DEBUG', '-c',
        'release/extlibs/redis/test/live_runtime.c', '-o', $runtime))
    $null = Run 'GCC C example build' $gcc ($flags + @(
        'release/examples/extlibs/redis_c.c', $runtime, '-o', $gccExample,
        '-lws2_32', '-lbcrypt', '-lshlwapi', '-liphlpapi'))
    $gccOutput = Run 'GCC C example' $gccExample @()
    $cExample = Run 'C example' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/examples/extlibs/redis_c.c')
    $cDirect = Run 'C example xl run' $xl @('run',
        'release/examples/extlibs/redis_c.c')
    if ($cDirect -cne $cExample -or $cDirect -cne $gccOutput) {
        throw '[redis] C runner, xl run and GCC example outputs differ'
    }
    Push-Location $release
    try {
        $null = Run 'XLang central case' $xl @('run', '../test/cases/extlib_redis.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/redis/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run',
            'extlibs/redis/test/allocations.xl')
        $preflight = Run 'TCP preflight' $xl @('run',
            'extlibs/redis/test/preflight_repro.xl')
        $null = Run 'TCP current API' $xl @('run',
            '../test/cases/xlir_extlib_redis_current_tcp.xl')
        $reuseOutput = Run 'TCP reuse' $xl @('run', 'extlibs/redis/test/reuse_loopback.xl')
        $commandOutput = Run 'TCP fragmented/error/deadline' $xl @('run',
            'extlibs/redis/test/command_loopback.xl')
        if ($preflight -cne 'redis preflight: ok' -or
            $reuseOutput -cne 'redis reuse loopback: 500' -or
            $commandOutput -cne 'redis command loopback: ok') {
            throw '[redis] TCP test output differs from fixed expectation'
        }
        if ($Stress) {
            $null = Run 'TCP reuse 100000' $xl @('run',
                'extlibs/redis/test/reuse_loopback.xl', '--', '100000') 0 600
        }
        $xlExample = Run 'XLang example' $xl @('run', 'examples/extlibs/redis.xl')
        if (-not $SkipAot) {
            [IO.Directory]::CreateDirectory($work) | Out-Null
            $core = Join-Path $work 'redis_core.exe'
            $null = Run 'XLang core build' $xl @('build',
                'extlibs/redis/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            foreach ($case in @('preflight_repro', 'command_loopback', 'reuse_loopback')) {
                $target = Join-Path $work "$case.exe"
                $null = Run "TCP $case build" $xl @('build',
                    "extlibs/redis/test/$case.xl", '-o', $target)
                $output = Run "TCP $case AOT" $target @()
                $expected = switch ($case) {
                    'preflight_repro' { 'redis preflight: ok' }
                    'command_loopback' { 'redis command loopback: ok' }
                    'reuse_loopback' { 'redis reuse loopback: 500' }
                }
                if ($output -cne $expected) {
                    throw "[redis] TCP $case AOT output differs from fixed expectation"
                }
            }
            $example = Join-Path $work 'redis_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/redis.xl', '-o', $example)
            $aotExample = Run 'XLang example AOT' $example @()
            if ($aotExample -cne $cExample) {
                throw '[redis] C/XLang AOT example output differs'
            }
            $cTarget = Join-Path $work 'redis_c_example.exe'
            $null = Run 'C example AOT build' $xl @('build',
                'examples/extlibs/redis_c.c', '-o', $cTarget)
            $cAotExample = Run 'C example AOT' $cTarget @()
            if ($cAotExample -cne $cExample) {
                throw '[redis] C AOT example output differs from GCC'
            }
        }
    } finally { Pop-Location }
    if ($cExample -cne $xlExample) { throw '[redis] C/XLang example output differs' }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "[redis] input changed during gate: $path"
        }
    }
    Write-Host '[redis] C/XLang example parity: PASS'
    $passed = $true
} catch {
    $failure = $_.Exception.Message
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'), $failure,
        [Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "redis temporary work path escaped build/test: $resolved"
        }
        [ordered]@{passed=$passed; failure=$failure; inputs=$hashes; checks=$checks} |
            ConvertTo-Json -Depth 10 | Set-Content -LiteralPath ($work + '.report.json') -Encoding UTF8
        if ($passed) { Remove-Item -LiteralPath $resolved -Recurse -Force }
        else { Write-Warning "Redis failure artifacts: $resolved" }
    }
}

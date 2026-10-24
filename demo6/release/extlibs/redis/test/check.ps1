param([switch]$Stress, [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('redis_guard_' + [guid]::NewGuid().ToString('N'))

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[redis] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict C syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt',
        'release/extlibs/redis/redis_native.c',
        'release/extlibs/redis/test/test_redis.c',
        'release/extlibs/redis/test/test_redis_alloc.c',
        'release/extlibs/redis/test/test_redis_threads.c')
    $null = Run 'C boundaries' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/extlibs/redis/test/test_redis.c')
    $null = Run 'C allocation failures' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/extlibs/redis/test/test_redis_alloc.c')
    $null = Run 'C threads' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/extlibs/redis/test/test_redis_threads.c')
    $cExample = Run 'C example' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'redis', 'release/examples/extlibs/redis_c.c')
    Push-Location $release
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/redis/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run',
            'extlibs/redis/test/allocations.xl')
        $null = Run 'TCP current API' $xl @('run',
            '../test/cases/xlir_extlib_redis_current_tcp.xl')
        $null = Run 'TCP fragmented/error/deadline' $xl @('run',
            'extlibs/redis/test/command_loopback.xl')
        $null = Run 'TCP reuse' $xl @('run', 'extlibs/redis/test/reuse_loopback.xl')
        if ($Stress) {
            $null = Run 'TCP reuse 100000' $xl @('run',
                'extlibs/redis/test/reuse_loopback.xl', '--', '100000')
        }
        $xlExample = Run 'XLang example' $xl @('run', 'examples/extlibs/redis.xl')
        if (-not $SkipAot) {
            [IO.Directory]::CreateDirectory($work) | Out-Null
            $core = Join-Path $work 'redis_core.exe'
            $null = Run 'XLang core build' $xl @('build',
                'extlibs/redis/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            $example = Join-Path $work 'redis_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/redis.xl', '-o', $example)
            $aotExample = Run 'XLang example AOT' $example @()
            if ($aotExample -cne $cExample) {
                throw '[redis] C/XLang AOT example output differs'
            }
        }
    } finally { Pop-Location }
    if ($cExample -cne $xlExample) { throw '[redis] C/XLang example output differs' }
    Write-Host '[redis] C/XLang example parity: PASS'
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "redis temporary work path escaped build/test: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

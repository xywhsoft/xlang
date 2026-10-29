param([switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $buildRoot ('tar_writer_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
[void][IO.Directory]::CreateDirectory($work)
$passed = $false
$hashes = @{}
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/tar') -Recurse -File |
    ForEach-Object { $_.FullName })
foreach ($dependency in @('gzip', 'bz2', '_bzip2', 'lzma')) {
    $inputs += Get-ChildItem -LiteralPath (Join-Path $root "release/extlibs/$dependency") -Recurse -File |
        Where-Object { $_.Extension -in @('.c', '.h', '.xl') -or $_.Name -eq 'xlang.package.json' } |
        ForEach-Object { $_.FullName }
}
$inputs += @('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/xl.exe',
    'release/xl.dll', 'release/xlc.dll') | ForEach-Object { Join-Path $root $_ }
foreach ($path in @($inputs | Sort-Object -Unique)) {
    $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash
    Add-Content -LiteralPath (Join-Path $work 'inputs.sha256') -Value "$($hashes[$path])  $path"
}
function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $prior }
    Add-Content -LiteralPath (Join-Path $work 'commands.log') -Value @(
        "[$label] cwd=$pwd", "$program $($arguments -join ' ')", $output, "exit=$code")
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[tar writer] ${label}: PASS"
}
Push-Location $root
try {
    Run 'GCC version' $gcc @('--version')
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DXRT_MODULE_MEMORY_DEBUG',
        '-Ilib/xrt', '-Irelease/extlibs/tar')
    $sources = @('release/extlibs/tar/xrt_tar_common.c', 'release/extlibs/tar/xrt_tar_reader.c',
        'release/extlibs/tar/xrt_tar_writer.c', 'release/extlibs/tar/test/runtime.c')
    foreach ($test in @('test_writer_allocations', 'test_tar_c', 'closed_transaction_oom')) {
        $binary = Join-Path $work "$test.exe"
        $arguments = $flags + $sources + @("release/extlibs/tar/test/$test.c", '-o', $binary,
            '-lws2_32', '-lbcrypt', '-lshlwapi')
        if ($test -eq 'test_tar_c') { $arguments += 'release/extlibs/tar/xrt_tar_extract.c' }
        Run "$test strict C build" $gcc $arguments
    }
    $probe = Join-Path $work 'test_writer_allocations.exe'
    Run 'C faults, snapshots, atomic files and no-allocation destruction' $probe @($work)
    Run 'existing C TAR suite' (Join-Path $work 'test_tar_c.exe') @()
    Run 'XLang allocation and transaction boundaries JIT' $xl @('run',
        'release/extlibs/tar/test/writer_allocations.xl', '--', $work)
    Run 'JIT temporary cleanup' $probe @($work, '--check-clean')
    if (-not $SkipAot) {
        $aot = Join-Path $work 'writer_xl.exe'
        Run 'XLang allocation AOT build' $xl @('build',
            'release/extlibs/tar/test/writer_allocations.xl', '-o', $aot)
        Run 'XLang allocation and transaction boundaries AOT' $aot @($work)
        Run 'AOT temporary cleanup' $probe @($work, '--check-clean')
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "TAR writer validation input changed: $path"
        }
    }
    # 必须执行已关闭事务的回滚探针；不能用前面通过的矩阵掩盖该风险。
    Run 'closed transaction rollback OOM' (Join-Path $work 'closed_transaction_oom.exe') @($work)
    $passed = $true
    Write-Host $(if ($SkipAot) { '[tar writer] C/JIT checks: PASS (AOT skipped)' }
        else { '[tar writer] C/JIT/AOT checks: PASS' })
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($work)
    if (-not $resolved.StartsWith($buildRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolved).StartsWith('tar_writer_')) {
        throw 'TAR writer directory escaped build/test'
    }
    if ($passed) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    else { Write-Warning "TAR writer failure artifacts retained: $resolved" }
}

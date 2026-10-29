param([switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $buildRoot ('tar_reader_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
[void][IO.Directory]::CreateDirectory($work)
$passed = $false
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/tar') -Recurse -File |
    ForEach-Object { $_.FullName })
foreach ($dependency in @('gzip', 'bz2', '_bzip2', 'lzma')) {
    $inputs += Get-ChildItem -LiteralPath (Join-Path $root "release/extlibs/$dependency") -Recurse -File |
        Where-Object { $_.Extension -in @('.c', '.h', '.xl') -or $_.Name -eq 'xlang.package.json' } |
        ForEach-Object { $_.FullName }
}
$inputs += @('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/xl.exe',
    'release/xl.dll', 'release/xlc.dll') | ForEach-Object { Join-Path $root $_ }
$hashes = @{}
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
    Write-Host "[tar reader] ${label}: PASS"
}

Push-Location $root
try {
    Run 'GCC version' $gcc @('--version')
    $probe = Join-Path $work 'reader_c.exe'
    Run 'strict C allocation probe build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_MEMORY_DEBUG', '-Ilib/xrt', '-Irelease/extlibs/tar',
        'release/extlibs/tar/test/test_reader_allocations.c', 'release/extlibs/tar/xrt_tar_common.c',
        'release/extlibs/tar/xrt_tar_reader.c', 'release/extlibs/tar/xrt_tar_writer.c',
        'release/extlibs/tar/test/runtime.c', '-o', $probe, '-lws2_32', '-lbcrypt', '-lshlwapi')
    Run 'C faults, snapshots and extension tails' $probe @($work)
    Run 'XLang allocation and extension boundaries JIT' $xl @('run',
        'release/extlibs/tar/test/reader_allocations.xl', '--', $work)
    if (-not $SkipAot) {
        $aot = Join-Path $work 'reader_xl.exe'
        Run 'XLang allocation AOT build' $xl @('build',
            'release/extlibs/tar/test/reader_allocations.xl', '-o', $aot)
        Run 'XLang allocation and extension boundaries AOT' $aot @($work)
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "TAR reader validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host $(if ($SkipAot) { '[tar reader] C/JIT checks: PASS (AOT skipped)' }
        else { '[tar reader] C/JIT/AOT checks: PASS' })
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($work)
    if (-not $resolved.StartsWith($buildRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolved).StartsWith('tar_reader_')) {
        throw 'TAR reader directory escaped build/test'
    }
    if ($passed) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    else { Write-Warning "TAR reader failure artifacts retained: $resolved" }
}

param([switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$buildRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$build = Join-Path $buildRoot ('tar_numeric_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $release 'xl.exe'
[void][IO.Directory]::CreateDirectory($build)
$passed = $false
$log = Join-Path $build 'commands.log'
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $release 'extlibs/tar') -Recurse -File |
    ForEach-Object { $_.FullName })
$inputs += @('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/xl.exe',
    'release/xl.dll', 'release/xlc.dll') | ForEach-Object { Join-Path $root $_ }
foreach ($dependency in @('gzip', 'bz2', '_bzip2', 'lzma')) {
    $inputs += Get-ChildItem -LiteralPath (Join-Path $release "extlibs/$dependency") -Recurse -File |
        Where-Object { $_.Extension -in @('.c', '.h', '.xl') -or $_.Name -eq 'xlang.package.json' } |
        ForEach-Object { $_.FullName }
}
$inputs = @($inputs | Sort-Object -Unique)
$hashes = @{}
foreach ($file in $inputs) {
    $hashes[$file] = (Get-FileHash -LiteralPath $file).Hash
    Add-Content -LiteralPath (Join-Path $build 'inputs.sha256') -Value "$($hashes[$file])  $file"
}

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $prior }
    Add-Content -LiteralPath $log -Value @("[$label] cwd=$pwd", "$program $($arguments -join ' ')",
        $output, "exit=$code")
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[tar numeric] ${label}: PASS"
    return $output
}

Push-Location $root
try {
    [void](Run 'GCC version' $gcc @('--version'))
    [void](Run 'Python version' $python @('--version'))
    $probe = Join-Path $build 'numeric_c.exe'
    [void](Run 'strict C numeric probe build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Ilib/xrt', '-Irelease/extlibs/tar', 'release/extlibs/tar/test/test_numeric_fields.c',
        'release/extlibs/tar/xrt_tar_common.c', 'release/extlibs/tar/xrt_tar_reader.c',
        'release/extlibs/tar/xrt_tar_writer.c', 'release/extlibs/tar/test/runtime.c',
        '-o', $probe, '-lws2_32', '-lbcrypt', '-lshlwapi'))
    [void](Run 'C header range and unsigned ID matrix' $probe @())
    $cExample = Join-Path $build 'metadata_gcc.exe'
    [void](Run 'strict C metadata example build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Ilib/xrt', 'release/extlibs/tar/examples/metadata_c.c',
        'release/extlibs/tar/test/runtime.c', '-o', $cExample, '-lws2_32', '-lbcrypt', '-lshlwapi'))
    $expected = 'mtime=-1;uid=18446744073709551615;gid=9223372036854775808'
    $cOutput = @(Run 'C metadata example' $cExample @()) -join "`n"
    if ($cOutput -cne $expected) { throw 'C metadata output differs' }
    foreach ($name in @('negative_id_type', 'negative_gid_type')) {
        $prior = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $output = @(& $xl check "release/extlibs/tar/test/$name.xl" 2>&1 |
                ForEach-Object { $_.ToString() })
            $code = $LASTEXITCODE
        } finally { $ErrorActionPreference = $prior }
        Add-Content -LiteralPath $log -Value @("[$name]", $output, "exit=$code")
        if ($code -eq 0 -or ($output -join "`n") -notmatch '\[sem\].*TarWriter.add') {
            throw "negative unsigned ID was not rejected: $($output -join "`n")"
        }
        Write-Host "[tar numeric] $name compile-only: PASS"
    }
    [void](Run 'XLang full-width IDs' $xl @('run', 'release/extlibs/tar/test/numeric_ids.xl'))
    foreach ($source in @('metadata.xl', 'metadata_c.c')) {
        $output = @(Run "$source JIT" $xl @('run', "release/extlibs/tar/examples/$source")) -join "`n"
        if ($output -cne $expected) { throw "$source output differs from C" }
    }
    $generated = Join-Path $build 'numeric_ids.c'
    [void](Run 'C ABI cgen' $xl @('cgen', 'release/extlibs/tar/test/numeric_ids.xl', '--emit', $generated))
    $text = [IO.File]::ReadAllText($generated)
    foreach ($name in @('uid', 'gid')) {
        if ($text -notmatch "extern uint64_t xtar_reader_$name\(void\*\);") {
            throw "TAR $name ABI is not unsigned 64-bit"
        }
    }
    foreach ($name in @('mode', 'device_major', 'device_minor')) {
        if ($text -notmatch "extern unsigned int xtar_reader_$name\(void\*\);") {
            throw "TAR $name ABI is not C unsigned int"
        }
    }
    $aotArguments = @()
    if (-not $SkipAot) {
        $numeric = Join-Path $build 'numeric_fields.exe'
        [void](Run 'numeric fields AOT build' $xl @('build',
            'release/extlibs/tar/test/numeric_fields.xl', '-o', $numeric))
        $aotArguments = @('--aot', $numeric)
        foreach ($source in @('test/numeric_ids.xl', 'examples/metadata.xl', 'examples/metadata_c.c')) {
            $target = Join-Path $build (([IO.Path]::GetFileName($source)) + '.exe')
            [void](Run "$source AOT build" $xl @('build', "release/extlibs/tar/$source", '-o', $target))
            $output = @(Run "$source AOT" $target @()) -join "`n"
            $line = if ($source -eq 'test/numeric_ids.xl') { 'TAR uint64 identifiers: PASS' } else { $expected }
            if ($output -cne $line) { throw "$source AOT output differs" }
        }
    }
    $oracleArguments = @('release/extlibs/tar/test/numeric_fields.py',
        $release, $probe, $build) + $aotArguments
    [void](Run 'independent Python numeric oracle' $python $oracleArguments)
    foreach ($file in $inputs) {
        if ((Get-FileHash -LiteralPath $file).Hash -cne $hashes[$file]) {
            throw "TAR numeric validation input changed: $file"
        }
    }
    $passed = $true
    Write-Host $(if ($SkipAot) { '[tar numeric] C/JIT checks: PASS (AOT skipped)' }
        else { '[tar numeric] C/JIT/AOT checks: PASS' })
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($build)
    if (-not $resolved.StartsWith($buildRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolved).StartsWith('tar_numeric_')) {
        throw 'TAR numeric directory escaped build/test'
    }
    if ($passed) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    else { Write-Warning "TAR numeric failure artifacts retained: $resolved" }
}

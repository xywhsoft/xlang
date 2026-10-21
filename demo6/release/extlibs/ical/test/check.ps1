param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('ical_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop |
    Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/ical/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[ical] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

[IO.Directory]::CreateDirectory($work) | Out-Null
Push-Location $root
try {
    $units = @('ical_native', 'ical_parse', 'ical_build')
    $objects = @()
    $null = Run 'strict manifest syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE',
        '-DXRT_MODULE_VALUE_CONTAINER', '-fsyntax-only') +
        @($units | ForEach-Object { "release/extlibs/ical/$_.c" }))
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/ical/test/xrt_runtime.c', '-o', $runtime))
    foreach ($unit in $units) {
        $object = Join-Path $work "$unit.o"
        $null = Run "$unit compile" $gcc ($flags + @('-c',
            "release/extlibs/ical/$unit.c", '-o', $object))
        $objects += $object
    }
    $test = Join-Path $work 'test_ical.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/ical/test/test_ical.c', $runtime) +
        $objects + $links + @('-o', $test))
    $null = Run 'C boundaries' $test @((Join-Path $work 'long.ics'))
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/ical/examples/ical.c', $runtime) +
        $links + @('-o', $example))
    $cFile = Join-Path $work 'c.ics'
    $xlFile = Join-Path $work 'xl.ics'
    $cOutput = Run 'C example' $example @($cFile)
    $directOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/ical/examples/ical.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs' }
    foreach ($case in @('test/cases/extlib_ical.xl',
        'test/cases/extlib_ical_boundaries.xl',
        'release/extlibs/ical/test/control_characters.xl',
        'release/extlibs/ical/test/allocations.xl',
        'release/extlibs/ical/test/text_identifiers.xl',
        'release/extlibs/ical/test/nested_calendar.xl')) {
        $null = Run $case $xl @('run', $case)
    }
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/ical.xl', '--', $xlFile)
    if ($xlOutput -cne $cOutput) { throw 'C/XLang example output differs' }
    $null = Run 'independent wire check' $python @(
        'release/extlibs/ical/test/check_wire.py', $cFile, $xlFile,
        (Join-Path $work 'long.ics'))
    if (-not $SkipAot) {
        $target = Join-Path $work 'aot_no_index.exe'
        $null = Run 'XLang core AOT build' $xl @('build',
            'release/extlibs/ical/test/aot_no_index.xl', '-o', $target)
        $null = Run 'XLang core AOT' $target @()
        $target = Join-Path $work 'ical_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/ical.xl', '-o', $target)
        $aotFile = Join-Path $work 'aot.ics'
        $aotOutput = Run 'XLang example AOT' $target @($aotFile)
        if ($aotOutput -cne $cOutput) { throw 'AOT/C example output differs' }
        $null = Run 'independent AOT wire check' $python @(
            'release/extlibs/ical/test/check_wire.py', $cFile, $aotFile,
            (Join-Path $work 'long.ics'))
    }
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($work)
    $prefix = $safeRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not ([IO.Path]::GetFileName($resolved)).StartsWith('ical_guard_')) {
        throw "Refusing to remove unexpected test directory: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('si_' + [guid]::NewGuid().ToString('N').Substring(0, 12))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/singleinstance')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$previousProfile = $env:USERPROFILE

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[singleinstance] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

[IO.Directory]::CreateDirectory($work) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $work 'home')) | Out-Null
$env:USERPROFILE = Join-Path $work 'home'
Push-Location $root
try {
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/singleinstance/test/runtime.c', '-o', $runtime))
    $native = Join-Path $work 'native.o'
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/singleinstance/singleinstance_native.c', '-o', $native))
    $test = Join-Path $work 'boundaries.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/singleinstance/test/test_singleinstance.c',
        $native, $runtime) + $links + @('-o', $test))
    $null = Run 'C cross-process boundaries' $test @()
    $moduleObjects = @()
    foreach ($instance in @('a', 'b')) {
        $object = Join-Path $work "module_${instance}.o"
        $null = Run "independent module $instance compile" $gcc ($flags + @(
            "-Dxsi_acquire=xsi_${instance}_acquire",
            "-Dxsi_release=xsi_${instance}_release",
            "-Dxsi_is_acquired=xsi_${instance}_is_acquired",
            '-c', 'release/extlibs/singleinstance/singleinstance_native.c', '-o', $object))
        $moduleObjects += $object
    }
    $modules = Join-Path $work 'modules.exe'
    $null = Run 'independent modules test compile' $gcc ($flags + @('-DNDEBUG',
        'release/extlibs/singleinstance/test/test_multiple_modules.c') +
        $moduleObjects + @($runtime) + $links + @('-o', $modules))
    $null = Run 'independent modules same-process boundaries' $modules @()
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/singleinstance/examples/singleinstance_c.c',
        $native, $runtime) + $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $direct = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/singleinstance/examples/singleinstance_c.c')
    $directExe = Join-Path $work 'example-direct.exe'
    $null = Run 'C example direct xl build' $xl @('build',
        'release/extlibs/singleinstance/examples/singleinstance_c.c', '-o', $directExe)
    $directBuilt = Run 'C example direct xl executable' $directExe @()
    $null = Run 'central XLang test' $xl @('run', 'test/cases/extlib_singleinstance.xl')
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/singleinstance/test/boundaries.xl')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/singleinstance.xl')
    if ($cOutput -cne $xlOutput -or $direct -cne $cOutput -or $directBuilt -cne $cOutput) {
        throw "C/XLang outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput`nDirect:`n$direct`nBuilt:`n$directBuilt"
    }
    Write-Host '[singleinstance] C/XLang parity: PASS'
    if (-not $SkipAot) {
        $target = Join-Path $work 'singleinstance.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/singleinstance.xl', '-o', $target)
        $null = Run 'XLang example AOT' $target @()
    }
} finally {
    Pop-Location
    $env:USERPROFILE = $previousProfile
    $resolved = [IO.Path]::GetFullPath($work)
    $prefix = $safeRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not ([IO.Path]::GetFileName($resolved)).StartsWith('si_')) {
        throw "Refusing to remove unexpected test directory: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

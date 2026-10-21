param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('slugify_guard_' + [guid]::NewGuid().ToString('N'))

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
    Write-Host "[slugify] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $flags = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
        '-Ilib/xrt', '-Irelease/extlibs/slugify',
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE')
    $null = Run 'strict C syntax' $gcc ($flags + @(
        'release/extlibs/slugify/slugify_native.c'))
    foreach ($name in @('test_slugify.c', 'test_threads.c', 'slugify_c.c')) {
        $source = if ($name -eq 'slugify_c.c') {
            'release/extlibs/slugify/examples/slugify_c.c'
        } else {
            "release/extlibs/slugify/test/$name"
        }
        if ($name -ne 'slugify_c.c') {
            $null = Run $name 'powershell' @('-NoProfile', '-ExecutionPolicy',
                'Bypass', '-File', $runner, '-Package', 'slugify', '-Source', $source)
        } else {
            $cOutput = Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy',
                'Bypass', '-File', $runner, '-Package', 'slugify', '-Source', $source)
        }
    }
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_slugify.xl')
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/slugify/examples/slugify_c.c')
    if ($directOutput -cne $cOutput) {
        throw 'direct C example output differs from GCC example'
    }
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/slugify/test/boundaries.xl')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/slugify.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang example output differs`nC: $cOutput`nXLang: $xlOutput"
    }
    Write-Host '[slugify] C/XLang output: PASS'
    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'slugify_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'test/cases/extlib_slugify.xl', '-o', $core)
        $null = Run 'XLang core AOT' $core @()
        $example = Join-Path $work 'slugify_example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/slugify.xl', '-o', $example)
        $aotOutput = Run 'XLang example AOT' $example @()
        if ($aotOutput -cne $cOutput) {
            throw 'C/XLang AOT example output differs'
        }
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "slugify temporary work path escaped build/test: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

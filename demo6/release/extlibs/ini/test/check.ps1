param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('ini_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[ini] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict C syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/ini',
        'release/extlibs/ini/test/test_ini.c', 'release/extlibs/ini/ini_native.c')
    Push-Location $release
    try {
        $null = Run 'C boundaries' $xl @('run', 'extlibs/ini/test/test_ini.c',
            '--inc-path', '../lib/xrt')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/ini/test/boundaries.xl')
        $oldErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $privateDiagnostic = ((& $xl check extlibs/ini/test/private_native.xl 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $privateExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldErrorActionPreference
        }
        if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('getNative') -or
            -not $privateDiagnostic.Contains('access')) {
            throw "INI native member access check failed: $privateDiagnostic"
        }
        $oldErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $constructorDiagnostic = ((& $xl check extlibs/ini/test/private_config_constructor.xl 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $constructorExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldErrorActionPreference
        }
        if ($constructorExitCode -eq 0 -or
            -not $constructorDiagnostic.Contains('no such exported type')) {
            throw "INI Config export check failed: $constructorDiagnostic"
        }
        $null = Run 'shared suite' $xl @('run', '../test/cases/extlib_ini.xl')
        $cOutput = Run 'C example' $xl @('run', 'examples/extlibs/ini_c.c')
        $xlOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/ini.xl')
        if ($cOutput -cne $xlOutput) { throw '[ini] C/XLang example output differs' }
        if (-not $SkipAot) {
            $coreExe = Join-Path $work 'float_core.exe'
            $null = Run 'AOT float core build' $xl @('build',
                'extlibs/ini/test/aot_float_core.xl', '-o', $coreExe)
            $null = Run 'AOT float core' $coreExe @()
            $exe = Join-Path $work 'boundaries.exe'
            $null = Run 'AOT boundaries build' $xl @('build',
                'extlibs/ini/test/boundaries.xl', '-o', $exe)
            $null = Run 'AOT boundaries' $exe @()
        }
    } finally { Pop-Location }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('ini_guard_')) {
        throw "INI test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}

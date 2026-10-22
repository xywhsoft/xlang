param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit $LASTEXITCODE" }
    Write-Host "[stb_image] ${label}: PASS"
    return $output
}

function RunNativeAccessNegative {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $xl check 'release/extlibs/stb_image/test/private_native_status_negative.xl' 2>&1 |
            ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    $diagnostic = $lines -join "`n"
    if ($code -eq 0 -or -not $diagnostic.Contains('validNative') -or
        -not $diagnostic.Contains('floatNative') -or -not $diagnostic.Contains('access')) {
        throw "Native status methods remain accessible: $diagnostic"
    }
    Write-Host '[stb_image] Native status access: PASS'
}

$work = $null
Push-Location $root
try {
    Run 'strict C syntax' 'gcc' @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_FILE_WHOLE', '-Ilib/xrt', '-fsyntax-only',
        'release/extlibs/stb_image/stb_image_native.c') | Out-Null
    Run 'C boundaries' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'stb_image',
        '-Source', 'release/extlibs/stb_image/test/test_stb_image.c') | Out-Null
    Run 'C threads' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'stb_image',
        '-Source', 'release/extlibs/stb_image/test/test_stb_image_threads.c') | Out-Null
    $c = Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'stb_image',
        '-Source', 'release/examples/extlibs/stb_image_c.c')
    Run 'XLang boundaries' $xl @('run', 'release/extlibs/stb_image/test/boundaries.xl') | Out-Null
    Run 'XLang allocations' $xl @('run', 'release/extlibs/stb_image/test/allocations.xl') | Out-Null
    RunNativeAccessNegative
    Push-Location (Join-Path $root 'release')
    try {
        $xlang = Run 'XLang example' $xl @('run', 'examples/extlibs/stb_image.xl')
    } finally {
        Pop-Location
    }
    $expected = @('image:  16 x 16', 'channels:  4',
        'pixel bytes:  1024', 'stb_image C/XL example: PASS') -join "`n"
    if (($c -join "`n") -cne $expected) {
        throw 'C image example differs from fixed output'
    }
    if (($c -join "`n") -cne ($xlang -join "`n")) {
        throw 'C/XLang example output differs'
    }
    Write-Host '[stb_image] C/XLang output parity: PASS'

    if (-not $SkipAot) {
        $work = Join-Path ([IO.Path]::GetTempPath()) ('xlang-stb-image-aot-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $work | Out-Null
        $core = Join-Path $work 'stb_image_core.exe'
        $example = Join-Path $work 'stb_image_example.exe'
        Run 'core AOT build' $xl @('build', 'test/cases/extlib_stb_image.xl', '-o', $core) | Out-Null
        Run 'core AOT' $core @() | Out-Null
        Run 'example AOT build' $xl @('build', 'release/examples/extlibs/stb_image.xl', '-o', $example) | Out-Null
        $asset = Join-Path $root 'release/examples/extlibs/assets/tiny.png'
        $aot = Run 'example AOT' $example @($asset)
        if (($c -join "`n") -cne ($aot -join "`n")) {
            throw 'C/XLang AOT example output differs'
        }
        Write-Host '[stb_image] C/XLang AOT output parity: PASS'
    }
} finally {
    Pop-Location
    if ($work -ne $null) {
        foreach ($name in @('stb_image_core.exe', 'stb_image_example.exe')) {
            $output = Join-Path $work $name
            if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Force }
        }
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Force }
    }
}

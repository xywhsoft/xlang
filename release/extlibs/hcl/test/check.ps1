param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('hcl_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/hcl/hcl_native.c',
        'release/extlibs/hcl/test/test_hcl.c',
        'release/extlibs/hcl/test/test_hcl_threads.c',
        'release/examples/extlibs/hcl_c.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/hcl/include $source
        if ($LASTEXITCODE -ne 0) { throw "hcl strict GCC check failed: $source" }
    }

    foreach ($source in @(
        'release/extlibs/hcl/test/test_hcl.c',
        'release/extlibs/hcl/test/test_hcl_threads.c'
    )) {
        & $runner -Package hcl -Source $source
        if ($LASTEXITCODE -ne 0) { throw "hcl C test failed: $source" }
    }
    foreach ($source in @(
        'test/cases/extlib_hcl.xl',
        'release/extlibs/hcl/test/boundaries.xl',
        'release/extlibs/hcl/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "hcl XLang test failed: $source" }
    }

    $generated = Join-Path $work 'owned_text.c'
    & $xl cgen release/extlibs/hcl/test/boundaries.xl --emit $generated
    if ($LASTEXITCODE -ne 0) { throw 'hcl direct-text cgen failed' }
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($symbol in @('xhcl_get', 'xhcl_get_raw', 'xhcl_block_label',
            'xhcl_block_label_at')) {
        if ($generatedText -notmatch ('\.Data\s*=\s*' + $symbol + '\s*\(')) {
            throw "hcl $symbol must adopt the owned C result directly"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1')) {
        throw 'hcl text results must not use legacy transfer or copying'
    }
    Write-Output 'hcl direct string adoption: PASS'

    $cOutput = & $runner -Package hcl -Source release/examples/extlibs/hcl_c.c
    if ($LASTEXITCODE -ne 0) { throw 'hcl C example failed' }
    $directCOutput = & $xl run release/examples/extlibs/hcl_c.c
    if ($LASTEXITCODE -ne 0 -or ($cOutput -join "`n") -cne ($directCOutput -join "`n")) {
        throw 'hcl direct C example failed or output differs'
    }
    $xlOutput = & $xl run release/examples/extlibs/hcl.xl
    if ($LASTEXITCODE -ne 0) { throw 'hcl XLang example failed' }
    if (($cOutput -join "`n") -cne ($xlOutput -join "`n")) {
        throw 'hcl C/XLang example output differs'
    }

    if (-not $SkipAot) {
        $core = Join-Path $work 'core.exe'
        & $xl build release/extlibs/hcl/test/aot_core.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'hcl core AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'hcl core AOT run failed' }
        Write-Output 'hcl core AOT: PASS'

        $boundaries = Join-Path $work 'boundaries.exe'
        & $xl build release/extlibs/hcl/test/boundaries.xl -o $boundaries
        if ($LASTEXITCODE -ne 0) { throw 'hcl boundaries AOT build failed' }
        $boundariesOutput = & $boundaries
        if ($LASTEXITCODE -ne 0 -or ($boundariesOutput -join "`n") -cne 'hcl boundaries: ok') {
            throw 'hcl boundaries AOT failed or output differs'
        }
        Write-Output 'hcl boundaries AOT: PASS'

        $allocations = Join-Path $work 'allocations.exe'
        & $xl build release/extlibs/hcl/test/allocations.xl -o $allocations
        if ($LASTEXITCODE -ne 0) { throw 'hcl allocations AOT build failed' }
        $allocationsOutput = & $allocations
        if ($LASTEXITCODE -ne 0 -or ($allocationsOutput -join "`n") -cne 'hcl allocation: PASS') {
            throw 'hcl allocations AOT failed or output differs'
        }
        Write-Output 'hcl allocations AOT: PASS'

        $example = Join-Path $work 'example.exe'
        & $xl build release/examples/extlibs/hcl.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'hcl example AOT build failed' }
        $aotOutput = & $example
        if ($LASTEXITCODE -ne 0 -or ($aotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'hcl AOT example failed or output differs from C'
        }
        Write-Output 'hcl AOT/C example parity: PASS'
        $cExample = Join-Path $work 'c_example.exe'
        & $xl build release/examples/extlibs/hcl_c.c -o $cExample
        if ($LASTEXITCODE -ne 0) { throw 'hcl C example AOT build failed' }
        $cAotOutput = & $cExample
        if ($LASTEXITCODE -ne 0 -or ($cAotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'hcl C example AOT failed or output differs from GCC'
        }
        Write-Output 'hcl C example AOT parity: PASS'
    }
    Write-Output 'hcl Windows C/XLang gate: PASS'
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('hcl_guard_')) {
        throw "HCL test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$work = $null

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/stb_image_write/stb_image_write_native.c',
        'release/extlibs/stb_image_write/test/test_stb_image_write.c',
        'release/examples/extlibs/stb_image_write_c.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/stb_image_write/include `
            -Irelease/extlibs/stb_image/include $source
        if ($LASTEXITCODE -ne 0) { throw "stb_image_write GCC check failed: $source" }
    }

    & $runner -Package 'stb_image_write,stb_image' `
        -Source release/extlibs/stb_image_write/test/test_stb_image_write.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_stb_image_write.xl',
        'release/extlibs/stb_image_write/test/boundaries.xl',
        'release/extlibs/stb_image_write/test/allocations.xl',
        'release/extlibs/stb_image_write/test/output_limit.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "stb_image_write XLang test failed: $source" }
    }

    $work = Join-Path ([IO.Path]::GetTempPath()) ('xlang-stbiw-parity-' + [guid]::NewGuid().ToString('N'))
    $cDir = Join-Path $work 'c'
    $xlDir = Join-Path $work 'xl'
    New-Item -ItemType Directory -Path $cDir, $xlDir | Out-Null
    $direct = & $xl run release/examples/extlibs/stb_image_write_c.c -- $cDir
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write direct C example failed' }
    $runnerOutput = & $runner -Package 'stb_image_write,stb_image' `
        -Source release/examples/extlibs/stb_image_write_c.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write C package example failed' }
    $xlangOutput = & $xl run release/examples/extlibs/stb_image_write.xl -- $xlDir
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write XLang example failed' }
    $expected = 'stb_image_write C/XL example: PASS'
    if (($xlangOutput -join "`n") -cne $expected -or
        ($direct -join "`n") -cne $expected -or
        ($runnerOutput -join "`n") -cne $expected) {
        throw 'stb_image_write C/XLang example output differs'
    }
    foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
        $cFile = Join-Path $cDir $name
        $xlFile = Join-Path $xlDir $name
        if (-not (Test-Path -LiteralPath $cFile) -or -not (Test-Path -LiteralPath $xlFile)) {
            throw "stb_image_write missing example output: $name"
        }
        if ((Get-FileHash -LiteralPath $cFile -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $xlFile -Algorithm SHA256).Hash) {
            throw "stb_image_write C/XLang image differs: $name"
        }
    }

    if (-not $SkipAot) {
        $core = Join-Path $work 'stbiw_core.exe'
        $example = Join-Path $work 'stbiw_example.exe'
        $cExample = Join-Path $work 'stbiw_c_example.exe'
        & $xl build test/cases/extlib_stb_image_write.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'stb_image_write core AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'stb_image_write core AOT failed' }
        & $xl build release/examples/extlibs/stb_image_write.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'stb_image_write example AOT build failed' }
        $aotDir = Join-Path $work 'aot'
        New-Item -ItemType Directory -Path $aotDir | Out-Null
        $asset = Join-Path $root 'release/examples/extlibs/assets/tiny.png'
        $aotOutput = & $example $aotDir $asset
        if ($LASTEXITCODE -ne 0) { throw 'stb_image_write example AOT failed' }
        if (($aotOutput -join "`n") -cne $expected) {
            throw 'stb_image_write example AOT output differs'
        }
        foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
            $aotFile = Join-Path $aotDir $name
            $xlFile = Join-Path $xlDir $name
            if (-not (Test-Path -LiteralPath $aotFile) -or
                (Get-FileHash -LiteralPath $aotFile -Algorithm SHA256).Hash -cne
                (Get-FileHash -LiteralPath $xlFile -Algorithm SHA256).Hash) {
                throw "stb_image_write AOT image differs: $name"
            }
        }
        & $xl build release/examples/extlibs/stb_image_write_c.c -o $cExample
        if ($LASTEXITCODE -ne 0) { throw 'stb_image_write C example AOT build failed' }
        $cAotDir = Join-Path $work 'c_aot'
        New-Item -ItemType Directory -Path $cAotDir | Out-Null
        $cAotOutput = & $cExample $cAotDir $asset
        if ($LASTEXITCODE -ne 0 -or ($cAotOutput -join "`n") -cne $expected) {
            throw 'stb_image_write C example AOT failed'
        }
        foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
            if ((Get-FileHash -LiteralPath (Join-Path $cAotDir $name) -Algorithm SHA256).Hash -cne
                (Get-FileHash -LiteralPath (Join-Path $xlDir $name) -Algorithm SHA256).Hash) {
                throw "stb_image_write C AOT image differs: $name"
            }
        }
    }

    Write-Output 'stb_image_write Windows C/XLang gate: PASS'
} finally {
    Pop-Location
    if ($work -ne $null) {
        foreach ($dirName in @('c', 'xl', 'aot', 'c_aot')) {
            $dir = Join-Path $work $dirName
            foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
                $output = Join-Path $dir $name
                if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Force }
            }
            if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Force }
        }
        foreach ($name in @('stbiw_core.exe', 'stbiw_example.exe', 'stbiw_c_example.exe')) {
            $output = Join-Path $work $name
            if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Force }
        }
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Force }
    }
}

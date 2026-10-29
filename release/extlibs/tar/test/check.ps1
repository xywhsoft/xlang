param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test/tar_windows'))
$build = [IO.Path]::GetFullPath((Join-Path $buildRoot ("{0}_{1}" -f $PID, [guid]::NewGuid().ToString('N'))))
if (-not $build.StartsWith($buildRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'TAR test directory escaped build/test'
}
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/tar/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$sources = @(
    'release/extlibs/tar/xrt_tar_common.c',
    'release/extlibs/tar/xrt_tar_reader.c',
    'release/extlibs/tar/xrt_tar_writer.c',
    'release/extlibs/tar/xrt_tar_extract.c',
    'release/extlibs/tar/test/runtime.c',
    'release/extlibs/tar/test/test_tar_c.c'
)

[void][IO.Directory]::CreateDirectory($build)
$rootPushed = $false
try {
    Push-Location $root
    $rootPushed = $true
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Ilib/xruntime -Irelease/extlibs/tar release/extlibs/tar/tar_native.c
    if ($LASTEXITCODE -ne 0) { throw 'strict GCC TAR bridge syntax failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Ilib/xruntime -Irelease/extlibs/tar @moduleFlags `
        @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/tar/$_" })
    if ($LASTEXITCODE -ne 0) { throw 'strict GCC TAR manifest macro syntax failed' }
    $exe = Join-Path $build 'test_tar.exe'
    & $gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/tar `
        @sources -o $exe -lws2_32 -lbcrypt -lshlwapi
    if ($LASTEXITCODE -ne 0) { throw 'strict GCC TAR build failed' }
    $example = Join-Path $build 'basic_c.exe'
    & $gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/tar `
        release/extlibs/tar/examples/basic_c.c release/extlibs/tar/test/runtime.c `
        -o $example -lws2_32 -lbcrypt -lshlwapi
    if ($LASTEXITCODE -ne 0) { throw 'strict GCC standalone TAR example build failed' }
    $appendExample = Join-Path $build 'append_c.exe'
    & $gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/tar `
        release/extlibs/tar/examples/append_c.c release/extlibs/tar/test/runtime.c `
        -o $appendExample -lws2_32 -lbcrypt -lshlwapi
    if ($LASTEXITCODE -ne 0) { throw 'strict GCC TAR append example build failed' }
    Push-Location $build
    try {
        & $exe
        if ($LASTEXITCODE -ne 0) { throw 'strict GCC TAR test failed' }
        $cOutput = (& $example | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $cOutput -cne 'hello') {
            throw 'strict GCC standalone TAR example failed'
        }
        $appendCOutput = @(& $appendExample)
        if ($LASTEXITCODE -ne 0 -or ($appendCOutput -join "`n") -cne "first`nsecond") {
            throw 'strict GCC TAR append example failed'
        }
        & (Join-Path $root 'tools/run_extlib_c.ps1') -Package tar `
            -Source release/extlibs/tar/examples/basic_c.c
        if ($LASTEXITCODE -ne 0) { throw 'TCC C TAR example failed' }
        & (Join-Path $root 'tools/run_extlib_c.ps1') -Package tar `
            -Source release/extlibs/tar/test/test_read_file_limit.c
        if ($LASTEXITCODE -ne 0) { throw 'TAR native file limit failed' }
    } finally {
        Pop-Location
    }
    Push-Location (Join-Path $root 'release')
    try {
        foreach ($name in @('tar_c', 'tar_compressed_c')) {
            $source = "examples/extlibs/$name.c"
            $expected = if ($name -eq 'tar_c') { 'tar C example: PASS' } else { 'compressed TAR C example: PASS' }
            $output = (& $xl run $source | Out-String).Trim()
            if ($LASTEXITCODE -ne 0 -or $output -cne $expected) {
                throw "direct TCC $name example failed"
            }
            $binary = Join-Path $build "$name.exe"
            & $xl build $source -o $binary
            if ($LASTEXITCODE -ne 0) { throw "direct TCC $name example build failed" }
            $output = (& $binary | Out-String).Trim()
            if ($LASTEXITCODE -ne 0 -or $output -cne $expected) {
                throw "direct TCC $name example executable failed"
            }
        }
        $directOutput = (& $xl run extlibs/tar/examples/basic_c.c | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $directOutput -cne $cOutput) {
            throw 'standalone TCC TAR example differs from GCC'
        }
        $xlOutput = (& $xl run extlibs/tar/examples/basic.xl | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $xlOutput -cne $cOutput) {
            throw 'XLang TAR example differs from C'
        }
        $appendTccOutput = @(& $xl run extlibs/tar/examples/append_c.c)
        if ($LASTEXITCODE -ne 0 -or
            ($appendTccOutput -join "`n") -cne ($appendCOutput -join "`n")) {
            throw 'TCC TAR append example differs from GCC'
        }
        $appendXlOutput = @(& $xl run extlibs/tar/examples/append.xl)
        if ($LASTEXITCODE -ne 0 -or
            ($appendXlOutput -join "`n") -cne ($appendCOutput -join "`n")) {
            throw 'XLang TAR append example differs from C'
        }
        $centralOutput = (& $xl run examples/extlibs/tar.xl | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $centralOutput -cne 'tar example: PASS') {
            throw 'central TAR extraction example failed'
        }
        & $xl run extlibs/tar/test/boundaries.xl
        if ($LASTEXITCODE -ne 0) { throw 'XLang TAR boundaries failed' }
        & $xl run extlibs/tar/test/named_type.xl
        if ($LASTEXITCODE -ne 0) { throw 'XLang TAR exported entry type failed' }
        $python = Get-Command python -CommandType Application -All `
            -ErrorAction SilentlyContinue |
            Where-Object { $_.Version -ne [version]'0.0.0.0' } |
            Select-Object -First 1
        if ($null -ne $python) {
            & $python.Source extlibs/tar/test/interop.py .
            if ($LASTEXITCODE -ne 0) { throw 'Python/XLang XZ interop failed' }
        } else {
            Write-Warning 'Python 3 unavailable; skipping independent XZ interop check'
        }
        $oldErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $privateDiagnostic = ((& $xl check extlibs/tar/test/private_native.xl 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $privateExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldErrorActionPreference
        }
        if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('addStrNative') -or
            -not $privateDiagnostic.Contains('access')) {
            throw "TAR native member access check failed: $privateDiagnostic"
        }
        $oldErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $constructorDiagnostic = ((& $xl check extlibs/tar/test/private_reader_constructor.xl 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $constructorExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldErrorActionPreference
        }
        if ($constructorExitCode -eq 0 -or
            -not $constructorDiagnostic.Contains('no such exported type')) {
            throw "TAR Reader export check failed: $constructorDiagnostic"
        }
    } finally {
        Pop-Location
    }
    if (-not $SkipAot) {
        $aot = Join-Path $build 'aot_core.exe'
        & $xl build release/extlibs/tar/test/aot_core.xl -o $aot
        if ($LASTEXITCODE -ne 0) { throw 'XLang TAR AOT core build failed' }
        & $aot
        if ($LASTEXITCODE -ne 0) { throw 'XLang TAR AOT core failed' }
        $exampleAot = Join-Path $build 'basic_xl.exe'
        & $xl build release/extlibs/tar/examples/basic.xl -o $exampleAot
        if ($LASTEXITCODE -ne 0) { throw 'XLang TAR example AOT build failed' }
        $aotOutput = (& $exampleAot | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $aotOutput -cne $cOutput) {
            throw 'XLang TAR example AOT differs from C'
        }
        $appendAot = Join-Path $build 'append_xl.exe'
        & $xl build release/extlibs/tar/examples/append.xl -o $appendAot
        if ($LASTEXITCODE -ne 0) { throw 'XLang TAR append AOT build failed' }
        $appendAotOutput = @(& $appendAot)
        if ($LASTEXITCODE -ne 0 -or
            ($appendAotOutput -join "`n") -cne ($appendCOutput -join "`n")) {
            throw 'XLang TAR append AOT differs from C'
        }
    }
} finally {
    if ($rootPushed) { Pop-Location }
    if ([IO.Directory]::Exists($build)) {
        [IO.Directory]::Delete($build, $true)
    }
}
Write-Host '[tar] focused C/XLang gate: PASS'

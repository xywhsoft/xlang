$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$work = Join-Path $root 'build/test/zstd'
$binary = Join-Path $work 'interop.exe'
[void][IO.Directory]::CreateDirectory($work)

Push-Location $root
try {
    $vendor = Join-Path $work 'vendor.o'
    & gcc -std=c11 -O2 -c release/extlibs/zstd/third_party/zstd.c -o $vendor
    if ($LASTEXITCODE -ne 0) { throw 'zstd vendor build failed' }
    & gcc -std=c11 -O2 -Wall -Wextra -Werror `
        -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/zstd/include `
        test/support/extlib_codec_runtime.c `
        release/extlibs/zstd/xrt_zstd.c `
        release/extlibs/zstd/test/interop.c `
        $vendor -lws2_32 -ladvapi32 -liphlpapi -lm -o $binary
    if ($LASTEXITCODE -ne 0) { throw 'zstd C interop build failed' }

    $previousPythonPath = $env:PYTHONPATH
    try {
        $oracle = Join-Path $work 'python_oracle'
        if (Test-Path -LiteralPath (Join-Path $oracle 'zstandard')) {
            $env:PYTHONPATH = $oracle + [IO.Path]::PathSeparator + $previousPythonPath
        }
        & python release/extlibs/zstd/test/interop.py $binary
        if ($LASTEXITCODE -ne 0) { throw 'zstd Python/C interoperability failed' }
    } finally {
        $env:PYTHONPATH = $previousPythonPath
    }
} finally {
    Pop-Location
}

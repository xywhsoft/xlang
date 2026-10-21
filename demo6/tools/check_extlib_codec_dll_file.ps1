param([string[]]$Package = @('gzip', 'bz2'))

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $root 'build/test/codec_dll_file'
[void][IO.Directory]::CreateDirectory($work)
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source

foreach ($name in $Package) {
    if ($name -notin @('gzip', 'bz2', 'brotli')) { throw "Unknown codec package: $name" }
    $manifest = Get-Content (Join-Path $root "release/extlibs/$name/xlang.package.json") -Raw -Encoding UTF8 | ConvertFrom-Json
    $sources = @($manifest.artifact.cSources | ForEach-Object {
        Join-Path $root "release/extlibs/$name/$_"
    })
    if ($name -eq 'gzip') {
        $entry = Join-Path $root 'release/extlibs/gzip/test/file_allocation_dll.c'
    } elseif ($name -eq 'brotli') {
        $entry = Join-Path $root 'release/extlibs/brotli/test/file_allocation.c'
    } else {
        $entry = Join-Path $root 'release/extlibs/bz2/test/test_bz2_files.c'
        $vendor = Get-Content (Join-Path $root 'release/extlibs/_bzip2/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        $sources += @($vendor.artifact.cSources | ForEach-Object {
            Join-Path $root "release/extlibs/_bzip2/$_"
        })
    }
    foreach ($dll in @('xl', 'xlc')) {
        $library = Join-Path $root "release/$dll.dll"
        Copy-Item -LiteralPath $library -Destination (Join-Path $work "$dll.dll") -Force
        $hostExe = Join-Path $work "$dll-$name-host.exe"
        & $gcc -std=c11 -O2 -Wall -Wextra -Werror `
            '-I' (Join-Path $root 'release/include') `
            (Join-Path $root 'test/support/extlib_codec_dll_host.c') `
            $library -o $hostExe
        if ($LASTEXITCODE -ne 0) { throw "$name $dll host build failed" }
        $testDir = Join-Path $work "$dll-$name-files"
        & $hostExe $root (Join-Path $root 'lib/xrt') `
            (Join-Path $root 'lib/xruntime') (Join-Path $root 'test/cases') `
            $testDir $entry @sources
        if ($LASTEXITCODE -ne 0) { throw "$name $dll file allocation failed" }
        Write-Host "[$name] $dll direct DLL file allocation: PASS"
    }
}

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('clipboard_lifecycle_' + [guid]::NewGuid().ToString('N'))
$hostExe = Join-Path $work 'context_lifecycle.exe'

Push-Location $root
try {
    [void][IO.Directory]::CreateDirectory($work)
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Irelease/include `
        release/extlibs/clipboard/test/context_lifecycle.c -Lrelease -lxl -o $hostExe
    if ($LASTEXITCODE -ne 0) { throw 'clipboard lifecycle host build failed' }
    $priorPath = $env:PATH
    $priorPackagePath = $env:XLANG_PACKAGE_PATH
    try {
        $env:PATH = "$release;$priorPath"
        $env:XLANG_PACKAGE_PATH = Join-Path $release 'extlibs'
        & $hostExe $release `
            (Join-Path $release 'extlibs/clipboard/test/context_plain.xl') `
            (Join-Path $release 'extlibs/clipboard/test/context_clipboard.xl')
        $result = $LASTEXITCODE
    } finally {
        $env:PATH = $priorPath
        $env:XLANG_PACKAGE_PATH = $priorPackagePath
    }
    if ($result -ne 0) { throw "clipboard lifecycle host failed: exit $result" }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}

param([switch]$StrictC)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'uia requires Windows' }

$release = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$xl = Join-Path $release 'xl.exe'
$workRoot = [IO.Path]::GetFullPath((Join-Path $release '../build/test'))
$work = Join-Path $workRoot ('uia_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

function Invoke-Checked([string]$Label, [string[]]$Arguments) {
    Write-Host $Label
    $output = & $xl @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        throw "$Label failed with exit code $LASTEXITCODE"
    }
    return ($output | Out-String).Trim()
}

Push-Location $release
try {
    $cArgs = @('run', 'extlibs/uia/test/test_uia.c', '--link', 'user32', '--link', 'ole32', '--link', 'oleaut32')
    $cSmoke = Invoke-Checked 'C smoke' $cArgs
    if ($cSmoke -ne 'uia C smoke: PASS') { throw 'unexpected C smoke output' }

    foreach ($name in @('version', 'window_raw', 'window', 'window_auto', 'basic', 'boundaries')) {
        $file = "extlibs/uia/test/test_uia_$name.xl"
        [void](Invoke-Checked "XLang $name" @('run', $file))
    }
    $full = Invoke-Checked 'XLang full example' @('run', 'examples/extlibs/uia.xl')
    if ($full -ne 'uia extlib ok') { throw 'unexpected full example output' }

    $xlExample = Invoke-Checked 'XLang paired example' @('run', 'examples/extlibs/uia_quick.xl')
    $cExample = Invoke-Checked 'C paired example' @(
        'run', 'examples/extlibs/uia_quick_c.c', 'extlibs/uia/x_uia_artifact.c',
        '--link', 'user32', '--link', 'ole32', '--link', 'oleaut32'
    )
    if ($xlExample -cne $cExample) { throw "C/XLang output differs: $xlExample / $cExample" }

    $aot = Join-Path $work 'aot_core.exe'
    [void](Invoke-Checked 'XLang core AOT build' @('build', 'extlibs/uia/test/aot_core.xl', '-o', $aot))
    & $aot
    if ($LASTEXITCODE -ne 0) { throw 'XLang core AOT run failed' }

    if ($StrictC) {
        $gcc = (Get-Command gcc -ErrorAction Stop).Source
        $outputExe = Join-Path $work 'uia_c_test.exe'
        $xrtDir = (Resolve-Path (Join-Path $release '../lib/xrt')).Path
        Write-Host 'GCC standalone C test'
        & $gcc -std=c11 -O2 -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -DXRT_IMPLEMENTATION "-I$xrtDir" -o $outputExe (Join-Path $release 'extlibs/uia/test/test_uia.c') -lole32 -loleaut32 -luser32 -lws2_32 -lcrypt32 -lbcrypt -liphlpapi -lshlwapi -lshell32 -ladvapi32 -lgdi32 -lwinhttp -lwsock32
        if ($LASTEXITCODE -ne 0) { throw 'GCC standalone C build failed' }
        & $outputExe
        if ($LASTEXITCODE -ne 0) { throw 'GCC standalone C run failed' }
    }

    Write-Host 'uia package checks: PASS'
} finally {
    Pop-Location
    $safeRoot = $workRoot.TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe UIA test directory' }
    Remove-Item -LiteralPath $target -Recurse -Force
}

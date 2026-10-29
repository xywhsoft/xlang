param([switch]$StrictC, [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'uia requires Windows' }

$release = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$xl = Join-Path $release 'xl.exe'
$workRoot = [IO.Path]::GetFullPath((Join-Path $release '../build/test'))
$work = Join-Path $workRoot ('uia_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

function Invoke-Checked([string]$Label, [string[]]$Arguments) {
    Write-Host $Label
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $xl @Arguments 2>&1
    } finally {
        $ErrorActionPreference = 'Stop'
    }
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
    $closeC = Invoke-Checked 'C close/wait' @('run',
        'extlibs/uia/test/window_close_c.c', 'extlibs/uia/x_uia_artifact.c',
        '--inc-path', '../lib/xrt', '--link', 'user32', '--link', 'ole32', '--link', 'oleaut32')
    $closeXL = Invoke-Checked 'XLang close/wait' @('run', 'extlibs/uia/test/window_close.xl')
    if ($closeC -cne 'uia close/wait ok' -or $closeXL -cne $closeC) {
        throw 'C/XLang close/wait output differs'
    }

    foreach ($name in @('version', 'window_raw', 'window', 'window_auto', 'basic', 'boundaries')) {
        $file = "extlibs/uia/test/test_uia_$name.xl"
        [void](Invoke-Checked "XLang $name" @('run', $file))
    }
    $implicit = Invoke-Checked 'XLang implicit handles' @('run',
        'extlibs/uia/test/test_uia_implicit_handles.xl')
    $focus = Invoke-Checked 'XLang focus callback' @('run',
        'extlibs/uia/test/test_uia_focus_callback.xl')
    if ($focus -cne 'uia focus callback: PASS') {
        throw 'XLang focus callback output differs'
    }
    $full = Invoke-Checked 'XLang full example' @('run', 'examples/extlibs/uia.xl')
    if ($full -ne 'uia extlib ok') { throw 'unexpected full example output' }

    $xlExample = Invoke-Checked 'XLang paired example' @('run', 'examples/extlibs/uia_quick.xl')
    $cExample = Invoke-Checked 'C paired example' @(
        'run', 'examples/extlibs/uia_quick_c.c', 'extlibs/uia/x_uia_artifact.c',
        '--link', 'user32', '--link', 'ole32', '--link', 'oleaut32'
    )
    if ($xlExample -cne $cExample) { throw "C/XLang output differs: $xlExample / $cExample" }

    if ($StrictC) {
        $gcc = (Get-Command gcc -ErrorAction Stop).Source
        $outputExe = Join-Path $work 'uia_c_test.exe'
        $xrtDir = (Resolve-Path (Join-Path $release '../lib/xrt')).Path
        & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only "-I$xrtDir" `
            (Join-Path $release 'extlibs/uia/test/window_close_c.c')
        if ($LASTEXITCODE -ne 0) { throw 'GCC close/wait syntax check failed' }
        Write-Host 'GCC standalone C test'
        & $gcc -std=c11 -O2 -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -DXRT_IMPLEMENTATION "-I$xrtDir" -o $outputExe (Join-Path $release 'extlibs/uia/test/test_uia.c') -lole32 -loleaut32 -luser32 -lws2_32 -lcrypt32 -lbcrypt -liphlpapi -lshlwapi -lshell32 -ladvapi32 -lgdi32 -lwinhttp -lwsock32
        if ($LASTEXITCODE -ne 0) { throw 'GCC standalone C build failed' }
        & $outputExe
        if ($LASTEXITCODE -ne 0) { throw 'GCC standalone C run failed' }
    }

    if (-not $SkipAot) {
        $closeAot = Join-Path $work 'window_close.exe'
        [void](Invoke-Checked 'XLang close/wait AOT build' @('build',
            'extlibs/uia/test/window_close.xl', '-o', $closeAot))
        $closeOutput = & $closeAot 2>&1
        if ($LASTEXITCODE -ne 0 -or ($closeOutput | Out-String).Trim() -cne $closeC) {
            throw 'XLang close/wait AOT output differs from C/JIT'
        }
        $implicitAot = Join-Path $work 'implicit_handles.exe'
        [void](Invoke-Checked 'XLang implicit handles AOT build' @('build',
            'extlibs/uia/test/test_uia_implicit_handles.xl', '-o', $implicitAot))
        $implicitOutput = & $implicitAot 2>&1
        if ($LASTEXITCODE -ne 0 -or ($implicitOutput | Out-String).Trim() -cne $implicit) {
            throw 'XLang implicit handles AOT output differs from JIT'
        }
        $focusAot = Join-Path $work 'focus_callback.exe'
        [void](Invoke-Checked 'XLang focus callback AOT build' @('build',
            'extlibs/uia/test/test_uia_focus_callback.xl', '-o', $focusAot))
        $focusOutput = & $focusAot 2>&1
        if ($LASTEXITCODE -ne 0 -or ($focusOutput | Out-String).Trim() -cne $focus) {
            throw 'XLang focus callback AOT output differs from JIT'
        }
        $aot = Join-Path $work 'aot_core.exe'
        [void](Invoke-Checked 'XLang core AOT build' @('build', 'extlibs/uia/test/aot_core.xl', '-o', $aot))
        & $aot
        if ($LASTEXITCODE -ne 0) { throw 'XLang core AOT run failed' }

        $fullAot = Join-Path $work 'uia_full.exe'
        [void](Invoke-Checked 'XLang full example AOT build' @('build',
            'examples/extlibs/uia.xl', '-o', $fullAot))
        $fullOutput = & $fullAot 2>&1
        if ($LASTEXITCODE -ne 0 -or ($fullOutput | Out-String).Trim() -cne $full) {
            throw 'XLang full example AOT output differs from JIT'
        }

        $pairedAot = Join-Path $work 'uia_quick.exe'
        [void](Invoke-Checked 'XLang paired example AOT build' @('build',
            'examples/extlibs/uia_quick.xl', '-o', $pairedAot))
        $pairedOutput = & $pairedAot 2>&1
        if ($LASTEXITCODE -ne 0 -or ($pairedOutput | Out-String).Trim() -cne $cExample) {
            throw 'XLang paired example AOT output differs from C'
        }
    }

    Write-Host $(if ($SkipAot) { 'uia C/JIT checks: PASS (AOT skipped)' }
        else { 'uia package checks: PASS' })
} finally {
    Pop-Location
    $safeRoot = $workRoot.TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe UIA test directory' }
    Remove-Item -LiteralPath $target -Recurse -Force
}

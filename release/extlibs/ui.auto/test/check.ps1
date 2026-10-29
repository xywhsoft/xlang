param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'ui.auto requires Windows' }

$release = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$workRoot = Join-Path $release 'build/test'
$work = Join-Path $workRoot ('ui_auto_' + [guid]::NewGuid().ToString('N'))
$imagePath = Join-Path $work 'screenshot.png'

function Confirm-Screenshot([string]$path) {
    if (!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -le 0) {
        throw 'screenshot was not written'
    }
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($path)
    try {
        if ($image.Width -le 0 -or $image.Height -le 0) { throw 'screenshot has no pixels' }
        $bitmap = [System.Drawing.Bitmap]$image
        $ink = 0
        for ($y = 2; $y -lt $image.Height - 2; $y++) {
            for ($x = 2; $x -lt $image.Width - 2; $x++) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.R -lt 100 -and $pixel.G -lt 100 -and $pixel.B -lt 100) {
                    $ink++
                }
            }
        }
        if ($ink -lt 10) { throw 'screenshot contains no text pixels' }
        Write-Host "Screenshot: $($image.Width)x$($image.Height)"
    } finally {
        $image.Dispose()
    }
}

Push-Location $release
try {
    [void][IO.Directory]::CreateDirectory($work)
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
        '-I../lib/xrt' examples/extlibs/ui_auto_c.c
    if ($LASTEXITCODE -ne 0) { throw 'ui.auto C example GCC syntax check failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
        '-I../lib/xrt' extlibs/ui.auto/examples/screenshot_c.c
    if ($LASTEXITCODE -ne 0) { throw 'ui.auto C screenshot GCC syntax check failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
        '-I../lib/xrt' extlibs/ui.auto/test/external_window.c
    if ($LASTEXITCODE -ne 0) { throw 'ui.auto external window GCC syntax check failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
        '-I../lib/xrt' extlibs/ui.auto/test/cross_process_c.c
    if ($LASTEXITCODE -ne 0) { throw 'ui.auto C cross-process GCC syntax check failed' }
    $xlExample = @()
    foreach ($file in @(
        'examples/extlibs/ui_auto.xl',
        'extlibs/ui.auto/test/boundaries.xl',
        'extlibs/ui.auto/test/named_type.xl',
        'extlibs/ui.auto/test/screenshot.xl'
    )) {
        Write-Host "Run $file"
        if ($file -eq 'extlibs/ui.auto/test/screenshot.xl') {
            & $xl run $file -- $imagePath
        } elseif ($file -eq 'examples/extlibs/ui_auto.xl') {
            $xlExample = @(& $xl run $file)
        } else {
            & $xl run $file
        }
        if ($LASTEXITCODE -ne 0) { throw "$file failed with exit code $LASTEXITCODE" }
    }
    $fixtureExe = Join-Path $work 'external_window.exe'
    & $xl build extlibs/ui.auto/test/external_window.c `
        extlibs/uia/x_uia_artifact.c --inc-path ../lib/xrt `
        --link ole32 --link oleaut32 --link user32 -o $fixtureExe
    if ($LASTEXITCODE -ne 0) { throw 'external window fixture build failed' }
    $crossProcessCExe = Join-Path $work 'cross_process_c.exe'
    & $xl build extlibs/ui.auto/test/cross_process_c.c `
        extlibs/uia/x_uia_artifact.c --inc-path ../lib/xrt `
        --link ole32 --link oleaut32 --link user32 -o $crossProcessCExe
    if ($LASTEXITCODE -ne 0) { throw 'ui.auto C cross-process build failed' }
    $crossProcessExe = Join-Path $work 'cross_process.exe'
    if (-not $SkipAot) {
        & $xl build extlibs/ui.auto/test/cross_process.xl -o $crossProcessExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto cross-process AOT build failed' }
    }
    $start = [Diagnostics.ProcessStartInfo]::new($fixtureExe)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $fixture = [Diagnostics.Process]::Start($start)
    if ($null -eq $fixture) { throw 'external window fixture did not launch' }
    try {
        $ready = $fixture.StandardOutput.ReadLineAsync()
        if (-not $ready.Wait(5000)) { throw 'external window fixture start timed out' }
        if ($ready.Result -cne 'ready') {
            $detail = if ($fixture.HasExited) { $fixture.StandardError.ReadToEnd() } else { 'no ready message' }
            throw "external window fixture did not start: $detail"
        }
        $cCrossProcess = @(& $crossProcessCExe $fixture.Id)
        if ($LASTEXITCODE -ne 0 -or [string]::Join("`n", $cCrossProcess) -cne 'ui.auto C cross-process ok') {
            throw "ui.auto C cross-process test failed: $cCrossProcess"
        }
        $jitCapture = Join-Path $work 'cross_process_jit.png'
        & $xl run extlibs/ui.auto/test/cross_process.xl -- $fixture.Id $jitCapture
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto cross-process test failed' }
        Confirm-Screenshot $jitCapture
        if (-not $SkipAot) {
            $aotCapture = Join-Path $work 'cross_process_aot.png'
            & $crossProcessExe $fixture.Id $aotCapture
            if ($LASTEXITCODE -ne 0) { throw 'ui.auto cross-process AOT test failed' }
            Confirm-Screenshot $aotCapture
        }
        $fixture.StandardInput.WriteLine()
        if (-not $fixture.WaitForExit(5000) -or $fixture.ExitCode -ne 0) {
            throw 'external window fixture did not exit cleanly'
        }
    } finally {
        if (-not $fixture.HasExited) {
            try { $fixture.StandardInput.WriteLine() } catch { }
            if (-not $fixture.WaitForExit(5000)) {
                $fixture.Kill()
                $fixture.WaitForExit()
            }
        }
        $fixture.Dispose()
    }
    $cExample = @(& (Join-Path $release '../tools/run_extlib_c.ps1') -Package uia `
        -Source 'release/examples/extlibs/ui_auto_c.c'
    )
    if ($LASTEXITCODE -ne 0) { throw 'ui.auto C comparison example failed' }
    if ([string]::Join("`n", $xlExample) -cne [string]::Join("`n", $cExample)) {
        throw "ui.auto C/XLang example output differs: C=$cExample XL=$xlExample"
    }
    Write-Host 'ui.auto C/XLang example parity: PASS'
    Confirm-Screenshot $imagePath
    Push-Location $work
    try {
        $cScreenshot = @(& (Join-Path $release '../tools/run_extlib_c.ps1') -Package ui.auto `
            -Source 'release/extlibs/ui.auto/examples/screenshot_c.c')
        if ($LASTEXITCODE -ne 0 -or [string]::Join("`n", $cScreenshot) -cne 'ui.auto C screenshot ok') {
            throw "ui.auto C screenshot failed: $cScreenshot"
        }
    } finally {
        Pop-Location
    }
    Confirm-Screenshot (Join-Path $work 'screenshot_c.png')
    if (-not $SkipAot) {
        $coreExe = Join-Path $work 'ui_auto_core.exe'
        & $xl build extlibs/ui.auto/test/named_type.xl -o $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto core AOT build failed' }
        & $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto core AOT run failed' }

        $exampleExe = Join-Path $work 'ui_auto_example.exe'
        & $xl build examples/extlibs/ui_auto.xl -o $exampleExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto example AOT build failed' }
        $aotExample = @(& $exampleExe)
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto example AOT run failed' }
        if ([string]::Join("`n", $aotExample) -cne [string]::Join("`n", $xlExample)) {
            throw 'ui.auto example AOT output differs from C/XLang JIT'
        }

        $boundariesExe = Join-Path $work 'ui_auto_boundaries.exe'
        & $xl build extlibs/ui.auto/test/boundaries.xl -o $boundariesExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto boundaries AOT build failed' }
        & $boundariesExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto boundaries AOT run failed' }

        $screenshotExe = Join-Path $work 'ui_auto_screenshot.exe'
        $aotImagePath = Join-Path $work 'screenshot_aot.png'
        & $xl build extlibs/ui.auto/test/screenshot.xl -o $screenshotExe
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto screenshot AOT build failed' }
        & $screenshotExe $aotImagePath
        if ($LASTEXITCODE -ne 0) { throw 'ui.auto screenshot AOT run failed' }
        Confirm-Screenshot $aotImagePath
    }
    Write-Host 'ui.auto package checks: PASS'
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('ui_auto_')) {
        throw "ui.auto test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}

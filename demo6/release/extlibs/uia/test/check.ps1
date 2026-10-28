param([switch]$StrictC, [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'uia requires Windows' }

$release = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$xl = Join-Path $release 'xl.exe'
$workRoot = [IO.Path]::GetFullPath((Join-Path $release '../build/test'))
$work = Join-Path $workRoot ('uia_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$root = [IO.Path]::GetFullPath((Join-Path $release '..'))
$inputs = @('extlibs/uia', 'include') | ForEach-Object {
    Get-ChildItem (Join-Path $release $_) -Recurse -File | ForEach-Object FullName
}
$inputs += @('xl.exe', 'xl.dll', 'xlc.dll', 'examples/extlibs/uia.xl',
    'examples/extlibs/uia_quick.xl', 'examples/extlibs/uia_quick_c.c') |
    ForEach-Object { Join-Path $release $_ }
$inputs += @('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h') | ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($file in $inputs) { $hashes[$file] = (Get-FileHash -LiteralPath $file).Hash }
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
$passed = $false

function Invoke-Program([string]$Label, [string]$Program, [string[]]$Arguments) {
    Write-Host $Label
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $Program @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${Label}: $Program $($Arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        throw "$Label failed with exit code $code"
    }
    return ($output | Out-String).Trim()
}

function Invoke-Checked([string]$Label, [string[]]$Arguments) {
    Invoke-Program $Label $xl $Arguments
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
    [void](Invoke-Checked 'XLang owned text budgets' @('run', 'extlibs/uia/test/owned_results.xl'))
    [void](Invoke-Checked 'XLang complete ownership probe' @('run', 'extlibs/uia/test/owned_probe.xl'))
    $generated = Join-Path $work 'owned_probe.c'
    [void](Invoke-Checked 'whole-call ownership cgen' @('cgen',
        'extlibs/uia/test/owned_probe.xl', '--emit', $generated))
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($suffix in @('element_name', 'element_automation_id', 'element_class_name',
        'element_value', 'element_text', 'element_snapshot_json',
        'element_cached_children_json', 'locator_for', 'focused_locator', 'events_poll_json')) {
        $expectedCount = if ($suffix -in @('locator_for', 'events_poll_json')) { 2 } else { 1 }
        $pattern = "(?m)^[ `t]*\w+\.Data[ `t]*=[ `t]*x_uia_${suffix}_owned[ `t]*\("
        if ([regex]::Matches($generatedText, $pattern).Count -ne $expectedCount) {
            throw "UIA producer $suffix was not directly adopted in every public path"
        }
    }
    if ($generatedText -match '(?m)^[ \t]*\w+[ \t]*=[ \t]*xlir_string_copy[ \t]*\(') {
        throw 'UIA complete ownership probe copied an adopted result'
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
        $gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
        $outputExe = Join-Path $work 'uia_c_test.exe'
        $xrtDir = (Resolve-Path (Join-Path $release '../lib/xrt')).Path
        [void](Invoke-Program 'GCC version' $gcc @('--version'))
        [void](Invoke-Program 'GCC close/wait syntax' $gcc @('-std=c11', '-Wall', '-Wextra',
            '-Werror', '-fsyntax-only', "-I$xrtDir", (Join-Path $release 'extlibs/uia/test/window_close_c.c')))
        [void](Invoke-Program 'GCC standalone C build' $gcc @('-std=c11', '-O2',
            '-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types',
            '-DXRT_IMPLEMENTATION', "-I$xrtDir", '-o', $outputExe,
            (Join-Path $release 'extlibs/uia/test/test_uia.c'), '-lole32', '-loleaut32',
            '-luser32', '-lws2_32', '-lcrypt32', '-lbcrypt', '-liphlpapi', '-lshlwapi',
            '-lshell32', '-ladvapi32', '-lgdi32', '-lwinhttp', '-lwsock32'))
        [void](Invoke-Program 'GCC standalone C run' $outputExe @())
    }

    if (-not $SkipAot) {
        $closeAot = Join-Path $work 'window_close.exe'
        [void](Invoke-Checked 'XLang close/wait AOT build' @('build',
            'extlibs/uia/test/window_close.xl', '-o', $closeAot))
        $closeOutput = Invoke-Program 'XLang close/wait AOT' $closeAot @()
        if ($LASTEXITCODE -ne 0 -or ($closeOutput | Out-String).Trim() -cne $closeC) {
            throw 'XLang close/wait AOT output differs from C/JIT'
        }
        $implicitAot = Join-Path $work 'implicit_handles.exe'
        [void](Invoke-Checked 'XLang implicit handles AOT build' @('build',
            'extlibs/uia/test/test_uia_implicit_handles.xl', '-o', $implicitAot))
        $implicitOutput = Invoke-Program 'XLang implicit handles AOT' $implicitAot @()
        if ($LASTEXITCODE -ne 0 -or ($implicitOutput | Out-String).Trim() -cne $implicit) {
            throw 'XLang implicit handles AOT output differs from JIT'
        }
        $focusAot = Join-Path $work 'focus_callback.exe'
        [void](Invoke-Checked 'XLang focus callback AOT build' @('build',
            'extlibs/uia/test/test_uia_focus_callback.xl', '-o', $focusAot))
        $focusOutput = Invoke-Program 'XLang focus callback AOT' $focusAot @()
        if ($LASTEXITCODE -ne 0 -or ($focusOutput | Out-String).Trim() -cne $focus) {
            throw 'XLang focus callback AOT output differs from JIT'
        }
        $aot = Join-Path $work 'aot_core.exe'
        [void](Invoke-Checked 'XLang core AOT build' @('build', 'extlibs/uia/test/aot_core.xl', '-o', $aot))
        [void](Invoke-Program 'XLang core AOT' $aot @())

        $fullAot = Join-Path $work 'uia_full.exe'
        [void](Invoke-Checked 'XLang full example AOT build' @('build',
            'examples/extlibs/uia.xl', '-o', $fullAot))
        $fullOutput = Invoke-Program 'XLang full example AOT' $fullAot @()
        if ($LASTEXITCODE -ne 0 -or ($fullOutput | Out-String).Trim() -cne $full) {
            throw 'XLang full example AOT output differs from JIT'
        }

        $pairedAot = Join-Path $work 'uia_quick.exe'
        [void](Invoke-Checked 'XLang paired example AOT build' @('build',
            'examples/extlibs/uia_quick.xl', '-o', $pairedAot))
        $pairedOutput = Invoke-Program 'XLang paired example AOT' $pairedAot @()
        if ($LASTEXITCODE -ne 0 -or ($pairedOutput | Out-String).Trim() -cne $cExample) {
            throw 'XLang paired example AOT output differs from C'
        }
        foreach ($name in @('owned_results', 'owned_probe')) {
            $target = Join-Path $work ($name + '.exe')
            [void](Invoke-Checked "XLang $name AOT build" @('build',
                "extlibs/uia/test/$name.xl", '-o', $target))
            [void](Invoke-Program "XLang $name AOT" $target @())
        }
    }

    foreach ($file in $inputs) {
        if ((Get-FileHash -LiteralPath $file).Hash -cne $hashes[$file]) {
            throw "UIA validation input changed: $file"
        }
    }
    $passed = $true
    Write-Host $(if ($SkipAot) { 'uia C/JIT checks: PASS (AOT skipped)' }
        else { 'uia package checks: PASS' })
} finally {
    Pop-Location
    $safeRoot = $workRoot.TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe UIA test directory' }
    if ($passed) {
        Get-ChildItem -LiteralPath $target -File | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
        Remove-Item -LiteralPath $target -Force
    } else { Write-Warning "UIA failure artifacts retained: $target" }
}

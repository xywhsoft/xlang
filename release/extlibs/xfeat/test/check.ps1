param([switch]$ParallelLive, [switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$buildRoot = Join-Path $root 'build/test/xfeat'
$build = Join-Path $buildRoot ('guard_' + [guid]::NewGuid().ToString('N'))
$release = Join-Path $root 'release'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
if ($ParallelLive -and -not $env:XFEAT_MODEL) {
    throw '-ParallelLive requires XFEAT_MODEL'
}
[void][IO.Directory]::CreateDirectory($build)
$utf8Dir = Join-Path $build '中文路径'
[void][IO.Directory]::CreateDirectory($utf8Dir)
$fake = Join-Path $build 'fake_ort.dll'
$utf8Fake = Join-Path $utf8Dir 'fake_ort.dll'
$realBridge = Join-Path $release 'extlibs/onnxruntime/bin/onnxruntime_xlang.dll'
$sessionModel = Join-Path $release 'extlibs/rapidocr/models/ch_ppocr_mobile_v2.0_cls_infer.onnx'
$passed = $false
$log = Join-Path $build 'commands.log'
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $release 'extlibs/xfeat') -Recurse -File |
    ForEach-Object { $_.FullName })
$inputs += @('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/xl.exe', 'release/xl.dll',
    'release/xlc.dll', 'release/examples/extlibs/xfeat.xl', 'test/cases/extlib_xfeat.xl') |
    ForEach-Object { Join-Path $root $_ }
$inputs += $sessionModel
foreach ($dependency in @('opencv', 'onnxruntime')) {
    $inputs += Get-ChildItem -LiteralPath (Join-Path $release "extlibs/$dependency") -Recurse -File |
        Where-Object { $_.Extension -in @('.c', '.h', '.xl', '.dll') -or $_.Name -eq 'xlang.package.json' } |
        ForEach-Object { $_.FullName }
}
$inputs = @($inputs | Sort-Object -Unique)
$hashes = @{}
foreach ($file in $inputs) {
    $hashes[$file] = (Get-FileHash -LiteralPath $file).Hash
    Add-Content -LiteralPath (Join-Path $build 'inputs.sha256') -Value "$($hashes[$file])  $file"
}

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $prior }
    Add-Content -LiteralPath $log -Value @("[$label] cwd=$pwd", "$program $($arguments -join ' ')",
        $output, "exit=$code")
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[xfeat] ${label}: PASS"
    return $output
}

Push-Location $root
try {
    [void](Run 'GCC version' $gcc @('--version'))
    Run 'strict C build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_PATH_SYSTEM', '-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_ONCE',
        '-DXRT_MODULE_MUTEX', '-DXRT_MODULE_THREAD_KEY',
        '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER',
        '-Ilib/xrt', '-Irelease/extlibs/xfeat/include', 'release/extlibs/xfeat/test/test_match.c',
        'release/extlibs/xfeat/test/runtime.c', '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm',
        '-o', (Join-Path $build 'test_match.exe'))
    Run 'matching core' (Join-Path $build 'test_match.exe') @()
    Run 'fake bridge build' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror', '-shared',
        'release/extlibs/xfeat/test/fake_ort.c', '-o', $fake)
    Copy-Item -LiteralPath $fake -Destination $utf8Fake -Force
    Run 'C bridge build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_PATH_SYSTEM', '-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_ONCE',
        '-DXRT_MODULE_MUTEX', '-DXRT_MODULE_THREAD_KEY',
        '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER',
        '-DXRT_MODULE_MEMORY_DEBUG',
        '-Ilib/xrt', '-Irelease/extlibs/xfeat/include',
        'release/extlibs/xfeat/test/test_loader.c', 'release/extlibs/xfeat/src/xfeat_artifact.c',
        'release/extlibs/xfeat/test/runtime.c', '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm',
        '-o', (Join-Path $build 'test_loader.exe'))
    $oldBridge = $env:ONNXRUNTIME_XLANG_DLL
    try {
        $env:ONNXRUNTIME_XLANG_DLL = $utf8Fake
        Run 'UTF-8 bridge and feature rows' (Join-Path $build 'test_loader.exe') @()
        if (-not (Test-Path -LiteralPath $realBridge) -or
            -not (Test-Path -LiteralPath $sessionModel)) {
            throw 'real ONNX bridge or session test model is missing'
        }
        $env:ONNXRUNTIME_XLANG_DLL = $realBridge
        Push-Location $release
        try {
            Run 'XLang real session ownership' (Join-Path $release 'xl.exe') @('run', 'extlibs/xfeat/test/session_lifetime.xl')
        } finally {
            Pop-Location
        }
    } finally {
        $env:ONNXRUNTIME_XLANG_DLL = $oldBridge
    }
    Push-Location $release
    try {
        $smoke = @(Run 'XLang ABI example' (Join-Path $release 'xl.exe') @('run', 'examples/extlibs/xfeat.xl'))
        if (($smoke -join "`n").Trim() -cne 'xfeat extlib ok') {
            throw "XLang ABI smoke failed: $($smoke -join "`n")"
        }
        Write-Host '[xfeat] XLang ABI smoke: PASS'
        Run 'XLang JSON allocation failure' (Join-Path $release 'xl.exe') @('run', 'extlibs/xfeat/test/json_failure.xl')
        Run 'XLang owned result budgets' (Join-Path $release 'xl.exe') @('run', 'extlibs/xfeat/test/owned_results.xl')
        $generated = Join-Path $build 'owned_results.c'
        Run 'whole-call ownership cgen' (Join-Path $release 'xl.exe') @('cgen',
            'extlibs/xfeat/test/owned_results.xl', '--emit', $generated)
        $text = [IO.File]::ReadAllText($generated)
        foreach ($name in @('stats', 'location')) {
            $pattern = "(?m)^[ `t]*\w+\.Data[ `t]*=[ `t]*xfeat_${name}_json_native[ `t]*\("
            if ([regex]::Matches($text, $pattern).Count -ne 1) {
                throw "XFeat $name JSON was not directly adopted"
            }
        }
        if ($text -match 'xfeat_take_json') { throw 'XFeat copied its nullable success value' }
        Run 'XLang saved image result' (Join-Path $release 'xl.exe') @('run', 'extlibs/xfeat/test/save_result.xl')
        Run 'XLang NUL argument boundaries' (Join-Path $release 'xl.exe') @('run', 'extlibs/xfeat/test/nul_arguments.xl')
        Run 'XLang catalog contract' (Join-Path $release 'xl.exe') @('run', '../test/cases/extlib_xfeat.xl')
    } finally {
        Pop-Location
    }
    if (-not $SkipAot) {
        $core = Join-Path $build 'aot_core.exe'
        Run 'XLang AOT core build' (Join-Path $release 'xl.exe') @(
            'build', 'release/extlibs/xfeat/test/aot_core.xl', '-o', $core)
        Run 'XLang AOT core' $core @()
        $example = Join-Path $build 'xfeat_example.exe'
        Run 'XLang AOT example build' (Join-Path $release 'xl.exe') @(
            'build', 'release/examples/extlibs/xfeat.xl', '-o', $example)
        $oldBridge = $env:ONNXRUNTIME_XLANG_DLL
        try {
            $env:ONNXRUNTIME_XLANG_DLL = $realBridge
            $smoke = @(Run 'XLang AOT example run' $example @())
            if (($smoke -join "`n").Trim() -cne 'xfeat extlib ok') {
                throw "XLang AOT example failed: $($smoke -join "`n")"
            }
            Write-Host '[xfeat] XLang AOT example: PASS'
        } finally {
            $env:ONNXRUNTIME_XLANG_DLL = $oldBridge
        }
        foreach ($case in @('owned_results', 'json_failure', 'nul_arguments', 'save_result')) {
            $target = Join-Path $build ($case + '.exe')
            Run "XLang $case AOT build" (Join-Path $release 'xl.exe') @('build',
                "release/extlibs/xfeat/test/$case.xl", '-o', $target)
            Push-Location $release
            try { Run "XLang $case AOT" $target @() } finally { Pop-Location }
        }
    }
    if ($env:XFEAT_MODEL -or $env:XFEAT_TEST_IMAGE) {
        if (-not $env:XFEAT_MODEL) {
            throw 'XFEAT_TEST_IMAGE requires XFEAT_MODEL'
        }
        $model = (Resolve-Path -LiteralPath $env:XFEAT_MODEL).Path
        $image = Join-Path $PSScriptRoot 'assets/target.png'
        if ($env:XFEAT_TEST_IMAGE) {
            $image = (Resolve-Path -LiteralPath $env:XFEAT_TEST_IMAGE).Path
        }
        $unrelated = Join-Path $PSScriptRoot 'assets/unrelated.png'
        $query = Join-Path $build 'live_query.png'
        $cModel = Join-Path $build 'live_model_c.exe'
        Run 'C live model build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-DXRT_MODULE_PATH_SYSTEM', '-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_ONCE',
            '-DXRT_MODULE_MUTEX', '-DXRT_MODULE_THREAD_KEY', '-DXRT_MODULE_VALUE_CONTAINER',
            '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_CHARSET',
            '-DXRT_MODULE_STRING', '-Ilib/xrt',
            'release/extlibs/xfeat/examples/live_model_c.c',
            'release/extlibs/xfeat/src/xfeat_artifact.c',
            'release/extlibs/onnxruntime/src/onnxruntime_artifact.c',
            'release/extlibs/opencv/opencv_artifact.c',
            'release/extlibs/xfeat/test/runtime.c',
            '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm', '-o', $cModel)
        Push-Location $release
        try {
            $xlLive = & (Join-Path $release 'xl.exe') 'run' `
                'extlibs/xfeat/examples/live_model.xl' '--' $model $image $query
            if ($LASTEXITCODE -ne 0) { throw 'XLang live image localization failed' }
            $xlLive | ForEach-Object { Write-Host $_ }
            Write-Host '[xfeat] XLang live image localization: PASS'
            $cLive = & $cModel $model $query $image
            if ($LASTEXITCODE -ne 0) { throw 'C live image localization failed' }
            $cLive | ForEach-Object { Write-Host $_ }
            Write-Host '[xfeat] C live image localization: PASS'
            if (($xlLive -join "`n") -cne ($cLive -join "`n")) {
                throw 'C and XLang live localization results differ'
            }
            Write-Host '[xfeat] C/XLang live output parity: PASS'
            if (-not $SkipAot) {
                $liveExe = Join-Path $build 'live_model_xl.exe'
                Run 'XLang live model AOT build' (Join-Path $release 'xl.exe') @(
                    'build', 'extlibs/xfeat/examples/live_model.xl', '-o', $liveExe)
                $aotLive = & $liveExe $model $image $query
                if ($LASTEXITCODE -ne 0 -or ($aotLive -join "`n") -cne ($cLive -join "`n")) {
                    throw 'XLang live AOT result differs from C/JIT'
                }
                Write-Host '[xfeat] C/XLang live AOT parity: PASS'
            }
            if (-not $env:XFEAT_TEST_IMAGE) {
                Run 'XLang unrelated image rejection' (Join-Path $release 'xl.exe') @(
                    'run', 'extlibs/xfeat/test/live_negative.xl', '--', $model, $unrelated, $image)
                Run 'C unrelated image rejection' $cModel @(
                    $model, $unrelated, $image, '--expect-missing')
            }
            if ($ParallelLive) {
                Run 'two-thread live localization' (Join-Path $release 'xl.exe') @(
                    'run', 'extlibs/xfeat/test/parallel_model.xl', '--', $model, $query, $image)
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Host '[xfeat] external XFeat weights: SKIP (no model supplied)'
    }
    foreach ($file in $inputs) {
        if ((Get-FileHash -LiteralPath $file).Hash -cne $hashes[$file]) {
            throw "XFeat validation input changed: $file"
        }
    }
    $passed = $true
    Write-Host $(if ($SkipAot) { '[xfeat] C/JIT checks: PASS (AOT skipped)' }
        else { '[xfeat] package checks: PASS' })
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $build -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $build).Path
        $base = (Resolve-Path -LiteralPath $buildRoot).Path.TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not [IO.Path]::GetFileName($resolved).StartsWith('guard_')) {
            throw 'xfeat test directory escaped the build root'
        }
        if ($passed) { Remove-Item -LiteralPath $resolved -Recurse -Force }
        else { Write-Warning "XFeat failure artifacts retained: $resolved" }
    }
}

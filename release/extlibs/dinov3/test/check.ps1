param([switch]$ParallelLive, [switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$buildRoot = Join-Path $root 'build/test'
$build = Join-Path $buildRoot ("dinov3_{0}" -f [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
if ($ParallelLive -and (-not $env:DINOV3_MODEL -or -not $env:DINOV3_TEST_IMAGE)) {
    throw '-ParallelLive requires DINOV3_MODEL and DINOV3_TEST_IMAGE'
}
[void][IO.Directory]::CreateDirectory($build)
$bridge = Join-Path $release 'extlibs/onnxruntime/bin/onnxruntime_xlang.dll'
$test = Join-Path $build 'test_dinov3.exe'
$live = Join-Path $build 'live_bridge.exe'
$transportModel = Join-Path $release 'extlibs/rapidocr/models/ch_ppocr_mobile_v2.0_cls_infer.onnx'
$transportImage = Join-Path $release 'extlibs/xfeat/test/assets/target.png'
$passed = $false
$log = Join-Path $build 'commands.log'
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $release 'extlibs/dinov3') -Recurse -File |
    ForEach-Object { $_.FullName })
$inputs += @('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/xl.exe', 'release/xl.dll',
    'release/xlc.dll', 'release/examples/extlibs/dinov3.xl',
    'release/extlibs/rapidocr/models/ch_ppocr_mobile_v2.0_cls_infer.onnx',
    'release/extlibs/xfeat/test/assets/target.png') | ForEach-Object { Join-Path $root $_ }
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
    Write-Host "[dinov3] ${label}: PASS"
    return $output
}

Push-Location $root
try {
    [void](Run 'GCC version' $gcc @('--version'))
    Run 'strict C build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER',
        '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY',
        '-Ilib/xrt', '-Irelease/extlibs/dinov3/include',
        'release/extlibs/dinov3/test/test_dinov3.c',
        'release/extlibs/dinov3/src/dinov3_artifact.c',
        'release/extlibs/dinov3/test/fake_ort.c',
        'release/extlibs/dinov3/test/runtime.c',
        '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm', '-o', $test)
    Run 'C result processing' $test @()
    Run 'real bridge C build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER',
        '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY',
        '-Ilib/xrt', '-Irelease/extlibs/dinov3/include',
        'release/extlibs/dinov3/test/live_bridge.c',
        'release/extlibs/dinov3/src/dinov3_artifact.c',
        'release/extlibs/onnxruntime/src/onnxruntime_artifact.c',
        'release/extlibs/dinov3/test/runtime.c',
        '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm', '-o', $live)
    if (-not (Test-Path -LiteralPath $bridge)) { throw 'ONNX bridge DLL is missing' }
    $oldBridge = $env:ONNXRUNTIME_XLANG_DLL
    try {
        $env:ONNXRUNTIME_XLANG_DLL = $bridge
        Run 'real bridge C load' $live @()
        Push-Location $release
        try {
            $jitOutput = @(Run 'XLang missing-model boundary' (Join-Path $release 'xl.exe') @('run', 'examples/extlibs/dinov3.xl'))
            if (($jitOutput -join "`n").Trim() -cne 'dinov3 extlib ok') {
                throw "XLang public example output differs: $($jitOutput -join '; ')"
            }
            Run 'XLang ONNX name boundaries' (Join-Path $release 'xl.exe') @('run', 'extlibs/dinov3/test/nul_names.xl')
            Run 'XLang unavailable output boundaries' (Join-Path $release 'xl.exe') @('run', 'extlibs/dinov3/test/invalid_output.xl')
            Run 'XLang real result ownership' (Join-Path $release 'xl.exe') @('run',
                'extlibs/dinov3/test/owned_results.xl', '--', $transportModel, $transportImage)
            $generated = Join-Path $build 'owned_results.c'
            Run 'whole-call ownership cgen' (Join-Path $release 'xl.exe') @('cgen',
                'extlibs/dinov3/test/owned_results.xl', '--emit', $generated)
            $text = [IO.File]::ReadAllText($generated)
            foreach ($name in @('embedding', 'output')) {
                $pattern = "(?m)^[ `t]*\w+\.Data[ `t]*=[ `t]*dinov3_${name}_json_native[ `t]*\("
                if ([regex]::Matches($text, $pattern).Count -ne 1) {
                    throw "DinoV3 $name JSON was not directly adopted"
                }
            }
            if ($text -match 'dinov3_take_json') { throw 'DinoV3 copied its nullable success value' }
        } finally {
            Pop-Location
        }
    } finally {
        $env:ONNXRUNTIME_XLANG_DLL = $oldBridge
    }
    if (-not $SkipAot) {
        $core = Join-Path $build 'aot_core.exe'
        Run 'XLang AOT core build' (Join-Path $release 'xl.exe') @(
            'build', 'release/extlibs/dinov3/test/aot_core.xl', '-o', $core)
        Run 'XLang AOT core' $core @()

        $example = Join-Path $build 'dinov3_example.exe'
        Run 'XLang public example AOT build' (Join-Path $release 'xl.exe') @(
            'build', 'release/examples/extlibs/dinov3.xl', '-o', $example)
        $oldBridge = $env:ONNXRUNTIME_XLANG_DLL
        try {
            $env:ONNXRUNTIME_XLANG_DLL = $bridge
            $aotOutput = @(Run 'XLang public example AOT' $example @())
            if (($aotOutput -join "`n").Trim() -cne 'dinov3 extlib ok') {
                throw "XLang public example AOT output differs: $($aotOutput -join '; ')"
            }
            foreach ($case in @('nul_names', 'invalid_output', 'owned_results')) {
                $target = Join-Path $build "$case.exe"
                Run "XLang $case AOT build" (Join-Path $release 'xl.exe') @(
                    'build', "release/extlibs/dinov3/test/$case.xl", '-o', $target)
                $caseArguments = if ($case -eq 'owned_results') { @($transportModel, $transportImage) } else { @() }
                Run "XLang $case AOT" $target $caseArguments
            }
        } finally {
            $env:ONNXRUNTIME_XLANG_DLL = $oldBridge
        }
    }
    if ($env:DINOV3_MODEL -or $env:DINOV3_TEST_IMAGE) {
        if (-not $env:DINOV3_MODEL -or -not $env:DINOV3_TEST_IMAGE) {
            throw 'set both DINOV3_MODEL and DINOV3_TEST_IMAGE'
        }
        $model = (Resolve-Path -LiteralPath $env:DINOV3_MODEL).Path
        $image = (Resolve-Path -LiteralPath $env:DINOV3_TEST_IMAGE).Path
        $cModel = Join-Path $build 'live_model_c.exe'
        Run 'C live model build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER',
            '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY', '-DXRT_MODULE_CHARSET',
            '-DXRT_MODULE_PATH_SYSTEM', '-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_STRING',
            '-Ilib/xrt', 'release/extlibs/dinov3/examples/live_model_c.c',
            'release/extlibs/dinov3/src/dinov3_artifact.c',
            'release/extlibs/onnxruntime/src/onnxruntime_artifact.c',
            'release/extlibs/opencv/opencv_artifact.c',
            'release/extlibs/dinov3/test/runtime.c',
            '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm', '-o', $cModel)
        Push-Location $release
        try {
            $xlResult = & (Join-Path $release 'xl.exe') run extlibs/dinov3/examples/live_model.xl -- $model $image
            if ($LASTEXITCODE -ne 0) { throw 'XLang live model failed' }
            $parsed = $xlResult | ConvertFrom-Json
            $cResult = & $cModel $model $image '--json'
            if ($LASTEXITCODE -ne 0) { throw 'C live model failed' }
            $cEmbedding = $cResult | ConvertFrom-Json
            if ($parsed.count -ne 384 -or $parsed.embedding.Count -ne 384 -or
                $cEmbedding.Count -ne 384) {
                throw 'C/XLang embedding shape differs'
            }
            for ($i = 0; $i -lt 384; $i++) {
                if ([math]::Abs([double]$parsed.embedding[$i] - [double]$cEmbedding[$i]) -gt 0.00001) {
                    throw "C/XLang embedding differs at index $i"
                }
            }
            Write-Host '[dinov3] C/XLang live model: PASS'
            if (-not $SkipAot) {
                $liveExe = Join-Path $build 'live_model_xl.exe'
                Run 'XLang live model AOT build' (Join-Path $release 'xl.exe') @(
                    'build', 'extlibs/dinov3/examples/live_model.xl', '-o', $liveExe)
                $aotResult = & $liveExe $model $image
                if ($LASTEXITCODE -ne 0) { throw 'XLang live model AOT failed' }
                $aotEmbedding = $aotResult | ConvertFrom-Json
                if ($aotEmbedding.count -ne 384 -or $aotEmbedding.embedding.Count -ne 384) {
                    throw 'XLang live model AOT embedding shape differs'
                }
                for ($i = 0; $i -lt 384; $i++) {
                    if ([math]::Abs([double]$aotEmbedding.embedding[$i] -
                            [double]$cEmbedding[$i]) -gt 0.00001) {
                        throw "C/XLang AOT embedding differs at index $i"
                    }
                }
                Write-Host '[dinov3] C/XLang live model AOT: PASS'
            }
            if ($ParallelLive) {
                Run 'two-thread live model calls' (Join-Path $release 'xl.exe') @(
                    'run', 'extlibs/dinov3/test/parallel_model.xl', '--', $model, $image)
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Host '[dinov3] external DinoV3 weights: SKIP (no model/image supplied)'
    }
    foreach ($file in $inputs) {
        if ((Get-FileHash -LiteralPath $file).Hash -cne $hashes[$file]) {
            throw "DinoV3 validation input changed: $file"
        }
    }
    $passed = $true
    Write-Host $(if ($SkipAot) { '[dinov3] C/JIT checks: PASS (AOT skipped)' }
        else { '[dinov3] package checks: PASS' })
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\') + '\'
    $resolvedBuild = [IO.Path]::GetFullPath($build)
    if (-not $resolvedBuild.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedBuild).StartsWith('dinov3_')) {
        throw "dinov3 test directory escapes build/test: $resolvedBuild"
    }
    if ($passed) { Remove-Item -LiteralPath $resolvedBuild -Recurse -Force }
    else { Write-Warning "DinoV3 failure artifacts retained: $resolvedBuild" }
}

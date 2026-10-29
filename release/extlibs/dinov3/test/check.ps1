param([switch]$ParallelLive, [switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$buildRoot = Join-Path $root 'build/test'
$build = Join-Path $buildRoot ("dinov3_{0}" -f [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
if ($ParallelLive -and (-not $env:DINOV3_MODEL -or -not $env:DINOV3_TEST_IMAGE)) {
    throw '-ParallelLive requires DINOV3_MODEL and DINOV3_TEST_IMAGE'
}
[void][IO.Directory]::CreateDirectory($build)
$bridge = Join-Path $release 'extlibs/onnxruntime/bin/onnxruntime_xlang.dll'
$test = Join-Path $build 'test_dinov3.exe'
$live = Join-Path $build 'live_bridge.exe'

function Run([string]$label, [string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "${label}: exit $LASTEXITCODE" }
    Write-Host "[dinov3] ${label}: PASS"
}

Push-Location $root
try {
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
            foreach ($case in @('nul_names', 'invalid_output')) {
                $target = Join-Path $build "$case.exe"
                Run "XLang $case AOT build" (Join-Path $release 'xl.exe') @(
                    'build', "release/extlibs/dinov3/test/$case.xl", '-o', $target)
                Run "XLang $case AOT" $target @()
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
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\') + '\'
    $resolvedBuild = [IO.Path]::GetFullPath($build)
    if (-not $resolvedBuild.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "dinov3 test directory escapes build/test: $resolvedBuild"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

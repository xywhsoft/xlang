param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('onnxruntime_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $release 'xl.exe'
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[onnxruntime] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

$native = 'extlibs/onnxruntime/src/onnxruntime_artifact.c'
$model = 'extlibs/rapidocr/models/ch_ppocr_mobile_v2.0_cls_infer.onnx'
Push-Location $release
try {
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'XRT once runtime build' $gcc @('-std=c11', '-O2', '-I../lib/xrt',
        '-c', 'extlibs/onnxruntime/test/runtime.c', '-o', $runtime)
    $cTest = Join-Path $work 'test_onnxruntime.exe'
    $null = Run 'C test build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY', '-I../lib/xrt', $native,
        'extlibs/onnxruntime/test/test_onnxruntime.c', $runtime, '-lm', '-o', $cTest)
    $null = Run 'C model inference' $cTest @($model)

    $unicodeRoot = Join-Path $work ('utf8-' + [char]0x4E2D + [char]0x6587)
    $unicodeBin = Join-Path $unicodeRoot 'extlibs/onnxruntime/bin'
    [void][IO.Directory]::CreateDirectory($unicodeBin)
    foreach ($dll in @('onnxruntime_xlang.dll', 'onnxruntime.dll', 'onnxruntime_providers_shared.dll')) {
        Copy-Item -LiteralPath (Join-Path $release "extlibs/onnxruntime/bin/$dll") -Destination $unicodeBin -Force
    }
    $priorDll = $env:ONNXRUNTIME_XLANG_DLL
    try {
        $env:ONNXRUNTIME_XLANG_DLL = Join-Path $unicodeBin 'onnxruntime_xlang.dll'
        $null = Run 'UTF-8 environment DLL path' $cTest @($model, '--expect-utf8-path')
        $env:ONNXRUNTIME_XLANG_DLL = $null
        $unicodeExe = Join-Path $unicodeRoot 'test_onnxruntime.exe'
        Copy-Item -LiteralPath $cTest -Destination $unicodeExe -Force
        $null = Run 'UTF-8 executable directory' $unicodeExe @($model, '--expect-utf8-path')
    } finally {
        $env:ONNXRUNTIME_XLANG_DLL = $priorDll
    }

    $partialDir = Join-Path $work 'partial'
    [void][IO.Directory]::CreateDirectory($partialDir)
    $partialDll = Join-Path $partialDir 'partial_onnxruntime.dll'
    $partialProbe = Join-Path $partialDir 'partial_probe.exe'
    $null = Run 'partial shim build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra',
        '-Werror', '-shared', 'extlibs/onnxruntime/test/partial_shim.c', '-o', $partialDll)
    $null = Run 'partial probe build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra',
        '-Werror', '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY', '-I../lib/xrt',
        $native, 'extlibs/onnxruntime/test/partial_probe.c', $runtime, '-lm',
        '-o', $partialProbe)
    try {
        $env:ONNXRUNTIME_XLANG_DLL = $partialDll
        Push-Location $partialDir
        try {
            $null = Run 'partial shim rejected' $partialProbe @()
            $env:ONNXRUNTIME_XLANG_DLL = Join-Path $partialDir 'missing.dll'
            $null = Run 'explicit missing shim rejected' $partialProbe @('missing')
        } finally {
            Pop-Location
        }
    } finally {
        $env:ONNXRUNTIME_XLANG_DLL = $priorDll
    }

    $cExample = Join-Path $work 'model_info_c.exe'
    $null = Run 'C example build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY', '-I../lib/xrt', $native,
        'extlibs/onnxruntime/examples/model_info_c.c', $runtime, '-o', $cExample)
    $cOutput = Run 'C example' $cExample @()
    $directOutput = Run 'C example direct JIT' $xl @('run', 'extlibs/onnxruntime/examples/model_info_c.c')
    if (!$SkipAot) {
        $directExe = Join-Path $work 'model_info_direct.exe'
        $null = Run 'C example direct build' $xl @('build', 'extlibs/onnxruntime/examples/model_info_c.c', '-o', $directExe)
        $directAot = Run 'C example direct executable' $directExe @()
        if ($cOutput -cne $directAot) { throw 'C direct executable metadata differs' }
    }

    $publicOutput = Run 'XLang existing API example' $xl @('run', 'examples/extlibs/onnxruntime.xl')
    if ($publicOutput -cne 'onnxruntime extlib ok') {
        throw "XLang public API example output differs: $publicOutput"
    }
    $null = Run 'XLang model inference JIT' $xl @('run', 'extlibs/onnxruntime/test/model_probe.xl')
    $null = Run 'XLang implicit handle cleanup' $xl @('run',
        'extlibs/onnxruntime/test/implicit_handles_cleanup.xl')
    $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/onnxruntime/examples/model_info.xl')
    if ($cOutput -cne $xlOutput -or $cOutput -cne $directOutput) {
        throw 'C/XLang model metadata differs'
    }

    if (!$SkipAot) {
        $cleanup = Join-Path $work 'implicit_handles_cleanup.exe'
        $null = Run 'implicit handle cleanup AOT build' $xl @('build',
            'extlibs/onnxruntime/test/implicit_handles_cleanup.xl', '-o', $cleanup)
        $null = Run 'implicit handle cleanup AOT' $cleanup @()
        $binary = Join-Path $work 'model_probe.exe'
        $null = Run 'XLang model inference AOT build' $xl @('build', 'extlibs/onnxruntime/test/model_probe.xl', '-o', $binary)
        $null = Run 'XLang model inference AOT' $binary @()

        $exampleExe = Join-Path $work 'model_info_xl.exe'
        $null = Run 'XLang example AOT build' $xl @('build', 'extlibs/onnxruntime/examples/model_info.xl', '-o', $exampleExe)
        $exampleOutput = Run 'XLang example AOT' $exampleExe @()
        if ($exampleOutput -cne $cOutput) { throw 'XLang AOT model metadata differs' }

        $publicExe = Join-Path $work 'onnxruntime_public_xl.exe'
        $null = Run 'XLang public API example AOT build' $xl @(
            'build', 'examples/extlibs/onnxruntime.xl', '-o', $publicExe)
        $publicAot = Run 'XLang public API example AOT' $publicExe @()
        if ($publicAot -cne $publicOutput) {
            throw 'XLang public API example JIT/AOT output differs'
        }
    }
} finally {
    Pop-Location
    $safeRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe onnxruntime work directory' }
    Remove-Item -LiteralPath $target -Recurse -Force
}

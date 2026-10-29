param([switch]$SkipAot, [switch]$Stress)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/rapidocr'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
$release = Join-Path $root 'release'
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
[void][IO.Directory]::CreateDirectory($work)
$previousTestDir = $env:RAPIDOCR_TEST_DIR
$previousTestImage = $env:RAPIDOCR_TEST_IMAGE
$env:RAPIDOCR_TEST_DIR = $work
$env:RAPIDOCR_TEST_IMAGE = Join-Path $work 'hello.png'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_JSON', '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_FILE_WHOLE', '-DXRT_MODULE_PATH_SYSTEM', '-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_ONCE', '-DXRT_MODULE_THREAD_KEY', '-DXRT_MODULE_DIR',
    '-Ilib/xrt', '-Irelease/extlibs/rapidocr/include', '-Irelease/extlibs/rapidocr/src')
$links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')

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
    Write-Host "[rapidocr] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'fixture' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        'release/extlibs/rapidocr/test/make_fixture.ps1', '-Target', $env:RAPIDOCR_TEST_IMAGE)
    $runtime = Join-Path $work 'runtime.o'
    $runtimeSource = Join-Path $root 'release/extlibs/rapidocr/test/runtime.c'
    $xrtHeader = Join-Path $root 'lib/xrt/xrt.h'
    if (!(Test-Path -LiteralPath $runtime) -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt (Get-Item -LiteralPath $runtimeSource).LastWriteTimeUtc -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt (Get-Item -LiteralPath $xrtHeader).LastWriteTimeUtc) {
        $null = Run 'XRT runtime build' $gcc @('-std=c11', '-O2', '-Ilib/xrt', '-c',
            'release/extlibs/rapidocr/test/runtime.c', '-o', $runtime)
    }
    $binary = Join-Path $work 'test_rapidocr.exe'
    $null = Run 'strict C build' $gcc ($flags + @(
        'release/extlibs/rapidocr/src/rapidocr_artifact.c',
        'release/extlibs/rapidocr/src/rapidocr_json.c',
        'release/extlibs/rapidocr/test/test_rapidocr.c', $runtime) + $links + @('-o', $binary))
    $null = Run 'C smoke' $binary @()
    $partial = Join-Path $work 'partial_rapidocr.dll'
    $probe = Join-Path $work 'partial_probe.exe'
    $null = Run 'partial shim build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra',
        '-Werror', '-shared', 'release/extlibs/rapidocr/test/partial_shim.c', '-o', $partial)
    $null = Run 'partial probe build' $gcc ($flags + @(
        'release/extlibs/rapidocr/src/rapidocr_artifact.c',
        'release/extlibs/rapidocr/src/rapidocr_json.c',
        'release/extlibs/rapidocr/test/partial_probe.c', $runtime) + $links + @('-o', $probe))
    $previousShim = $env:RAPIDOCR_XLANG_DLL
    try {
        $env:RAPIDOCR_XLANG_DLL = $partial
        $null = Run 'partial shim rejected' $probe @()
        Push-Location $release
        try {
            $null = Run 'XLang fallback with partial shim' $xl @('run',
                'extlibs/rapidocr/test/partial_shim.xl')
        } finally {
            Pop-Location
        }
    } finally {
        $env:RAPIDOCR_XLANG_DLL = $previousShim
    }
    $complete = Join-Path $work 'complete_rapidocr.dll'
    $completeProbe = Join-Path $work 'complete_probe.exe'
    $null = Run 'complete shim build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra',
        '-Werror', '-shared', '-Irelease/extlibs/rapidocr/include',
        'release/extlibs/rapidocr/test/complete_shim.c', '-o', $complete)
    $null = Run 'complete probe build' $gcc ($flags + @(
        'release/extlibs/rapidocr/src/rapidocr_artifact.c',
        'release/extlibs/rapidocr/src/rapidocr_json.c',
        'release/extlibs/rapidocr/test/complete_probe.c', $runtime) + $links + @('-o', $completeProbe))
    $previousShim = $env:RAPIDOCR_XLANG_DLL
    try {
        $env:RAPIDOCR_XLANG_DLL = $complete
        $null = Run 'complete C shim call-through' $completeProbe @()
        Push-Location $release
        try {
            $null = Run 'complete XLang shim call-through' $xl @('run',
                'extlibs/rapidocr/test/complete_shim.xl')
            $null = Run 'complete XLang shim implicit cleanup' $xl @('run',
                'extlibs/rapidocr/test/implicit_shim_cleanup.xl')
            if (!$SkipAot) {
                $shimCleanup = Join-Path $work 'implicit_shim_cleanup.exe'
                $null = Run 'complete shim cleanup AOT build' $xl @('build',
                    'extlibs/rapidocr/test/implicit_shim_cleanup.xl', '-o', $shimCleanup)
                $null = Run 'complete shim cleanup AOT' $shimCleanup @()
            }
        } finally {
            Pop-Location
        }
    } finally {
        $env:RAPIDOCR_XLANG_DLL = $previousShim
    }
    if ($Stress) {
        $cold = Join-Path $work 'cold_load_c.exe'
        $null = Run 'cold loader C build' $gcc ($flags + @('-DXRT_MODULE_CHARSET',
            '-DXRT_MODULE_STRING', 'release/extlibs/opencv/opencv_artifact.c',
            'release/extlibs/onnxruntime/src/onnxruntime_artifact.c',
            'release/extlibs/rapidocr/test/cold_load_runtime.c',
            'release/extlibs/rapidocr/test/cold_load_c.c') + $links + @('-o', $cold))
        Push-Location $release
        try { $null = Run 'cold loader C threads' $cold @() } finally { Pop-Location }
    }
    $cExample = Join-Path $work 'postprocess_c.exe'
    $null = Run 'C example build' $gcc ($flags + @(
        'release/extlibs/rapidocr/src/rapidocr_artifact.c',
        'release/extlibs/rapidocr/src/rapidocr_json.c',
        'release/extlibs/rapidocr/examples/postprocess_c.c', $runtime) + $links + @('-o', $cExample))
    $cOutput = Run 'C example' $cExample @()

    Push-Location $release
    try {
        $null = Run 'XLang API smoke' $xl @('run', 'examples/extlibs/rapidocr.xl')
        $generated = Join-Path $work 'owned_resource_path.c'
        $null = Run 'resource path cgen' $xl @('cgen',
            'examples/extlibs/rapidocr.xl', '--emit', $generated)
        $generatedText = [IO.File]::ReadAllText($generated)
        $ownedTextFunctions = @(
            'rapidocr_extlib_path_native',
            'rapidocr_det_boxes_from_output_native',
            'rapidocr_det_boxes_from_result_db_native',
            'rapidocr_charset_info_native',
            'rapidocr_rec_decode_native',
            'rapidocr_rec_decode_result_native',
            'rapidocr_rec_decode_result_at_charset_native',
            'rapidocr_cls_decode_result_native',
            'rapidocr_cls_decode_result_at_native',
            'rapidocr_to_ocr_items_json2_native',
            'rapidocr_assemble_result_native',
            'rapidocr_assemble_result2_native',
            'rapidocr_assemble_result4_native',
            'rapidocr_assemble_result_list_native'
        )
        foreach ($symbol in $ownedTextFunctions) {
            $pattern = '\.Data\s*=\s*' + [regex]::Escape($symbol) + '\s*\('
            if (-not $generatedText.Contains("extern char* $symbol(") -or
                -not [regex]::IsMatch($generatedText, $pattern)) {
                throw "RapidOCR owned C string is not directly adopted: $symbol"
            }
        }
        Write-Host '[rapidocr] owned text direct adoption: PASS'
        $null = Run 'XLang decoder boundaries' $xl @('run', 'extlibs/rapidocr/test/rec_decode_smoke.xl')
        $null = Run 'XLang native failure boundaries' $xl @('run', 'extlibs/rapidocr/test/native_failures.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/rapidocr/examples/postprocess.xl')
        if ($xlOutput -cne $cOutput) { throw 'C/XLang postprocess output differs' }
        $null = Run 'real model and failure paths' $xl @('run', 'extlibs/rapidocr/test/real_inference.xl')
        $null = Run 'real model implicit cleanup' $xl @('run',
            'extlibs/rapidocr/test/implicit_engine_cleanup.xl')
        if ($Stress) {
            $null = Run 'repeated inference' $xl @('run', 'extlibs/rapidocr/test/repeat_inference.xl')
            $null = Run 'parallel inference' $xl @('run', 'extlibs/rapidocr/test/parallel_inference.xl')
            $null = Run 'cold loader XLang threads' $xl @('run', 'extlibs/rapidocr/test/parallel_probe.xl')
            $null = Run 'cold parallel inference' $xl @('run', 'extlibs/rapidocr/test/parallel_inference.xl', '--', '--cold')
        }
        if (!$SkipAot) {
            $aot = Join-Path $work 'real_inference.exe'
            $null = Run 'AOT build' $xl @('build', 'extlibs/rapidocr/test/real_inference.xl', '-o', $aot)
            $null = Run 'AOT inference' $aot @()
            $cleanup = Join-Path $work 'implicit_engine_cleanup.exe'
            $null = Run 'real model cleanup AOT build' $xl @('build',
                'extlibs/rapidocr/test/implicit_engine_cleanup.xl', '-o', $cleanup)
            $null = Run 'real model cleanup AOT' $cleanup @()
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $env:RAPIDOCR_TEST_DIR = $previousTestDir
    $env:RAPIDOCR_TEST_IMAGE = $previousTestImage
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "rapidocr temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}

param([switch]$RealGpu, [switch]$Stress, [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$buildParent = Join-Path $root 'build/test/op'
$build = Join-Path $buildParent ('run_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$tcc = Join-Path $root 'build/tmp/tcc_bootstrap.exe'
$xl = Join-Path $root 'release/xl.exe'
[void][IO.Directory]::CreateDirectory($build)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "${label}: exit $LASTEXITCODE" }
    Write-Host "[op] ${label}: PASS"
}

function RunGpu([string]$label, [string]$program, [string]$hook) {
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $program
    $start.Arguments = '"' + $hook + '"'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($start)
    try {
        if (-not $process.WaitForExit(15000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "${label}: timed out"
        }
        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        if ($process.ExitCode -ne 0) {
            throw "${label}: exit $($process.ExitCode):`n$stdout$stderr"
        }
        Write-Host "[op] ${label}: PASS"
    } finally {
        $process.Dispose()
    }
}

Push-Location $root
try {
    if (!$Stress -and !$RealGpu) {
        $manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/op/xlang.package.json') -Raw | ConvertFrom-Json
        $moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
        Run 'GCC package ABI syntax' $gcc (@('-std=c11', '-Wall', '-Wextra',
            '-Wno-cast-function-type', '-Wno-unused-parameter', '-Werror',
            '-fsyntax-only', '-Ilib/xrt') + $moduleFlags +
            @('release/extlibs/op/op_native.c'))
        Run 'C read-only smoke' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/readonly_c_smoke.c')
        Run 'C random int boundaries' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/random_int.c')
        Run 'C delay boundaries' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/delay.c')
        Run 'C input parsing' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/input_parsing_native.c')
        $cExample = @(& (Join-Path $root 'tools/run_extlib_c.ps1') -Package op -Source 'release/examples/extlibs/op_c.c')
        if ($LASTEXITCODE -ne 0) { throw "C example: exit $LASTEXITCODE" }
        Push-Location (Join-Path $root 'release')
        try {
            Run 'XLang read-only smoke' $xl @('run', 'extlibs/op/test/readonly_smoke.xl',
                'extlibs/op/test/window_fixture.c')
            Run 'XLang NUL call boundary' $xl @('run', 'extlibs/op/test/nul_arguments.xl',
                'extlibs/op/test/window_fixture.c')
            Run 'XLang random int boundaries' $xl @('run', 'extlibs/op/test/random_int.xl')
            Run 'XLang delay boundaries' $xl @('run', 'extlibs/op/test/delay.xl')
            Run 'XLang input parsing' $xl @('run', 'extlibs/op/test/input_parsing.xl',
                'extlibs/op/test/input_window_fixture.c')
            $xlExample = @(& $xl run examples/extlibs/op.xl)
            if ($LASTEXITCODE -ne 0) { throw "XLang example: exit $LASTEXITCODE" }
            if (($cExample -join "`n") -ne ($xlExample -join "`n")) {
                throw "C and XLang examples differ: C=$cExample XL=$xlExample"
            }
            Write-Host '[op] C/XLang call smoke: PASS'
            if (!$SkipAot) {
                $core = Join-Path $build ("op_aot_core_$([guid]::NewGuid().ToString('N')).exe")
                $example = Join-Path $build ("op_aot_example_$([guid]::NewGuid().ToString('N')).exe")
                try {
                    Run 'XLang core AOT build' $xl @('build', 'extlibs/op/test/aot_core.xl', '-o', $core)
                    Run 'XLang core AOT call' $core @()
                    Run 'XLang example AOT build' $xl @('build', 'examples/extlibs/op.xl', '-o', $example)
                    $aotOutput = @(& $example)
                    if ($LASTEXITCODE -ne 0) { throw "XLang example AOT call: exit $LASTEXITCODE" }
                    if (($cExample -join "`n") -ne ($aotOutput -join "`n")) {
                        throw "C and XLang AOT examples differ: C=$cExample AOT=$aotOutput"
                    }
                    Write-Host '[op] XLang example AOT parity: PASS'
                } finally {
                    foreach ($output in @($core, $example)) {
                        if (Test-Path -LiteralPath $output -PathType Leaf) {
                            Remove-Item -LiteralPath $output -Force
                        }
                    }
                }
            }
        } finally {
            Pop-Location
        }
        return
    }
    $bounds = Join-Path $build 'hook_bounds.exe'
    Run 'strict bounds build' $gcc @('-std=c11', '-O2', '-DNDEBUG', '-Wall', '-Wextra', '-Werror',
        'release/extlibs/op/test/test_hook_bounds.c', '-o', $bounds)
    Run 'hook bounds' $bounds @()
    $protocol = Join-Path $build 'hook_protocol.exe'
    Run 'strict protocol build' $gcc @('-std=c11', '-O2', '-DNDEBUG', '-Wall', '-Wextra', '-Werror',
        'release/extlibs/op/test/test_hook_protocol.c', '-o', $protocol)
    Run 'hook protocol' $protocol @()
    Run 'SDK D3D9 ABI slots' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', 'release/extlibs/op/test/sdk_d3d9_abi.c')
    Run 'SDK D3D11 ABI slots' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', 'release/extlibs/op/test/sdk_d3d11_abi.c')
    if (Test-Path -LiteralPath $tcc) {
        foreach ($name in @('op_d3d9_hook', 'op_d3d11_hook', 'op_wgl_hook')) {
            Run "TCC $name compile" $tcc @('-B', 'res/tcc/win64/x86_64', '-c',
                '-o', (Join-Path $build "$name.o"), "release/extlibs/op/hooks/$name.c")
        }
        Run 'TCC package source compile' $tcc @('-B', 'res/tcc/win64/x86_64',
            '-Ilib/xrt', '-c', '-o', (Join-Path $build 'op_native.o'),
            'release/extlibs/op/op_native.c')
    }
    Run 'GCC package ABI syntax' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Wno-cast-function-type', '-Wno-unused-parameter', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', 'release/extlibs/op/op_native.c')
    Run 'GCC D3D11 hook syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', 'release/extlibs/op/hooks/op_d3d11_hook.c')
    Run 'GCC D3D9 hook syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', 'release/extlibs/op/hooks/op_d3d9_hook.c')
    Run 'GCC WGL hook syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', 'release/extlibs/op/hooks/op_wgl_hook.c')
    $processName = (-join @([char]0x4E2D, [char]0x6587, [char]0x8FDB, [char]0x7A0B)) + '.exe'
    $moduleName = (-join @([char]0x4E2D, [char]0x6587, [char]0x6A21, [char]0x5757)) + '.dll'
    Run 'UTF-8 process child build' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        'release/extlibs/op/test/utf8_process_child.c', '-o', (Join-Path $build $processName), '-luser32')
    Run 'UTF-8 module build' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-shared', 'release/extlibs/op/test/utf8_process_module.c', '-o', (Join-Path $build $moduleName))
    $clipboardRunner = Join-Path $build 'clipboard_isolated_runner.exe'
    Run 'isolated clipboard runner build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        'release/extlibs/clipboard/test/isolated_runner.c', '-luser32', '-o', $clipboardRunner)
    if (Test-Path -LiteralPath $xl) {
        Run 'C read-only smoke' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/readonly_c_smoke.c')
        Run 'C input parsing' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/input_parsing_native.c')
        Run 'C GDI pixels and frame bounds' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/gdi_frame.c')
        Run 'C hook timeout and pending cleanup' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/hook_timeout.c')
        Run 'C independent text results across threads' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/text_threads.c')
        Run 'C UTF-8 window title' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/utf8_window.c')
        Run 'C UTF-8 process/module' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/utf8_process.c')
        Run 'C UTF-8 BMP and malformed headers' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/utf8_bmp.c')
        Run 'C search region bounds' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/region_bounds.c')
        Run 'C process memory and cross-page scan' (Join-Path $root 'tools/run_extlib_c.ps1') @(
            'op', 'release/extlibs/op/test/memory_scan.c')
        $cExample = @(& (Join-Path $root 'tools/run_extlib_c.ps1') -Package op -Source 'release/examples/extlibs/op_c.c')
        if ($LASTEXITCODE -ne 0) { throw "C example: exit $LASTEXITCODE" }
        $cMemory = @(& (Join-Path $root 'tools/run_extlib_c.ps1') -Package op -Source 'release/examples/extlibs/op_memory_c.c')
        if ($LASTEXITCODE -ne 0) { throw "C memory example: exit $LASTEXITCODE" }
        Push-Location (Join-Path $root 'release')
        try {
            Run 'XLang read-only smoke' $xl @('run', 'extlibs/op/test/readonly_smoke.xl',
                'extlibs/op/test/window_fixture.c')
            Run 'XLang input parsing' $xl @('run', 'extlibs/op/test/input_parsing.xl',
                'extlibs/op/test/input_window_fixture.c')
            Run 'XLang GDI pixels' $xl @('run', 'extlibs/op/test/gdi_xlang_frame.xl',
                'extlibs/op/test/gdi_window_fixture.c')
            Run 'XLang process memory smoke' $xl @('run', 'extlibs/op/test/memory_smoke.xl',
                'extlibs/op/test/memory_fixture.c')
            Run 'XLang UTF-8 window title' $xl @('run', 'extlibs/op/test/utf8_title.xl',
                'extlibs/op/test/utf8_window_fixture.c')
            Run 'XLang isolated UTF-8 clipboard' $clipboardRunner @(
                $xl, 'extlibs/op/test/utf8_clipboard.xl', (Join-Path $root 'release'))
            $xlExample = @(& $xl run examples/extlibs/op.xl)
            if ($LASTEXITCODE -ne 0) { throw "XLang example: exit $LASTEXITCODE" }
            $xlMemory = @(& $xl run examples/extlibs/op_memory.xl examples/extlibs/op_memory_fixture.c)
            if ($LASTEXITCODE -ne 0) { throw "XLang memory example: exit $LASTEXITCODE" }
        } finally {
            Pop-Location
        }
        if (($cExample -join "`n") -ne ($xlExample -join "`n")) {
            throw "C and XLang examples differ: C=$cExample XL=$xlExample"
        }
        Write-Host '[op] C/XLang read-only examples: PASS'
        if (($cMemory -join "`n") -ne ($xlMemory -join "`n")) {
            throw "C and XLang memory examples differ: C=$cMemory XL=$xlMemory"
        }
        Write-Host '[op] C/XLang process memory examples: PASS'
    }
    if ($RealGpu) {
        if (Test-Path -LiteralPath $xl) {
            Run 'direct OpenGL frame and state' (Join-Path $root 'tools/run_extlib_c.ps1') @(
                'op', 'release/extlibs/op/test/opengl_direct.c')
            Run 'DXGI desktop frame' (Join-Path $root 'tools/run_extlib_c.ps1') @(
                'op', 'release/extlibs/op/test/dxgi_capture.c')
            Run 'DXGI shared source and contexts' (Join-Path $root 'tools/run_extlib_c.ps1') @(
                'op', 'release/extlibs/op/test/dxgi_contexts.c')
        }
        $hook = (Resolve-Path -LiteralPath (Join-Path $root 'release/extlibs/op/hooks/op_d3d11_hook.dll')).Path
        foreach ($size in @(@(64, 64), @(1920, 1080))) {
            $width = $size[0]
            $height = $size[1]
            $probe = Join-Path $build "d3d11_${width}x${height}.exe"
            Run "D3D11 ${width}x${height} probe build" $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                "-DFRAME_W=${width}u", "-DFRAME_H=${height}u",
                'release/extlibs/op/test/d3d11_capture.c', '-o', $probe,
                '-ld3d11', '-ldxgi', '-luser32', '-lgdi32')
            RunGpu "D3D11 ${width}x${height} real capture" $probe $hook
        }
        $hook9 = Join-Path $build 'op_d3d9_hook.dll'
        $mh = 'release/extlibs/op/hooks/third_party/minhook/src'
        Run 'D3D9 function hook build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-shared', '-o', $hook9, 'release/extlibs/op/hooks/op_d3d9_hook.c',
            "$mh/hook.c", "$mh/buffer.c", "$mh/trampoline.c", "$mh/hde/hde64.c",
            '-luser32', '-lgdi32', '-ld3d9')
        foreach ($size in @(@(64, 64), @(1920, 1080))) {
            $width = $size[0]
            $height = $size[1]
            $probe9 = Join-Path $build "d3d9_${width}x${height}.exe"
            Run "D3D9 ${width}x${height} probe build" $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                "-DFRAME_W=${width}u", "-DFRAME_H=${height}u",
                'release/extlibs/op/test/d3d9_capture.c', '-o', $probe9,
                '-ld3d9', '-luser32', '-lgdi32')
            RunGpu "D3D9 ${width}x${height} real capture" $probe9 $hook9
        }
        $remoteTarget = Join-Path $build 'd3d9_remote_target.exe'
        Run 'D3D9 remote target build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            'release/extlibs/op/test/d3d9_remote_target.c', '-o', $remoteTarget,
            '-ld3d9', '-luser32', '-lgdi32')
        $remoteSource = 'release/extlibs/op/test/d3d9_remote_capture.c'
        $cRunner = Join-Path $root 'tools/run_extlib_c.ps1'
        Run 'D3D9 two-target remote capture and exit' $cRunner @('op', $remoteSource)

        $hookGl = Join-Path $build 'op_wgl_hook.dll'
        Run 'WGL function hook build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-shared', '-o', $hookGl, 'release/extlibs/op/hooks/op_wgl_hook.c',
            "$mh/hook.c", "$mh/buffer.c", "$mh/trampoline.c", "$mh/hde/hde64.c",
            '-luser32', '-lgdi32')
        foreach ($size in @(@(64, 64), @(1920, 1080))) {
            $width = $size[0]
            $height = $size[1]
            $probeGl = Join-Path $build "wgl_${width}x${height}.exe"
            Run "WGL ${width}x${height} probe build" $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                "-DFRAME_W=${width}u", "-DFRAME_H=${height}u",
                'release/extlibs/op/test/wgl_capture.c', '-o', $probeGl,
                '-lopengl32', '-lgdi32', '-luser32')
            RunGpu "WGL ${width}x${height} real capture" $probeGl $hookGl
        }
        $remoteGl = Join-Path $build 'wgl_remote_target.exe'
        Run 'WGL remote target build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            'release/extlibs/op/test/wgl_remote_target.c', '-o', $remoteGl,
            '-lopengl32', '-lgdi32', '-luser32')
        $remoteGlSource = 'release/extlibs/op/test/wgl_remote_capture.c'
        Run 'WGL two-target remote capture and exit' $cRunner @('op', $remoteGlSource)

        $unicodeName = [string]::Concat([char]0x4E2D, [char]0x6587)
        $unicodeRuntime = Join-Path $build $unicodeName
        $unicodeHooks = Join-Path $unicodeRuntime 'extlibs/op/hooks'
        [void][IO.Directory]::CreateDirectory($unicodeHooks)
        Copy-Item -LiteralPath (Join-Path $root 'release/xl.exe') -Destination (Join-Path $unicodeRuntime 'xl.exe')
        Copy-Item -LiteralPath (Join-Path $root 'release/xlc.dll') -Destination (Join-Path $unicodeRuntime 'xlc.dll')
        Copy-Item -LiteralPath (Join-Path $root 'release/extlibs/op/hooks/op_d3d9_hook.dll') -Destination (Join-Path $unicodeHooks 'op_d3d9_hook.dll')
        Copy-Item -LiteralPath (Join-Path $root 'release/extlibs/op/hooks/op_wgl_hook.dll') -Destination (Join-Path $unicodeHooks 'op_wgl_hook.dll')
        Run 'D3D9 UTF-8 runtime path capture' $cRunner @('op', $remoteSource, $unicodeRuntime)
        Run 'WGL UTF-8 runtime path capture' $cRunner @('op', $remoteGlSource, $unicodeRuntime)
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($buildParent).TrimEnd('\') + '\'
    $buildPath = [IO.Path]::GetFullPath($build)
    if (-not $buildPath.StartsWith($parentPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "op test directory escaped build/test: $buildPath"
    }
    [IO.Directory]::Delete($buildPath, $true)
}

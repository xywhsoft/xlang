param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$relativeWork = 'build/test/clipboard_guard_' + [guid]::NewGuid().ToString('N')
$work = Join-Path $root $relativeWork
$release = Join-Path $root 'release'
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source

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
    Write-Host "[clipboard] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-DXRT_MODULE_MEMORY_DEBUG',
    '-DXRT_MODULE_VALUE_CONTAINER', '-DXRT_MODULE_FILE_WHOLE',
    '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE',
    '-Ilib/xrt', '-Irelease/extlibs/clipboard/include')
$links = @('-luser32', '-lgdi32', '-lshell32', '-lws2_32', '-ladvapi32', '-lm')

Push-Location $root
try {
    [void][IO.Directory]::CreateDirectory($work)
    $null = Run 'public header' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-x', 'c', '-Ilib/xrt',
        'release/extlibs/clipboard/include/clipboard.h')
    $null = Run 'public header with XRT value' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-x', 'c', '-DXRT_MODULE_VALUE_CONTAINER',
        '-Ilib/xrt', 'release/extlibs/clipboard/include/clipboard.h')
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'runtime build' $gcc ($flags + @('-c', 'release/extlibs/clipboard/test/runtime.c', '-o', $runtime))
    $null = Run 'native build' $gcc ($flags + @('-c', 'release/extlibs/clipboard/clipboard_native.c', '-o', $native))
    $cTest = Join-Path $work 'test_clipboard.exe'
    $null = Run 'C test build' $gcc ($flags + @('release/extlibs/clipboard/test/test_clipboard.c', $native, $runtime) + $links + @('-o', $cTest))
    $null = Run 'isolated C test' $cTest @($relativeWork)
    $graphics = Join-Path $work 'graphic_snapshot.exe'
    $null = Run 'graphic snapshot build' $gcc ($flags +
        @('release/extlibs/clipboard/test/test_graphic_snapshot.c', $native, $runtime) +
        $links + @('-o', $graphics))
    $null = Run 'isolated graphic snapshot' $graphics @()
    $runner = Join-Path $work 'isolated_runner.exe'
    $null = Run 'isolation runner build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        'release/extlibs/clipboard/test/isolated_runner.c', '-luser32', '-o', $runner)
    $snapshotC = Join-Path $work 'snapshot_c.exe'
    $null = Run 'C snapshot example build' $gcc ($flags + @('release/extlibs/clipboard/examples/snapshot_c.c', $native, $runtime) + $links + @('-o', $snapshotC))
    $cExample = Join-Path $work 'basic_c.exe'
    $null = Run 'C example build' $gcc ($flags + @('release/extlibs/clipboard/examples/basic_c.c', $native, $runtime) + $links + @('-o', $cExample))
    $cOutput = Run 'C example' $cExample @()
    $directBasic = Run 'C example direct xl run' $xl @('run', 'release/extlibs/clipboard/examples/basic_c.c')
    $directBasicExe = Join-Path $work 'basic_direct.exe'
    $null = Run 'C example direct xl build' $xl @('build',
        'release/extlibs/clipboard/examples/basic_c.c', '-o', $directBasicExe)
    $directBasicBuilt = Run 'C example direct xl executable' $directBasicExe @()
    $directSnapshotExe = Join-Path $work 'snapshot_direct.exe'
    $null = Run 'C snapshot direct xl build' $xl @('build',
        'release/extlibs/clipboard/examples/snapshot_c.c', '-o', $directSnapshotExe)

    Push-Location $release
    try {
        $null = Run 'XLang readonly' $xl @('run', 'extlibs/clipboard/test/readonly.xl')
        $null = Run 'XLang invalid file list' $xl @('run', 'extlibs/clipboard/test/invalid_files.xl')
        $null = Run 'XLang thread state' $xl @('run', 'extlibs/clipboard/test/state_smoke.xl')
        $null = Run 'isolated implicit snapshot cleanup' $runner @($xl,
            'extlibs/clipboard/test/implicit_snapshot_cleanup.xl', $release)
        $null = Run 'isolated file-list projection' $runner @($xl,
            'extlibs/clipboard/test/file_list_projection.xl', $release)
        $null = Run 'isolated XLang mutation' $runner @($xl, 'extlibs/clipboard/test/mutate.xl', $release)
        $null = Run 'isolated C snapshot example' $runner @($snapshotC, '-', $release, '--binary')
        $null = Run 'isolated C snapshot direct xl run' $runner @($xl,
            'extlibs/clipboard/examples/snapshot_c.c', $release)
        $null = Run 'isolated C snapshot direct xl executable' $runner @($directSnapshotExe,
            '-', $release, '--binary')
        $null = Run 'isolated XLang snapshot example' $runner @($xl, 'extlibs/clipboard/examples/snapshot.xl', $release)
        $null = Run 'isolated public XLang example' $runner @($xl,
            'examples/extlibs/clipboard.xl', $release, '--restored')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/clipboard/examples/basic.xl')
        if ($cOutput -cne $xlOutput -or $directBasic -cne $cOutput -or
            $directBasicBuilt -cne $cOutput) { throw 'C/XLang example output differs' }
        $generated = Join-Path $work 'readonly.c'
        $null = Run 'generated C' $xl @('cgen', 'extlibs/clipboard/test/readonly.xl', '--emit', $generated)
        $generatedMutation = Join-Path $work 'mutate.c'
        $null = Run 'generated mutation C' $xl @('cgen', 'extlibs/clipboard/test/mutate.xl', '--emit', $generatedMutation)
        if (!$SkipAot) {
            $snapshotCleanup = Join-Path $work 'snapshot_cleanup_aot.exe'
            $null = Run 'implicit snapshot cleanup AOT build' $xl @('build',
                'extlibs/clipboard/test/implicit_snapshot_cleanup.xl', '-o', $snapshotCleanup)
            $null = Run 'isolated implicit snapshot cleanup AOT' $runner @($snapshotCleanup,
                '-', $release, '--binary')
            $binary = Join-Path $work 'readonly_aot.exe'
            $null = Run 'XLang AOT build' $xl @('build', 'extlibs/clipboard/test/readonly.xl', '-o', $binary)
            $null = Run 'XLang AOT' $binary @()
            $mutation = Join-Path $work 'mutate_aot.exe'
            $null = Run 'mutation AOT build' $xl @('build', 'extlibs/clipboard/test/mutate.xl', '-o', $mutation)
            $null = Run 'isolated mutation AOT' $runner @($mutation, '-', $release, '--binary')
        }
    } finally {
        Pop-Location
    }

    $declarations = @(Get-Content $generatedMutation -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bx_clipboard_\w+\(' })
    if ($declarations.Count -ne 26) { throw "expected 26 generated C declarations, got $($declarations.Count)" }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi, "#include `"clipboard.h`"`n$($declarations -join "`n")`n", [Text.UTF8Encoding]::new($false))
    $null = Run 'generated C ABI' $gcc ($flags + @('-fsyntax-only', $abi))
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}

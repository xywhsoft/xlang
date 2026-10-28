param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg = Join-Path $root 'release/extlibs/dotenv'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('dotenv_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$modules = @('-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_FILE_WHOLE',
    '-DXRT_MODULE_DIR_TEMP',
    '-DXRT_MODULE_MAP', '-DXRT_MODULE_STRING', '-DXRT_MODULE_UNICODE',
    '-DXRT_MODULE_MEMORY_DEBUG')
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt') + $modules
$links = @('-lWs2_32', '-lAdvapi32', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $program @arguments 2>&1
        $status = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousAction
    }
    if ($status -ne 0) {
        throw "${name}: exit $status`n$($output -join "`n")"
    }
    Write-Host "[dotenv] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $sources = @(
        'release/extlibs/dotenv/xrt_dotenv.c',
        'release/extlibs/dotenv/test/xrt_runtime.c'
    )
    $testExe = Join-Path $work 'dotenv_c_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @('release/extlibs/dotenv/test/test_dotenv.c') +
        $sources + $links + @('-o', $testExe))
    $null = Run 'C test' $testExe @()

    $exampleExe = Join-Path $work 'dotenv_c_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('release/extlibs/dotenv/examples/basic.c',
        'release/extlibs/dotenv/test/xrt_runtime.c') + $links + @('-o', $exampleExe))
    $cOutput = Run 'C example' $exampleExe @()
    $directOutput = Run 'C example xl run' $xl @('run', 'release/extlibs/dotenv/examples/basic.c')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/dotenv.xl')
    $golden = [IO.File]::ReadAllText((Join-Path $pkg 'examples/basic.expected.txt')).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $directOutput -cne $golden -or $xlOutput -cne $golden) {
        throw 'C/XLang example output differs from the common golden file'
    }
    # 中央用例的固定夹具以 DEMO6 根目录为基准，JIT 和 AOT 使用同一工作目录。
    $null = Run 'XLang boundaries' $xl @('run', 'test/cases/extlib_dotenv.xl')
    $null = Run 'XLang limit priority' $xl @('run', 'release/extlibs/dotenv/test/limits.xl')
    $null = Run 'XLang apply OOM' $xl @('run', 'release/extlibs/dotenv/test/apply_oom.xl')
    $null = Run 'XLang factory OOM' $xl @('run', 'release/extlibs/dotenv/test/create_oom.xl')
    $null = Run 'XLang replace' $xl @('run', 'release/extlibs/dotenv/test/replace.xl')
    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $privateDiagnostic = ((& $xl check release/extlibs/dotenv/test/private_native.xl 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n")
        $privateExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('parseNative') -or
        -not $privateDiagnostic.Contains('access')) {
        throw "dotenv Native member access check failed: $privateDiagnostic"
    }
    if (-not $SkipAot) {
        $testAot = Join-Path $work 'dotenv_xl_test.exe'
        $null = Run 'XLang test build' $xl @('build', 'test/cases/extlib_dotenv.xl', '-o', $testAot)
        $null = Run 'XLang test AOT' $testAot @()
        foreach ($name in @('limits', 'apply_oom', 'create_oom', 'replace')) {
            $source = "release/extlibs/dotenv/test/$name.xl"
            $binary = Join-Path $work "dotenv_$name.exe"
            $null = Run "$name AOT build" $xl @('build', $source, '-o', $binary)
            $null = Run "$name AOT" $binary @()
        }
        $exampleAot = Join-Path $work 'dotenv_xl_example.exe'
        $null = Run 'XLang example build' $xl @('build', 'release/examples/extlibs/dotenv.xl', '-o', $exampleAot)
        $aotOutput = Run 'XLang example AOT' $exampleAot @()
        if ($aotOutput -cne $golden) { throw 'XLang AOT output differs from golden file' }
        $cExampleAot = Join-Path $work 'dotenv_c_example_aot.exe'
        $null = Run 'C example AOT build' $xl @('build',
            'release/extlibs/dotenv/examples/basic.c', '-o', $cExampleAot)
        $cAotOutput = Run 'C example AOT' $cExampleAot @()
        if ($cAotOutput -cne $golden) { throw 'C AOT output differs from golden file' }
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'dotenv test directory escaped the build root'
        }
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}

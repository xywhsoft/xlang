param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('slugify_guard_' + [guid]::NewGuid().ToString('N'))
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/slugify') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_slugify.xl', 'release/examples/extlibs/slugify.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[slugify] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $flags = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
        '-Ilib/xrt', '-Irelease/extlibs/slugify',
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE')
    $null = Run 'strict C syntax' $gcc ($flags + @(
        'release/extlibs/slugify/slugify_native.c'))
    $compileFlags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($compileFlags + @('-c',
        'release/extlibs/slugify/test/xrt_runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_slugify.exe'
    $null = Run 'GCC C boundary build' $gcc ($compileFlags + @(
        'release/extlibs/slugify/slugify_native.c', 'release/extlibs/slugify/test/test_slugify.c',
        $runtime, '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()
    foreach ($name in @('test_slugify.c', 'test_threads.c', 'slugify_c.c')) {
        $source = if ($name -eq 'slugify_c.c') {
            'release/extlibs/slugify/examples/slugify_c.c'
        } else {
            "release/extlibs/slugify/test/$name"
        }
        if ($name -ne 'slugify_c.c') {
            $null = Run $name 'powershell' @('-NoProfile', '-ExecutionPolicy',
                'Bypass', '-File', $runner, '-Package', 'slugify', '-Source', $source)
        } else {
            $cOutput = Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy',
                'Bypass', '-File', $runner, '-Package', 'slugify', '-Source', $source)
        }
    }
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_slugify.xl')
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/slugify/examples/slugify_c.c')
    if ($directOutput -cne $cOutput) {
        throw 'direct C example output differs from C runner example'
    }
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/slugify/test/boundaries.xl')
    $null = Run 'XLang owned result budgets' $xl @('run',
        'release/extlibs/slugify/test/owned_results.xl')
    $generated = Join-Path $work 'owned_text.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/slugify/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    $direct = [regex]::Matches($generatedText,
        '\b\w+\.Data\s*=\s*xsl_slugify\s*\(').Count
    if ($direct -ne 1 -or $generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'slugify must adopt the native result without a later return copy'
    }
    Write-Host '[slugify] direct string adoption: PASS'
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/slugify.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang example output differs`nC: $cOutput`nXLang: $xlOutput"
    }
    Write-Host '[slugify] C/XLang output: PASS'
    if (-not $SkipAot) {
        $owned = Join-Path $work 'owned_results.exe'
        $null = Run 'owned result AOT build' $xl @('build',
            'release/extlibs/slugify/test/owned_results.xl', '-o', $owned)
        $null = Run 'owned result AOT' $owned @()
        $core = Join-Path $work 'slugify_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'test/cases/extlib_slugify.xl', '-o', $core)
        $null = Run 'XLang core AOT' $core @()
        $boundaries = Join-Path $work 'slugify_boundaries.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/slugify/test/boundaries.xl', '-o', $boundaries)
        $boundaryOutput = Run 'XLang boundaries AOT' $boundaries @()
        if ($boundaryOutput -cne 'slugify boundaries: PASS') {
            throw "unexpected XLang boundaries AOT output: $boundaryOutput"
        }
        $example = Join-Path $work 'slugify_example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/slugify.xl', '-o', $example)
        $aotOutput = Run 'XLang example AOT' $example @()
        if ($aotOutput -cne $cOutput) {
            throw 'C/XLang AOT example output differs'
        }
        $cExample = Join-Path $work 'slugify_c_example.exe'
        $null = Run 'C example build' $xl @('build',
            'release/extlibs/slugify/examples/slugify_c.c', '-o', $cExample)
        $cAotOutput = Run 'C example AOT' $cExample @()
        if ($cAotOutput -cne $cOutput) {
            throw 'C example AOT output differs from C runner example'
        }
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "slugify validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host 'slugify Windows package gate: PASS'
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "slugify temporary work path escaped build/test: $resolved"
        }
        if ($passed) {
            Get-ChildItem -LiteralPath $resolved -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $resolved -Force
        } else {
            Write-Warning "slugify failure artifacts retained: $resolved"
        }
    }
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('ini_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop |
    Select-Object -First 1).Source
[void][IO.Directory]::CreateDirectory($work)
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/ini')
$links = @('-lws2_32', '-lbcrypt', '-lshlwapi', '-liphlpapi')
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/cases/extlib_ini.xl', 'release/examples/extlibs/ini.xl',
    'release/examples/extlibs/ini_c.c') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/ini') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${label}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[ini] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    $null = Run 'strict C syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/ini',
        'release/extlibs/ini/test/test_ini.c', 'release/extlibs/ini/ini_native.c')
    $runtime = Join-Path $work 'xrt.o'
    $null = Run 'shared C runtime compile' $gcc ($flags + @('-c',
        'release/extlibs/ini/test/xrt_runtime.c', '-o', $runtime))
    foreach ($name in @('test_ini', 'test_updates', 'test_output_limits')) {
        $target = Join-Path $work "$name.exe"
        $null = Run "GCC $name build" $gcc ($flags + @(
            "release/extlibs/ini/test/$name.c", $runtime, '-o', $target) + $links)
        $null = Run "GCC $name" $target @()
    }
    $cExampleExe = Join-Path $work 'ini_c.exe'
    $null = Run 'strict GCC C example build' $gcc ($flags + @(
        'release/examples/extlibs/ini_c.c', $runtime, '-o', $cExampleExe) + $links)
    $gccOutput = Run 'strict GCC C example' $cExampleExe @()
    $profilesExe = Join-Path $work 'profiles_c.exe'
    $null = Run 'GCC profiles build' $gcc ($flags + @(
        'release/extlibs/ini/examples/profiles_c.c', $runtime, '-o', $profilesExe) + $links)
    $profilesOutput = Run 'GCC profiles' $profilesExe @()
    $interopExe = Join-Path $work 'ini_interop.exe'
    $null = Run 'independent INI interoperability build' $gcc ($flags + @(
        'release/extlibs/ini/test/interop.c', $runtime, '-o', $interopExe) + $links)
    $null = Run 'independent C/Python INI interoperability' $python @(
        'release/extlibs/ini/test/interop.py', $interopExe)
    Push-Location $release
    try {
        $null = Run 'C boundaries' $xl @('run', 'extlibs/ini/test/test_ini.c',
            '--inc-path', '../lib/xrt')
        $null = Run 'C updates direct run' $xl @('run', 'extlibs/ini/test/test_updates.c')
        $null = Run 'C output limits direct run' $xl @('run', 'extlibs/ini/test/test_output_limits.c')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/ini/test/boundaries.xl')
        $null = Run 'XLang updates' $xl @('run', 'extlibs/ini/test/updates.xl')
        $null = Run 'XLang output limits' $xl @('run', 'extlibs/ini/test/output_limits.xl')
        $profilesC = Run 'C profiles direct run' $xl @('run', 'extlibs/ini/examples/profiles_c.c')
        $profilesXl = Run 'XLang profiles' $xl @('run', 'extlibs/ini/examples/profiles.xl')
        if ($profilesC -cne $profilesOutput -or $profilesXl -cne $profilesOutput) {
            throw 'INI profile GCC/C/XLang outputs differ'
        }
        $generated = Join-Path $work 'owned_text.c'
        $null = Run 'direct-text cgen' $xl @('cgen',
            'extlibs/ini/test/boundaries.xl', '--emit', $generated)
        $generatedText = [IO.File]::ReadAllText($generated)
        foreach ($symbol in @('xini_error_text', 'xini_section_name',
            'xini_key_name', 'xini_get', 'xini_stringify_text')) {
            if ($generatedText -cnotmatch "\bv\d+\.Data\s*=\s*$symbol\s*\(") {
                throw "$symbol must adopt the native C string directly"
            }
        }
        if ($generatedText.Contains('xl6CStringCopyV1')) {
            throw 'INI text must not be copied at the native boundary'
        }
        Write-Host '[ini] direct string adoption: PASS'
        $oldErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $privateDiagnostic = ((& $xl check extlibs/ini/test/private_native.xl 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $privateExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldErrorActionPreference
        }
        if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('getNative') -or
            -not $privateDiagnostic.Contains('access')) {
            throw "INI native member access check failed: $privateDiagnostic"
        }
        $oldErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $constructorDiagnostic = ((& $xl check extlibs/ini/test/private_config_constructor.xl 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $constructorExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldErrorActionPreference
        }
        if ($constructorExitCode -eq 0 -or
            -not $constructorDiagnostic.Contains('no such exported type')) {
            throw "INI Config export check failed: $constructorDiagnostic"
        }
        $null = Run 'shared suite' $xl @('run', '../test/cases/extlib_ini.xl')
        $cOutput = Run 'C example' $xl @('run', 'examples/extlibs/ini_c.c')
        $xlOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/ini.xl')
        if ($gccOutput -cne $cOutput -or $cOutput -cne $xlOutput) {
            throw '[ini] GCC/TCC/XLang example output differs'
        }
        if (-not $SkipAot) {
            $coreExe = Join-Path $work 'float_core.exe'
            $null = Run 'AOT float core build' $xl @('build',
                'extlibs/ini/test/aot_float_core.xl', '-o', $coreExe)
            $null = Run 'AOT float core' $coreExe @()
            $exe = Join-Path $work 'boundaries.exe'
            $null = Run 'AOT boundaries build' $xl @('build',
                'extlibs/ini/test/boundaries.xl', '-o', $exe)
            $null = Run 'AOT boundaries' $exe @()
            $sharedExe = Join-Path $work 'shared_suite.exe'
            $null = Run 'AOT shared suite build' $xl @('build',
                '../test/cases/extlib_ini.xl', '-o', $sharedExe)
            $null = Run 'AOT shared suite' $sharedExe @()
            $exampleExe = Join-Path $work 'example.exe'
            $null = Run 'AOT example build' $xl @('build',
                'examples/extlibs/ini.xl', '-o', $exampleExe)
            $aotOutput = Run 'AOT example' $exampleExe @()
            if ($aotOutput -cne $xlOutput) {
                throw '[ini] AOT example output differs from C/XLang JIT'
            }
            $cExampleAot = Join-Path $work 'ini_c_aot.exe'
            $null = Run 'AOT C example build' $xl @('build',
                'examples/extlibs/ini_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'AOT C example' $cExampleAot @()
            if ($cAotOutput -cne $gccOutput) {
                throw '[ini] AOT C example output differs from GCC'
            }
            foreach ($name in @('updates', 'output_limits')) {
                $target = Join-Path $work "$name.exe"
                $null = Run "AOT $name build" $xl @('build',
                    "extlibs/ini/test/$name.xl", '-o', $target)
                $null = Run "AOT $name" $target @()
            }
            foreach ($source in @('profiles.xl', 'profiles_c.c')) {
                $profileAot = Join-Path $work ($source.Replace('.', '_') + '.exe')
                $null = Run "AOT $source build" $xl @('build',
                    "extlibs/ini/examples/$source", '-o', $profileAot)
                $output = Run "AOT $source" $profileAot @()
                if ($output -cne $profilesOutput) { throw "INI $source AOT output differs from GCC" }
            }
        }
    } finally { Pop-Location }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "INI input changed during gate: $path"
        }
    }
    $passed = $true
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('ini_guard_')) {
        throw "INI test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        if ($passed) {
            Get-ChildItem -LiteralPath $resolvedWork -File |
                ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
            Remove-Item -LiteralPath $resolvedWork
        } else {
            Write-Warning "INI failure artifacts: $resolvedWork"
        }
    }
}

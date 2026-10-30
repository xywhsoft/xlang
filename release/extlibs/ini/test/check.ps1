param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('ini_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop |
    Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
[void][IO.Directory]::CreateDirectory($work)
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/ini')
$links = @('-lws2_32', '-lbcrypt', '-lshlwapi', '-liphlpapi')
# 所有子进程统一限时、记录原始退出状态，不能让启动失败跳过命令日志。
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$failure = ''
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/cases/extlib_ini.xl', 'release/examples/extlibs/ini.xl',
    'release/examples/extlibs/ini_c.c') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/ini') -Recurse -File |
    ForEach-Object FullName)
$inputs += @($gcc, $python, $shell, (Join-Path $root 'tools/extlib_smoke_process.ps1'),
    (Join-Path $root 'tools/xlir_native_process.ps1'))
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$label, [string]$program, [string[]]$arguments,
    [int]$expectedExit = 0, [ValidateRange(1,3600)][int]$timeoutSeconds = 120) {
    if ($null -eq $arguments) { $arguments = @() }
    $directory = (Get-Location).Path
    $result = Invoke-ExtlibSmokeProcess $program $arguments $directory $timeoutSeconds
    $checks.Add([ordered]@{label=$label; program=$program; arguments=$arguments;
        directory=$directory; expectedExit=$expectedExit; result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $work 'report.json') -Encoding UTF8
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${label}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "${label}: $($result.status) exit=$($result.exitCode), expected=$expectedExit`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[ini] ${label}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    if (-not (Test-Path -LiteralPath $cc1 -PathType Leaf)) { throw "GCC cc1 not found: $cc1" }
    $cc1 = (Resolve-Path -LiteralPath $cc1).Path
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'Python version' $python @('--version')
    $null = Run 'process diagnostics' $shell @('-NoProfile','-NonInteractive',
        '-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'check_gate_diagnostics.ps1'))
    $null = Run 'strict C syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/ini',
        'release/extlibs/ini/test/test_ini.c', 'release/extlibs/ini/ini_native.c')
    $runtime = Join-Path $work 'xrt.o'
    $null = Run 'shared C runtime compile' $gcc ($flags + @('-c',
        'release/extlibs/ini/test/xrt_runtime.c', '-o', $runtime))
    foreach ($name in @('test_ini', 'test_updates', 'test_output_limits', 'test_interpolation_refs', 'test_owned_text', 'test_stream_errors', 'test_set_limits')) {
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
    $boundedExe = Join-Path $work 'bounded_updates_c.exe'
    $null = Run 'GCC bounded updates build' $gcc ($flags + @(
        'release/extlibs/ini/examples/bounded_updates_c.c', $runtime, '-o', $boundedExe) + $links)
    $boundedOutput = Run 'GCC bounded updates' $boundedExe @()
    $boundedGolden = @('oversized update rejected', '你好', '8080',
        'bounded configuration round-trip: OK') -join "`n"
    if ($boundedOutput -cne $boundedGolden) { throw 'INI bounded updates differ from golden output' }
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
        $null = Run 'C reference errors direct run' $xl @('run', 'extlibs/ini/test/test_interpolation_refs.c')
        $null = Run 'C owned text direct run' $xl @('run', 'extlibs/ini/test/test_owned_text.c')
        $null = Run 'C stream first-cause direct run' $xl @('run', 'extlibs/ini/test/test_stream_errors.c')
        $null = Run 'C item limits direct run' $xl @('run', 'extlibs/ini/test/test_set_limits.c')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/ini/test/boundaries.xl')
        $null = Run 'XLang updates' $xl @('run', 'extlibs/ini/test/updates.xl')
        $null = Run 'XLang output limits' $xl @('run', 'extlibs/ini/test/output_limits.xl')
        $null = Run 'XLang item limits' $xl @('run', 'extlibs/ini/test/set_limits.xl')
        $null = Run 'XLang reference errors' $xl @('run', 'extlibs/ini/test/interpolation_refs.xl')
        $null = Run 'XLang owned text and defaults' $xl @('run', 'extlibs/ini/test/owned_results.xl')
        $null = Run 'XLang ownership probe' $xl @('run', 'extlibs/ini/test/owned_probe.xl')
        $profilesC = Run 'C profiles direct run' $xl @('run', 'extlibs/ini/examples/profiles_c.c')
        $profilesXl = Run 'XLang profiles' $xl @('run', 'extlibs/ini/examples/profiles.xl')
        if ($profilesC -cne $profilesOutput -or $profilesXl -cne $profilesOutput) {
            throw 'INI profile GCC/C/XLang outputs differ'
        }
        foreach ($source in @('bounded_updates_c.c', 'bounded_updates.xl')) {
            $output = Run "direct $source" $xl @('run', "extlibs/ini/examples/$source")
            if ($output -cne $boundedGolden) { throw "INI $source differs from golden output" }
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
        $ownedProbe = Join-Path $work 'owned_probe.c'
        $null = Run 'whole-return ownership cgen' $xl @('cgen',
            'extlibs/ini/test/owned_probe.xl', '--emit', $ownedProbe)
        $ownedText = [IO.File]::ReadAllText($ownedProbe)
        foreach ($symbol in @('xini_error_text', 'xini_section_name', 'xini_key_name',
            'xini_get', 'xini_stringify_text')) {
            if ($ownedText -cnotmatch "\b\w+\.Data\s*=\s*$symbol\s*\(") {
                throw "$symbol must be directly adopted in the complete probe"
            }
        }
        # 保留借用默认值的独立快照，不把必要的默认值复制当作成功结果复制。
        if ([regex]::Matches($ownedText, '=\s*xlir_string_copy\s*\(').Count -gt 1) {
            throw 'INI copied an owned result in the complete return chain'
        }
        $privateDiagnostic = Run 'private native access' $xl @('check',
            'extlibs/ini/test/private_native.xl') 1
        if (-not $privateDiagnostic.Contains('getNative') -or
            -not $privateDiagnostic.Contains('access')) {
            throw "INI native member access check failed: $privateDiagnostic"
        }
        $constructorDiagnostic = Run 'private Config constructor' $xl @('check',
            'extlibs/ini/test/private_config_constructor.xl') 1
        if (-not $constructorDiagnostic.Contains('no such exported type')) {
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
            foreach ($name in @('updates', 'output_limits', 'interpolation_refs', 'owned_results', 'owned_probe', 'set_limits')) {
                $target = Join-Path $work "$name.exe"
                $null = Run "AOT $name build" $xl @('build',
                    "extlibs/ini/test/$name.xl", '-o', $target)
                $null = Run "AOT $name" $target @()
            }
            foreach ($name in @('test_owned_text', 'test_stream_errors', 'test_set_limits')) {
                $nativeTest = Join-Path $work "$name.c.exe"
                $null = Run "AOT C $name build" $xl @('build',
                    "extlibs/ini/test/$name.c", '-o', $nativeTest)
                $null = Run "AOT C $name" $nativeTest @()
            }
            foreach ($source in @('profiles.xl', 'profiles_c.c')) {
                $profileAot = Join-Path $work ($source.Replace('.', '_') + '.exe')
                $null = Run "AOT $source build" $xl @('build',
                    "extlibs/ini/examples/$source", '-o', $profileAot)
                $output = Run "AOT $source" $profileAot @()
                if ($output -cne $profilesOutput) { throw "INI $source AOT output differs from GCC" }
            }
            foreach ($source in @('bounded_updates.xl', 'bounded_updates_c.c')) {
                $target = Join-Path $work ($source.Replace('.', '_') + '.exe')
                $null = Run "AOT $source build" $xl @('build', "extlibs/ini/examples/$source", '-o', $target)
                $output = Run "AOT $source" $target @()
                if ($output -cne $boundedGolden) { throw "INI $source AOT differs from golden output" }
            }
        }
    } finally { Pop-Location }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "INI input changed during gate: $path"
        }
    }
    $passed = $true
} catch {
    $failure = $_.Exception.Message
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'), $failure,
        [Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('ini_guard_')) {
        throw "INI test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        # 报告留在独占目录旁；成功只删除该目录里的可执行文件等临时产物。
        [ordered]@{passed=$passed; failure=$failure; inputs=$hashes; checks=$checks} |
            ConvertTo-Json -Depth 10 | Set-Content -LiteralPath ($work + '.report.json') -Encoding UTF8
        if ($passed) {
            Get-ChildItem -LiteralPath $resolvedWork -File |
                ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
            Remove-Item -LiteralPath $resolvedWork
        } else {
            Write-Warning "INI failure artifacts: $resolvedWork"
        }
    }
}

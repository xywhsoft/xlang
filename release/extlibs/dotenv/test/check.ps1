param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg = Join-Path $root 'release/extlibs/dotenv'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('dotenv_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$outcome = 'failed'
$failure = ''
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$modules = @('-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_FILE_WHOLE',
    '-DXRT_MODULE_DIR_TEMP',
    '-DXRT_MODULE_MAP', '-DXRT_MODULE_STRING', '-DXRT_MODULE_UNICODE',
    '-DXRT_MODULE_MEMORY_DEBUG')
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt') + $modules
$links = @('-lWs2_32', '-lAdvapi32', '-lm')

function InputPaths {
    Get-ChildItem -LiteralPath $pkg -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'tools/check_extlib_native_globals.ps1','tools/check_extlib_native_globals.sh',
        'test/cases/extlib_dotenv.xl','test/data/dotenv_test.env','release/examples/extlibs/dotenv.xl')) {
        Join-Path $root $path
    }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$seconds = 120) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[dotenv] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    # 测试、范例复用同一份独立运行时对象，不重复编译整个 XRT。
    $runtime = Join-Path $work 'runtime.o'
    $core = Join-Path $work 'core.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'C runtime compile' $gcc ($flags + @('-c','release/extlibs/dotenv/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'C core compile' $gcc ($flags + @('-c','release/extlibs/dotenv/xrt_dotenv.c','-o',$core))
    $null = Run 'C native compile' $gcc ($flags + @('-c','release/extlibs/dotenv/dotenv_native.c','-o',$native))
    $testExe = Join-Path $work 'dotenv_c_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @('release/extlibs/dotenv/test/test_dotenv.c') +
        @($core,$runtime) + $links + @('-o', $testExe))
    $null = Run 'C test' $testExe @()

    $exampleExe = Join-Path $work 'dotenv_c_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('release/extlibs/dotenv/examples/basic.c',
        $runtime) + $links + @('-o', $exampleExe))
    $cOutput = Run 'C example' $exampleExe @()
    $directOutput = Run 'C example xl run' $xl @('run', 'release/extlibs/dotenv/examples/basic.c')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/dotenv.xl')
    $golden = [IO.File]::ReadAllText((Join-Path $pkg 'examples/basic.expected.txt')).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $directOutput -cne $golden -or $xlOutput -cne $golden) {
        throw 'C/XLang example output differs from the common golden file'
    }
    $reloadGolden = [IO.File]::ReadAllText((Join-Path $pkg 'examples/reload.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $reloadExe = Join-Path $work 'reload-c.exe'
    $null = Run 'reload C compile' $gcc ($flags + @('release/extlibs/dotenv/examples/reload_c.c',$runtime) + $links + @('-o',$reloadExe))
    $reloadC = Run 'reload C' $reloadExe @()
    $reloadDirect = Run 'reload C xl run' $xl @('run','release/extlibs/dotenv/examples/reload_c.c')
    $reloadXL = Run 'reload XLang' $xl @('run','release/extlibs/dotenv/examples/reload.xl')
    if ($reloadC -cne $reloadGolden -or $reloadDirect -cne $reloadGolden -or $reloadXL -cne $reloadGolden) {
        throw 'Reload examples differ from the common golden file'
    }
    # 中央用例的固定夹具以 DEMO6 根目录为基准，JIT 和 AOT 使用同一工作目录。
    $null = Run 'XLang boundaries' $xl @('run', 'test/cases/extlib_dotenv.xl')
    $null = Run 'XLang limit priority' $xl @('run', 'release/extlibs/dotenv/test/limits.xl')
    $null = Run 'XLang apply OOM' $xl @('run', 'release/extlibs/dotenv/test/apply_oom.xl')
    $null = Run 'XLang factory OOM' $xl @('run', 'release/extlibs/dotenv/test/create_oom.xl')
    $null = Run 'XLang replace' $xl @('run', 'release/extlibs/dotenv/test/replace.xl')
    $privateArgs = @('check','release/extlibs/dotenv/test/private_native.xl')
    $private = Invoke-ExtlibSmokeProcess $xl $privateArgs $root 90 $environment
    $checks.Add([ordered]@{name='private native rejection';program=$xl;arguments=$privateArgs;result=$private})
    if ($private.status -ne 'fail' -or !$private.stderr.Contains('parseNative') -or !$private.stderr.Contains('access')) {
        throw "dotenv Native member access check failed: $($private.stderr)"
    }
    $decls = @()
    foreach ($source in @('test/cases/extlib_dotenv.xl','release/extlibs/dotenv/examples/reload.xl')) {
        $module = Join-Path $work (([IO.Path]::GetFileNameWithoutExtension($source)) + '.c')
        $null = Run "cgen $source" $xl @('cgen',$source,'--emit',$module)
        $decls += @(Get-Content -LiteralPath $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxdotenv_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = [IO.File]::ReadAllText((Join-Path $pkg 'src/dotenv.xl'))
    $expected = @([regex]::Matches($source,'extern "(xdotenv_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxdotenv_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne $expected.Count -or @(Compare-Object $expected $actual).Count) { throw 'Dotenv generated ABI coverage incomplete' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"dotenv_native.c`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated C prototypes' $gcc ($flags + @('-Irelease/extlibs/dotenv','-fsyntax-only',$abi))
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
        foreach ($extension in @('c','xl')) {
            $reloadAot = Join-Path $work "reload-$extension.exe"
            $source = if ($extension -eq 'c') { 'release/extlibs/dotenv/examples/reload_c.c' } else { 'release/extlibs/dotenv/examples/reload.xl' }
            $null = Run "reload $extension build" $xl @('build',$source,'-o',$reloadAot)
            $text = Run "reload $extension AOT" $reloadAot @()
            if ($text -cne $reloadGolden) { throw "Reload $extension AOT differs from golden file" }
        }
    }
    $lifecycle = @('-NoProfile','-ExecutionPolicy','Bypass','-File',
        (Join-Path $root 'tools/check_extlib_native_globals.ps1'),'-Root',$root,'-Package','dotenv')
    if ($SkipAot) { $lifecycle += '-SkipAot' }
    $null = Run 'top-level lifecycle' (Get-Process -Id $PID).Path $lifecycle 360
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = Join-Path $workRoot (([IO.Path]::GetFileName($work)) + '.json')
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[dotenv] report: $report"
    if ($outcome -eq 'passed' -and (Test-Path -LiteralPath $work -PathType Container)) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'dotenv test directory escaped the build root'
        }
        Remove-Item -LiteralPath $work -Recurse -Force
    }
    if ($changed.Count) { throw 'Inputs changed during dotenv gate' }
}

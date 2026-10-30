param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('plist_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$inputs = @()
foreach ($package in @('plist', 'xml.dom', 'xml.sax', '_expat')) {
    $inputs += Get-ChildItem (Join-Path $root "release/extlibs/$package") -Recurse -File |
        ForEach-Object FullName
}
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1', 'test/cases/extlib_plist.xl',
    'tools/extlib_smoke_process.ps1', 'tools/xlir_native_process.ps1',
    'release/examples/extlibs/plist.xl', 'release/examples/extlibs/plist_c.c') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
foreach ($path in @($gcc,$python,$shell)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expected = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    if ($program.EndsWith('.ps1')) {
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;expectedExit=$expected;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expected) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[plist] ${name}: PASS"
    return (($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n") -split "`n")
}

Push-Location $root
try {
    [void][IO.Directory]::CreateDirectory($work)
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = (Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')) -join "`n"
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt')
    $links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'C runtime compile' $gcc ($flags + @('-c','release/extlibs/plist/test/runtime.c','-o',$runtime))
    foreach ($name in @('cold_state','binary_boundaries')) {
        $target = Join-Path $work ($name + '.exe')
        $null = Run "C $name compile" $gcc ($flags + @("release/extlibs/plist/test/test_$name.c",$runtime) + $links + @('-o',$target))
    }
    $cold = Join-Path $work 'cold_state.exe'
    $null = Run 'C thread state isolation' $cold @('threads')
    foreach ($kind in 0..10) {
        $complete = $false
        foreach ($budget in 0..127) {
            $output = (Run "C cold $kind/$budget" $cold @("$kind","$budget",(Join-Path $work "cold-$kind-$budget.plist"))) -join "`n"
            if ($output -match 'injected=0 PASS$') { $complete = $true; break }
        }
        if (!$complete) { throw "Cold $kind budget did not complete" }
    }
    $oracle = Join-Path $work 'oracle'
    [void][IO.Directory]::CreateDirectory($oracle)
    $null = Run 'C binary OOM/limits' (Join-Path $work 'binary_boundaries.exe') @($oracle)
    $null = Run 'Python binary fixture values' $python @('-I','release/extlibs/plist/test/check_binary_fixtures.py',$oracle)
    foreach ($source in @('test_cold_state.c','test_binary_boundaries.c')) {
        $path = "release/extlibs/plist/test/$source"
        $null = Run "C TCC $source" $xl @('run',$path)
        if (!$SkipAot) {
            $target = Join-Path $work ($source + '.exe')
            $null = Run "C AOT build $source" $xl @('build',$path,'-o',$target)
            $null = Run "C AOT $source" $target @()
            if ($source -eq 'test_cold_state.c') { $null = Run 'C AOT threads' $target @('threads') }
        }
    }
    $null = Run 'C TCC threads' $xl @('run','release/extlibs/plist/test/test_cold_state.c','--','threads')
    $coldScript = 'release/extlibs/plist/test/cold_start.xl'
    $coldAot = Join-Path $work 'cold_start.exe'
    if (!$SkipAot) { $null = Run 'XLang cold AOT build' $xl @('build',$coldScript,'-o',$coldAot) }
    foreach ($mode in @('JIT','AOT')) {
        if ($SkipAot -and $mode -eq 'AOT') { continue }
        $complete = $false
        foreach ($budget in 0..127) {
            if ($mode -eq 'JIT') { $output = (Run "XLang cold $mode/$budget" $xl @('run',$coldScript,'--',"$budget")) -join "`n" }
            else { $output = (Run "XLang cold $mode/$budget" $coldAot @("$budget")) -join "`n" }
            if ($output -match 'injected=\s*false\s+PASS$') { $complete = $true; break }
        }
        if (!$complete) { throw "XLang cold $mode budget did not complete" }
    }
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/plist/examples/portable_state.expected.txt'),
        [Text.UTF8Encoding]::new($false)).Replace("`r`n","`n").TrimEnd("`n")
    $example = Join-Path $work 'portable_gcc.exe'
    $null = Run 'GCC portable example build' $gcc ($flags + @('release/extlibs/plist/examples/portable_state_c.c',$runtime) + $links + @('-o',$example))
    if (((Run 'GCC portable golden' $example @()) -join "`n") -cne $golden) { throw 'C portable golden differs' }
    foreach ($source in @('portable_state.xl','portable_state_c.c')) {
        $path = "release/extlibs/plist/examples/$source"
        if (((Run "portable JIT $source" $xl @('run',$path)) -join "`n") -cne $golden) { throw "Portable JIT golden differs: $source" }
        if (!$SkipAot) {
            $target = Join-Path $work ($source + '.exe')
            $null = Run "portable AOT build $source" $xl @('build',$path,'-o',$target)
            if (((Run "portable AOT $source" $target @()) -join "`n") -cne $golden) { throw "Portable AOT golden differs: $source" }
        }
    }
    $null = Run 'strict native C syntax' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/plist/include',
        'release/extlibs/plist/plist_native.c', 'release/extlibs/plist/test/test_native.c')
    foreach ($source in @(
        'release/extlibs/plist/test/test_plist.c',
        'release/extlibs/plist/test/test_native.c'
    )) {
        $null = Run $source $runner @('plist', $source)
    }

    foreach ($source in @('test/cases/extlib_plist.xl',
        'release/extlibs/plist/test/boundaries.xl', 'release/extlibs/plist/test/allocations.xl',
        'release/extlibs/plist/test/owned_results.xl','release/extlibs/plist/test/binary_errors.xl')) {
        $null = Run $source $xl @('run', $source)
    }
    $generated = Join-Path $work 'owned_probe.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/plist/test/owned_probe.xl', '--emit', $generated)
    $text = [IO.File]::ReadAllText($generated)
    foreach ($symbol in @('xplist_error_message', 'xplist_stringify_text')) {
        $pattern = '\b\w+\.Data\s*=\s*' + $symbol + '\s*\('
        if ([regex]::Matches($text, $pattern).Count -ne 1) {
            throw "plist probe must directly adopt $symbol exactly once"
        }
    }
    # 此夹具仍有构树所需的值转换，约束的是旧的两个返回复制不能回归。
    if ([regex]::Matches($text, '=\s*xlir_string_copy\s*\(').Count -gt 5) {
        throw 'plist controlled fixture regained redundant return copies'
    }

    $privateDiagnostic = (Run 'private native document check' $xl @('check',
        'release/extlibs/plist/test/private_native_document_negative.xl') 1) -join "`n"
    if (-not $privateDiagnostic.Contains('no such exported type')) {
        throw "plist native document access check failed: $privateDiagnostic"
    }

    $cXml = Join-Path $work 'c.xml'
    $cBinary = Join-Path $work 'c.bplist'
    $xlXml = Join-Path $work 'xlang.xml'
    $xlBinary = Join-Path $work 'xlang.bplist'
    $xlangOutput = @(Run 'XLang example' $xl @('run',
        'release/examples/extlibs/plist.xl', '--', $xlXml, $xlBinary))
    $cOutput = @(Run 'C example' $xl @('run',
        'release/examples/extlibs/plist_c.c', '--', $cXml, $cBinary))
    if (($xlangOutput -join "`n") -cne ($cOutput -join "`n")) {
        throw 'plist C/XLang example output differs'
    }
    $null = Run 'C/XLang plistlib interop' $python @('-I',
        'release/extlibs/plist/test/check_external.py', $cXml, $xlXml, $cBinary, $xlBinary)
    if (-not $SkipAot) {
        $core = Join-Path $work 'plist_core.exe'
        $null = Run 'import AOT build' $xl @('build', 'release/extlibs/plist/test/smoke.xl', '-o', $core)
        $null = Run 'import AOT' $core @()
        $central = Join-Path $work 'central.exe'
        $null = Run 'central AOT build' $xl @('build', 'test/cases/extlib_plist.xl', '-o', $central)
        $null = Run 'central AOT' $central @()

        foreach ($case in @('boundaries', 'allocations', 'owned_results','binary_errors')) {
            $caseExe = Join-Path $work "plist_$case.exe"
            $null = Run "$case AOT build" $xl @('build', "release/extlibs/plist/test/$case.xl", '-o', $caseExe)
            $null = Run "$case AOT" $caseExe @()
        }

        $example = Join-Path $work 'plist_example.exe'
        $aotXml = Join-Path $work 'aot.xml'
        $aotBinary = Join-Path $work 'aot.bplist'
        $null = Run 'XLang example AOT build' $xl @('build', 'release/examples/extlibs/plist.xl', '-o', $example)
        $aotOutput = @(Run 'XLang example AOT' $example @($aotXml, $aotBinary))
        if (($aotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'plist example AOT output differs from C/XLang'
        }
        $null = Run 'XLang AOT plistlib interop' $python @('-I',
            'release/extlibs/plist/test/check_external.py', $cXml, $aotXml, $cBinary, $aotBinary)

        $cAot = Join-Path $work 'plist_c_example.exe'
        $cAotXml = Join-Path $work 'c_aot.xml'
        $cAotBinary = Join-Path $work 'c_aot.bplist'
        $null = Run 'C example AOT build' $xl @('build', 'release/examples/extlibs/plist_c.c', '-o', $cAot)
        $cAotOutput = @(Run 'C example AOT' $cAot @($cAotXml, $cAotBinary))
        if (($cAotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'plist C example AOT output differs'
        }
        $null = Run 'C AOT plistlib interop' $python @('-I',
            'release/extlibs/plist/test/check_external.py', $cXml, $cAotXml, $cBinary, $cAotBinary)
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "plist validation input changed: $path"
        }
    }
    $passed = $true
    if ($SkipAot) {
        Write-Output 'plist Windows C/XLang (AOT skipped) gate: PASS'
    } else {
        Write-Output 'plist Windows C/XLang/AOT gate: PASS'
    }
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work) {
        $checked = [IO.Path]::GetFullPath($work)
        if (-not $checked.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw 'unsafe plist test cleanup path' }
        [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
            Set-Content ($work + '.report.json') -Encoding UTF8
        if ($passed) { Remove-Item -LiteralPath $checked -Recurse -Force }
        else {
            Write-Warning "plist failure artifacts retained: $checked"
        }
    }
}

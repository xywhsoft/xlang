param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('ical_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop |
    Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/ical') -Recurse -File | ForEach-Object FullName)
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName)
$inputs += @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'test/cases/extlib_ical.xl','test/cases/extlib_ical_boundaries.xl',
    'release/examples/extlibs/ical.xl') | ForEach-Object {Join-Path $root $_}
$hashes = @{}
foreach ($path in @($inputs) + @($gcc,$python,$shell)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/ical/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expected = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;
        expectedExit=$expected;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expected) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[ical] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

[IO.Directory]::CreateDirectory($work) | Out-Null
Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $units = @('ical_native', 'ical_parse', 'ical_build')
    $objects = @()
    $null = Run 'strict manifest syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE',
        '-DXRT_MODULE_VALUE_CONTAINER', '-fsyntax-only') +
        @($units | ForEach-Object { "release/extlibs/ical/$_.c" }))
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/ical/test/xrt_runtime.c', '-o', $runtime))
    foreach ($unit in $units) {
        $object = Join-Path $work "$unit.o"
        $null = Run "$unit compile" $gcc ($flags + @('-c',
            "release/extlibs/ical/$unit.c", '-o', $object))
        $objects += $object
    }
    $test = Join-Path $work 'test_ical.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/ical/test/test_ical.c', $runtime) +
        $objects + $links + @('-o', $test))
    $null = Run 'C boundaries' $test @((Join-Path $work 'long.ics'))
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/ical/examples/ical.c', $runtime) +
        $links + @('-o', $example))
    $cFile = Join-Path $work 'c.ics'
    $xlFile = Join-Path $work 'xl.ics'
    $cOutput = Run 'C example' $example @($cFile)
    $directOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/ical/examples/ical.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs' }
    $fold = Join-Path $work 'fold.exe'
    $null = Run 'C folding build' $gcc ($flags + @(
        'release/extlibs/ical/test/test_utf8_folding.c',$runtime) + $objects + $links + @('-o',$fold))
    $null = Run 'C folding/invalid/OOM' $fold @()
    $null = Run 'C folding JIT' $xl @('run','release/extlibs/ical/test/test_utf8_folding.c')
    $null = Run 'XLang folding JIT' $xl @('run','release/extlibs/ical/test/utf8_folding.xl')

    $generated = Join-Path $work 'generated.c'
    $null = Run 'actual ABI cgen' $xl @('cgen','release/extlibs/ical/test/utf8_folding.xl','--emit',$generated)
    $prototypes = [regex]::Matches([IO.File]::ReadAllText($generated),'(?m)^extern [^\r\n;]*\bxi_(?:parse|build)\([^;\r\n]*\);')
    if ($prototypes.Count -ne 2) {throw 'Both actual ical C prototypes must be checked'}
    $abi = Join-Path $work 'abi.c'
    $abiText = '#include "ical_native.h"' + "`n" + (($prototypes | ForEach-Object Value) -join "`n")
    [IO.File]::WriteAllText($abi,$abiText,[Text.UTF8Encoding]::new($false))
    $null = Run 'two actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $wrong = $abiText -replace '(?m)^extern [^\r\n;]*\bxi_parse\(', 'extern int xi_parse('
    if ($wrong -ceq $abiText) {throw 'Wrong ABI control did not change the declaration'}
    [IO.File]::WriteAllText($abi,$wrong,[Text.UTF8Encoding]::new($false))
    $null = Run 'wrong parse return rejected' $gcc ($flags + @('-fsyntax-only',$abi)) 1

    $expectedFold = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/ical/examples/folded_calendar.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($language in @('C','XLang')) {
        $source = if ($language -eq 'C') {'release/extlibs/ical/examples/folded_calendar_c.c'} else {'release/extlibs/ical/examples/folded_calendar.xl'}
        foreach ($engine in @('GCC','JIT','AOT')) {
            if (($engine -eq 'GCC' -and $language -ne 'C') -or ($SkipAot -and $engine -eq 'AOT')) {continue}
            $target = Join-Path $work ("folded_${language}_${engine}.exe")
            if ($engine -eq 'GCC') {$null = Run 'GCC folded example build' $gcc ($flags + @($source,$runtime) + $objects + $links + @('-o',$target))}
            elseif ($engine -eq 'AOT') {$null = Run "$language folded AOT build" $xl @('build',$source,'-o',$target)}
            $outputFile = Join-Path $work ("folded_${language}_${engine}.ics")
            if ($engine -eq 'JIT') {$output = Run "$language folded JIT" $xl @('run',$source,'--',$outputFile)}
            else {$output = Run "$language folded $engine" $target @($outputFile)}
            if ($output -cne $expectedFold) {throw 'Folded example output differs'}
            $null = Run "$language $engine independent canonical bytes" $python @('-I','release/extlibs/ical/test/check_folded.py',$outputFile)
        }
    }
    if (!$SkipAot) {
        foreach ($source in @('release/extlibs/ical/test/test_utf8_folding.c','release/extlibs/ical/test/utf8_folding.xl','release/extlibs/ical/examples/ical.c')) {
            $target = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            $null = Run "$source AOT build" $xl @('build',$source,'-o',$target)
            $null = Run "$source AOT" $target @()
        }
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "ical input changed: $path"}
    }
    # 原有语言字典创建必须继续作为正例执行；M1 失败不能改成预期成功。
    foreach ($case in @('test/cases/extlib_ical.xl',
        'test/cases/extlib_ical_boundaries.xl',
        'release/extlibs/ical/test/control_characters.xl',
        'release/extlibs/ical/test/allocations.xl',
        'release/extlibs/ical/test/text_identifiers.xl',
        'release/extlibs/ical/test/nested_calendar.xl')) {
        $null = Run $case $xl @('run', $case)
    }
    $null = Run 'XLang limits' $xl @('run', 'release/extlibs/ical/test/limits.xl')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/ical.xl', '--', $xlFile)
    if ($xlOutput -cne $cOutput) { throw 'C/XLang example output differs' }
    $null = Run 'independent wire check' $python @(
        'release/extlibs/ical/test/check_wire.py', $cFile, $xlFile,
        (Join-Path $work 'long.ics'))
    if (-not $SkipAot) {
        $target = Join-Path $work 'aot_no_index.exe'
        $null = Run 'XLang core AOT build' $xl @('build',
            'release/extlibs/ical/test/aot_no_index.xl', '-o', $target)
        $null = Run 'XLang core AOT' $target @()
        $target = Join-Path $work 'ical_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/ical.xl', '-o', $target)
        $aotFile = Join-Path $work 'aot.ics'
        $aotOutput = Run 'XLang example AOT' $target @($aotFile)
        if ($aotOutput -cne $cOutput) { throw 'AOT/C example output differs' }
        $null = Run 'independent AOT wire check' $python @(
            'release/extlibs/ical/test/check_wire.py', $cFile, $aotFile,
            (Join-Path $work 'long.ics'))
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "ical input changed: $path"}
    }
    $passed = $true
    if ($SkipAot) {Write-Output 'ical Windows C/XLang (AOT skipped) gate: PASS'}
    else {Write-Output 'ical Windows full C/XLang/AOT gate: PASS'}
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($work)
    $prefix = $safeRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not ([IO.Path]::GetFileName($resolved)).StartsWith('ical_guard_')) {
        throw "Refusing to remove unexpected test directory: $resolved"
    }
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) {Remove-Item -LiteralPath $resolved -Recurse -Force}
    else {Write-Warning "ical failure artifacts retained: $resolved"}
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('vcard_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/vcard') -Recurse -File | ForEach-Object FullName)
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName)
$inputs += @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h','tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1',
    'tools/xlir_native_process.ps1','test/cases/extlib_vcard.xl',
    'test/cases/extlib_vcard_boundaries.xl','release/examples/extlibs/vcard.xl') |
    ForEach-Object {Join-Path $root $_}
$hashes = @{}
foreach ($path in @($inputs) + @($gcc,$python,$shell)) {$hashes[$path] = (Get-FileHash -LiteralPath $path).Hash}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/vcard/include')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expected = 0) {
    if ($null -eq $arguments) {$arguments = @()}
    if ($program.EndsWith('.ps1')) {
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 @{LC_ALL='C'}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expected;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expected) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[vcard] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n")
}

function VerifyInputs {
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "vcard input changed: $path"}
    }
}

[void][IO.Directory]::CreateDirectory($work)
Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $units = @('vcard_native','vcard_parse','vcard_build')
    $null = Run 'strict manifest syntax' $gcc ($flags + @('-fsyntax-only',
        '-DXRT_MODULE_BUFFER','-DXRT_MODULE_UNICODE','-DXRT_MODULE_VALUE_CONTAINER') +
        @($units | ForEach-Object {"release/extlibs/vcard/$_.c"}))
    $null = Run 'strict fixture syntax' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/vcard/test/test_vcard.c','release/extlibs/vcard/examples/vcard.c',
        'release/extlibs/vcard/test/utf8_folding.c'))
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c','release/extlibs/vcard/test/xrt_runtime.c','-o',$runtime))
    $objects = @($runtime)
    foreach ($unit in $units) {
        $object = Join-Path $work ($unit + '.o')
        $null = Run "$unit compile" $gcc ($flags + @('-c',"release/extlibs/vcard/$unit.c",'-o',$object))
        $objects += $object
    }
    foreach ($name in @('test_vcard','test_utf8_folding')) {
        $target = Join-Path $work ($name + '.exe')
        $null = Run "GCC $name build" $gcc ($flags + @("release/extlibs/vcard/test/$name.c") + $objects + $links + @('-o',$target))
        $argsForTest = if ($name -eq 'test_vcard') {@((Join-Path $work 'long.vcf'),(Join-Path $work 'four.vcf'))} else {@()}
        $null = Run "GCC $name" $target $argsForTest
    }
    $null = Run 'old C boundaries JIT' $runner @('-Package','vcard','-Source','release/extlibs/vcard/test/test_vcard.c')
    $null = Run 'C folding JIT' $xl @('run','release/extlibs/vcard/test/test_utf8_folding.c')
    $null = Run 'XLang folding JIT' $xl @('run','release/extlibs/vcard/test/utf8_folding.xl')

    $generated = Join-Path $work 'generated.c'
    $null = Run 'actual ABI cgen' $xl @('cgen','release/extlibs/vcard/test/utf8_folding.xl','--emit',$generated)
    $prototypes = [regex]::Matches([IO.File]::ReadAllText($generated),'(?m)^extern [^\r\n;]*\bxv_(?:parse|build)\([^;\r\n]*\);')
    if ($prototypes.Count -ne 2) {throw 'Both actual vcard C prototypes must be checked'}
    $abi = Join-Path $work 'abi.c'
    $abiText = '#include "vcard_native.h"' + "`n" + (($prototypes | ForEach-Object Value) -join "`n")
    [IO.File]::WriteAllText($abi,$abiText,[Text.UTF8Encoding]::new($false))
    $null = Run 'two actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $wrong = $abiText -replace '(?m)^extern [^\r\n;]*\bxv_parse\(', 'extern int xv_parse('
    if ($wrong -ceq $abiText) {throw 'Wrong ABI control did not change declaration'}
    [IO.File]::WriteAllText($abi,$wrong,[Text.UTF8Encoding]::new($false))
    $null = Run 'wrong parse return rejected' $gcc ($flags + @('-fsyntax-only',$abi)) 1
    if ($checks[-1].result.stderr -notmatch 'conflicting types') {throw 'Wrong ABI failed for an unrelated reason'}

    $cFile = Join-Path $work 'c.vcf'
    $oldExample = Join-Path $work 'old_example.exe'
    $null = Run 'GCC original C example build' $gcc ($flags + @('release/extlibs/vcard/examples/vcard.c') + $objects + $links + @('-o',$oldExample))
    $cOutput = Run 'GCC original C example' $oldExample @($cFile)
    $directFile = Join-Path $work 'direct.vcf'
    $direct = Run 'original C example direct JIT' $xl @('run','release/extlibs/vcard/examples/vcard.c','--',$directFile)
    if ($direct -cne $cOutput -or (Get-FileHash $directFile).Hash -ne (Get-FileHash $cFile).Hash) {throw 'Original C example JIT/GCC mismatch'}
    $null = Run 'original C GCC failed write' $oldExample @($work) 1
    if ($checks[-1].result.stdout -ne '' -or $checks[-1].result.stderr -ne '') {throw 'Failed C write printed success or unrelated output'}
    $null = Run 'original C JIT failed write' $xl @('run','release/extlibs/vcard/examples/vcard.c','--',$work) 1
    if ($checks[-1].result.stdout -ne '' -or $checks[-1].result.stderr -ne '') {throw 'Failed C JIT write printed success or unrelated output'}

    $expected = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/vcard/examples/folded_contact.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($language in @('C','XLang')) {
        $source = if ($language -eq 'C') {'release/extlibs/vcard/examples/folded_contact_c.c'} else {'release/extlibs/vcard/examples/folded_contact.xl'}
        foreach ($engine in @('GCC','JIT','AOT')) {
            if (($engine -eq 'GCC' -and $language -ne 'C') -or ($SkipAot -and $engine -eq 'AOT')) {continue}
            $target = Join-Path $work ("folded_${language}_${engine}.exe")
            if ($engine -eq 'GCC') {$null = Run 'GCC folded example build' $gcc ($flags + @($source) + $objects + $links + @('-o',$target))}
            elseif ($engine -eq 'AOT') {$null = Run "$language folded AOT build" $xl @('build',$source,'-o',$target)}
            $outputFile = Join-Path $work ("folded_${language}_${engine}.vcf")
            if ($engine -eq 'JIT') {$output = Run "$language folded JIT" $xl @('run',$source,'--',$outputFile)}
            else {$output = Run "$language folded $engine" $target @($outputFile)}
            if ($output -cne $expected) {throw 'Folded example output differs'}
            $null = Run "$language $engine independent canonical bytes" $python @('-I','release/extlibs/vcard/test/check_folded.py',$outputFile)
        }
    }
    if (!$SkipAot) {
        foreach ($source in @('release/extlibs/vcard/test/test_utf8_folding.c','release/extlibs/vcard/test/utf8_folding.xl')) {
            $target = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            $null = Run "$source AOT build" $xl @('build',$source,'-o',$target)
            $null = Run "$source AOT" $target @()
        }
        $target = Join-Path $work 'original_c.exe'
        $null = Run 'original C AOT build' $xl @('build','release/extlibs/vcard/examples/vcard.c','-o',$target)
        $output = Run 'original C AOT' $target @((Join-Path $work 'aot-c.vcf'))
        if ($output -cne $cOutput -or (Get-FileHash (Join-Path $work 'aot-c.vcf')).Hash -ne (Get-FileHash $cFile).Hash) {throw 'Original C AOT/GCC mismatch'}
    }
    VerifyInputs

    # 原字典创建是必须通过的正例；M1 不能改为预期错误或用原生 parse 对象替代。
    foreach ($source in @('test/cases/extlib_vcard.xl','test/cases/extlib_vcard_boundaries.xl',
        'release/extlibs/vcard/test/control_characters.xl','release/extlibs/vcard/test/allocations.xl',
        'release/extlibs/vcard/test/uri_values.xl','release/extlibs/vcard/test/structure.xl',
        'release/extlibs/vcard/test/limits.xl')) {
        $null = Run $source $xl @('run',$source)
    }
    $xlangFile = Join-Path $work 'xlang.vcf'
    $xlangOutput = Run 'original XLang example' $xl @('run','release/examples/extlibs/vcard.xl','--',$xlangFile)
    if ($xlangOutput -cne $cOutput -or (Get-FileHash $xlangFile).Hash -ne (Get-FileHash $cFile).Hash) {throw 'Original C/XLang example mismatch'}
    if (!$SkipAot) {
        $core = Join-Path $work 'core.exe'
        $null = Run 'original XLang core AOT build' $xl @('build','release/extlibs/vcard/test/aot_core.xl','-o',$core)
        $null = Run 'original XLang core AOT' $core @()
        $target = Join-Path $work 'original_xlang.exe'
        $file = Join-Path $work 'aot.vcf'
        $null = Run 'original XLang example AOT build' $xl @('build','release/examples/extlibs/vcard.xl','-o',$target)
        $output = Run 'original XLang example AOT' $target @($file)
        if ($output -cne $cOutput -or (Get-FileHash $file).Hash -ne (Get-FileHash $cFile).Hash) {throw 'Original XLang AOT/C example mismatch'}
    }
    VerifyInputs
    $passed = $true
    if ($SkipAot) {Write-Output 'vcard Windows C/XLang (AOT skipped) gate: PASS'}
    else {Write-Output 'vcard Windows full C/XLang/AOT gate: PASS'}
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        !([IO.Path]::GetFileName($resolved)).StartsWith('vcard_guard_')) {throw 'Unsafe vcard cleanup path'}
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) {Remove-Item -LiteralPath $resolved -Recurse -Force}
    else {Write-Warning "vcard failure artifacts retained: $resolved"}
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('zip_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $root 'release/xl.exe'
$shell = (Get-Process -Id $PID).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$inputs = @('zip','_libzip','_zlib') | ForEach-Object {
    Get-ChildItem (Join-Path $root "release/extlibs/$_") -Recurse -File | ForEach-Object FullName
}
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName)
$inputs += @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
    'tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'release/examples/extlibs/zip.xl','release/examples/extlibs/zip_c.c') | ForEach-Object {Join-Path $root $_}
$hashes = @{}
foreach ($path in @($inputs) + @($gcc,$python,$shell)) {$hashes[$path] = (Get-FileHash -LiteralPath $path).Hash}
$flags = @('-std=c11','-D_FILE_OFFSET_BITS=64','-O1','-Wall','-Wextra',
    '-Ilib/xrt','-Irelease/extlibs/zip/include','-Irelease/extlibs/zip',
    '-Irelease/extlibs/_libzip/third_party/libzip/lib','-Irelease/extlibs/_zlib/third_party/zlib')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$sources = [Collections.Generic.List[string]]::new()
$visited = @{}
function Sources([string]$package) {
    if ($visited.ContainsKey($package)) {return}
    $visited[$package] = $true
    $folder = Join-Path $root "release/extlibs/$package"
    $manifest = [IO.File]::ReadAllText((Join-Path $folder 'xlang.package.json')) | ConvertFrom-Json
    foreach ($dependency in @($manifest.dependencies)) {Sources $dependency.name}
    foreach ($source in @($manifest.artifact.cSources)) {$sources.Add((Join-Path $folder $source))}
}
Sources 'zip'
$cArgs = @($sources.ToArray())
foreach ($dir in @('lib/xrt','release/extlibs/zip/include','release/extlibs/zip',
    'release/extlibs/_libzip/third_party/libzip/lib','release/extlibs/_zlib/third_party/zlib')) {$cArgs += @('--inc-path',$dir)}

function Run([string]$name, [string]$program, [string[]]$arguments,
    [string]$expected = $null, [int]$expectedExit = 0) {
    if ($null -eq $arguments) {$arguments = @()}
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 240 @{LC_ALL='C'}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expectedExit;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    $actual = ($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n")
    if ($PSBoundParameters.ContainsKey('expected') -and $actual -cne $expected) {throw "${name}: unexpected output`n$actual"}
    Write-Host "[zip] ${name}: PASS"
    return $actual
}
function VerifyInputs {
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "ZIP input changed: $path"}
    }
}
function Negative([string]$name, [string]$source, [string]$symbol) {
    $null = Run $name $xl @('check',$source) -expectedExit 1
    $diagnostic = $checks[-1].result.stdout + $checks[-1].result.stderr
    if ($diagnostic -notmatch 'access|private' -or ($symbol -and !$diagnostic.Contains($symbol))) {throw "$name failed for an unrelated reason"}
}

[void][IO.Directory]::CreateDirectory($work)
Push-Location $root
try {
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $null = Run 'strict libzip platform macro' $gcc ($flags + @('-Werror','-fsyntax-only','-DXRT_MODULE_FILE_TEMP',
        'release/extlibs/_libzip/platform/zip_source_file_xrt.c'))
    $null = Run 'strict package and fixtures' $gcc ($flags + @('-Werror','-fsyntax-only',
        'release/extlibs/zip/xrt_zip_source.c','release/extlibs/zip/xrt_zip_archive.c',
        'release/extlibs/zip/xrt_zip_extract.c','release/extlibs/zip/zip_native.c',
        'release/extlibs/zip/test/test_commit_memory.c','release/extlibs/zip/examples/memory_roundtrip_c.c'))
    $objects = @()
    $allSources = @((Join-Path $root 'release/extlibs/zip/test/xrt_runtime.c')) + $sources.ToArray()
    for ($i = 0; $i -lt $allSources.Count; ++$i) {
        $object = Join-Path $work ("unit_$i.o")
        $null = Run "native compile $i" $gcc ($flags + @('-c',$allSources[$i],'-o',$object))
        $objects += $object
    }
    foreach ($name in @('test_zip','test_commit_memory')) {
        $target = Join-Path $work ($name + '.exe')
        $null = Run "GCC $name build" $gcc ($flags + @('-Werror',"release/extlibs/zip/test/$name.c") + $objects + $links + @('-o',$target))
        $null = Run "GCC $name" $target @()
        $null = Run "TCC $name" $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','zip','-Source',"release/extlibs/zip/test/$name.c")
    }
    $null = Run 'Python/C ZIP interop' $python @('-I','release/extlibs/zip/test/interop.py') 'zip Python/C interop: PASS'
    $null = Run 'XLang commit memory' $xl @('run','release/extlibs/zip/test/commit_memory.xl') 'zip commit MemoryError/retry: PASS'
    $null = Run 'XLang text transfer' $xl @('run','release/extlibs/zip/test/text_transfer.xl') ''
    $null = Run 'XLang boundaries' $xl @('run','release/extlibs/zip/test/boundaries.xl') 'zip boundaries: ok'
    Negative 'native member rejection' 'release/extlibs/zip/test/private_native_negative.xl' 'addNative'

    $generated = Join-Path $work 'generated.c'
    $null = Run 'actual ABI cgen' $xl @('cgen','release/extlibs/zip/test/boundaries.xl','--emit',$generated)
    $decls = [regex]::Matches([IO.File]::ReadAllText($generated),'(?m)^extern [^\r\n;]*\bxzip_[a-z_]+\([^;\r\n]*\);')
    $expectedNames = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/zip/src/zip.xl')),'extern "(xzip_[a-z_]+)"') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
    $actualNames = @($decls | ForEach-Object {[regex]::Match($_.Value,'\bxzip_[a-z_]+(?=\()').Value} | Sort-Object -Unique)
    if ($decls.Count -ne 32 -or @(Compare-Object $expectedNames $actualNames).Count) {throw 'ZIP generated ABI coverage incomplete'}
    $body = "#define XRT_MODULE_ALL`n#include `"xrt_zip_source.c`"`n#include `"xrt_zip_archive.c`"`n#include `"xrt_zip_extract.c`"`n#include `"zip_native.c`"`n" + (($decls | ForEach-Object Value) -join "`n")
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,$body,[Text.UTF8Encoding]::new($false))
    $null = Run '32 actual prototypes vs definitions' $gcc ($flags + @('-Werror','-fsyntax-only',$abi))
    $wrong = $body -replace '(?m)^extern [^\r\n;]*\bxzip_text_size\(', 'extern int xzip_text_size('
    if ($wrong -ceq $body) {throw 'ABI negative did not mutate a declaration'}
    [IO.File]::WriteAllText($abi,$wrong,[Text.UTF8Encoding]::new($false))
    $null = Run 'wrong size return rejected' $gcc ($flags + @('-Werror','-fsyntax-only',$abi)) -expectedExit 1
    if ($checks[-1].result.stderr -notmatch 'conflicting types') {throw 'ABI failed for an unrelated reason'}

    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/zip/examples/memory_roundtrip.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $source = 'release/extlibs/zip/examples/memory_roundtrip_c.c'
    $target = Join-Path $work 'memory_gcc.exe'
    $null = Run 'GCC memory example build' $gcc ($flags + @('-Werror',$source) + $objects + $links + @('-o',$target))
    $file = Join-Path $work 'gcc.zip'
    $null = Run 'GCC memory example' $target @($file) $golden
    $null = Run 'GCC independent ZIP wire' $python @('-I','release/extlibs/zip/test/check_memory_wire.py',$file)
    $file = Join-Path $work 'cjit.zip'
    $null = Run 'C memory example JIT' $xl (@('run',$source) + $cArgs + @('--',$file)) $golden
    $null = Run 'C JIT independent ZIP wire' $python @('-I','release/extlibs/zip/test/check_memory_wire.py',$file)
    $file = Join-Path $work 'xljit.zip'
    $null = Run 'XLang memory example JIT' $xl @('run','release/extlibs/zip/examples/memory_roundtrip.xl','--',$file) $golden
    $null = Run 'XL JIT independent ZIP wire' $python @('-I','release/extlibs/zip/test/check_memory_wire.py',$file)
    $null = Run 'original C example' $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','zip','-Source','release/examples/extlibs/zip_c.c') 'zip C/XL example: PASS'
    $null = Run 'original XLang example' $xl @('run','release/examples/extlibs/zip.xl') 'zip C/XL example: PASS'
    if (!$SkipAot) {
        foreach ($source in @('release/extlibs/zip/test/aot_core.xl','release/extlibs/zip/test/text_transfer.xl',
            'release/extlibs/zip/test/boundaries.xl','release/examples/extlibs/zip.xl','release/extlibs/zip/test/commit_memory.xl')) {
            $target = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            $null = Run "$source AOT build" $xl @('build',$source,'-o',$target)
            $null = Run "$source AOT" $target @()
        }
        $target = Join-Path $work 'original_c.exe'
        $null = Run 'original C example AOT build' $xl @('build','release/extlibs/zip/test/c_aot_import.xl','release/examples/extlibs/zip_c.c','-o',$target)
        $null = Run 'original C example AOT' $target @() 'zip C/XL example: PASS'
        $target = Join-Path $work 'commit_c.exe'
        $null = Run 'C commit AOT build' $xl (@('build','release/extlibs/zip/test/test_commit_memory.c') + $cArgs + @('-o',$target))
        $null = Run 'C commit AOT' $target @()
        foreach ($kind in @('c','xl')) {
            $source = if ($kind -eq 'c') {'release/extlibs/zip/examples/memory_roundtrip_c.c'} else {'release/extlibs/zip/examples/memory_roundtrip.xl'}
            $target = Join-Path $work ("memory_$kind.exe")
            $argsForBuild = @('build',$source)
            if ($kind -eq 'c') {$argsForBuild += $cArgs}
            $null = Run "$kind memory AOT build" $xl ($argsForBuild + @('-o',$target))
            $file = Join-Path $work ("$kind.aot.zip")
            $null = Run "$kind memory AOT" $target @($file) $golden
            $null = Run "$kind AOT independent ZIP wire" $python @('-I','release/extlibs/zip/test/check_memory_wire.py',$file)
        }
    }
    VerifyInputs
    # 汇总独立主线边界，所有正例仍要求 0，私有构造器仍必须拒绝。
    $boundaryFailures = [Collections.Generic.List[string]]::new()
    try {
        $null = Run 'global lifecycle JIT' $xl @('run','release/extlibs/zip/test/global_lifecycle.xl') 'zip globals: PASS'
        if (!$SkipAot) {
            $target = Join-Path $work 'global.exe'
            $null = Run 'global lifecycle AOT build' $xl @('build','release/extlibs/zip/test/global_lifecycle.xl','-o',$target)
            $null = Run 'global lifecycle AOT' $target @() 'zip globals: PASS'
        }
    } catch {$boundaryFailures.Add($_.Exception.Message)}
    try {
        $null = Run 'public error descriptions JIT' $xl @('run','release/extlibs/zip/test/error_messages.xl') 'zip described errors: 2'
    } catch {$boundaryFailures.Add($_.Exception.Message)}
    if (!$SkipAot) {
        try {
            $target = Join-Path $work 'error_messages.exe'
            $null = Run 'public error descriptions AOT build' $xl @('build','release/extlibs/zip/test/error_messages.xl','-o',$target)
            $null = Run 'public error descriptions AOT' $target @() 'zip described errors: 2'
        } catch {$boundaryFailures.Add($_.Exception.Message)}
    }
    try {Negative 'private memory constructor must reject' 'release/extlibs/zip/test/private_memory_constructor_negative.xl' ''}
    catch {$boundaryFailures.Add($_.Exception.Message)}
    if ($boundaryFailures.Count) {throw ($boundaryFailures -join "`n")}
    $passed = $true
    Write-Output 'zip Windows complete gate: PASS'
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    VerifyInputs
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        !([IO.Path]::GetFileName($resolved)).StartsWith('zip_guard_')) {throw 'Unsafe ZIP cleanup path'}
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) {Remove-Item -LiteralPath $resolved -Recurse -Force}
    else {Write-Warning "ZIP failure artifacts retained: $resolved"}
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$shell = (Get-Process -Id $PID).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$workParent = Join-Path $root 'build/test/snappy_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$passed = $false
$checks = [Collections.Generic.List[object]]::new()
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror',
    '-Ilib/xrt','-Ilib/xruntime','-Irelease/extlibs/snappy/include')
$links = @('-lws2_32','-ladvapi32','-liphlpapi','-lm')
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/snappy') -Recurse -File | ForEach-Object FullName)
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName)
$inputs += @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
    'test/support/extlib_codec_runtime.c','test/cases/extlib_snappy.xl','test/cases/extlib_snappy_progress.xl',
    'release/examples/extlibs/snappy.xl','release/examples/extlibs/snappy_basic.xl',
    'tools/run_extlib_c.ps1','tools/check_extlib_native_globals.ps1','tools/check_extlib_native_globals.sh',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1') | ForEach-Object {Join-Path $root $_}
$hashes = @{}
foreach ($path in $inputs + @($gcc,$python,$shell)) {$hashes[$path] = (Get-FileHash -LiteralPath $path).Hash}
function VerifyInputs {
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "Snappy input changed: $path"}
    }
}
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
    $text = ($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n")
    if ($PSBoundParameters.ContainsKey('expected') -and $text -cne $expected) {throw "${name}: unexpected output`n$text"}
    Write-Host "[snappy] ${name}: PASS"
    return $text
}
function Run-C([string]$name, [string]$source) {
    return Run $name $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File',
        'tools/run_extlib_c.ps1','-Package','snappy','-Source',$source)
}

Push-Location $root
try {
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    foreach ($order in @(@(),@('-DSNAPPY_CORE_FIRST'),@('-DSNAPPY_NATIVE_FIRST'),@('-DSNAPPY_XRT_FIRST'))) {
        $null = Run 'GCC native header order' $gcc ($flags + $order + @('-fsyntax-only','release/extlibs/snappy/test/native_header.c'))
    }
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'GCC runtime build' $gcc ($flags + @('-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $objects = @($runtime)
    foreach ($source in @('xrt_snappy.c','snappy_native.c','third_party/xrt_snappy.c')) {
        $object = Join-Path $work (($source -replace '[/\\.]','_') + '.o')
        $null = Run "GCC native build $source" $gcc ($flags + @('-c',"release/extlibs/snappy/$source",'-o',$object))
        $objects += $object
    }
    foreach ($name in @('test_snappy','test_allocations','empty_chunks_c')) {
        $binary = Join-Path $work ($name + '.exe')
        $null = Run "GCC $name build" $gcc ($flags + @('-DNDEBUG','-DSNAPPY_MEMORY_GUARD',
            "release/extlibs/snappy/test/$name.c") + $objects + $links + @('-o',$binary))
        $null = Run "GCC $name" $binary @()
    }
    $null = Run-C 'TCC C boundaries' 'release/extlibs/snappy/test/test_snappy.c'
    $null = Run 'TCC empty chunks' $xl @('run','release/extlibs/snappy/test/empty_chunks_c.c')
    foreach ($kind in @('compressor','decompressor')) {
        $null = Run "private $kind must reject" $xl @('check',"release/extlibs/snappy/test/private_${kind}_negative.xl") -expectedExit 1
        $diagnostic = $checks[-1].result.stdout + $checks[-1].result.stderr
        if ($diagnostic -notmatch 'updateNative' -or $diagnostic -notmatch 'access') {throw 'Unrelated private access failure'}
    }
    foreach ($source in @('test/cases/extlib_snappy.xl','test/cases/extlib_snappy_progress.xl',
        'release/extlibs/snappy/test/file_ops.xl','release/extlibs/snappy/test/allocations.xl',
        'release/extlibs/snappy/test/text_transfer.xl','release/extlibs/snappy/test/empty_chunks.xl',
        'release/extlibs/snappy/test/abi_surface.xl',
        'release/examples/extlibs/snappy.xl')) {
        $null = Run "XLang $source" $xl @('run',$source)
    }
    $generated = Join-Path $work 'generated.c'
    $null = Run 'actual ABI cgen' $xl @('cgen','release/extlibs/snappy/test/abi_surface.xl','--emit',$generated)
    $null = Run 'make actual ABI probes' $python @('-I','release/extlibs/snappy/test/make_abi_probe.py',$generated,$work)
    $null = Run 'actual prototypes vs public C header' $gcc ($flags + @('-fsyntax-only',(Join-Path $work 'abi.c')))
    $null = Run 'wrong version return must reject' $gcc ($flags + @('-fsyntax-only',(Join-Path $work 'abi_wrong.c'))) -expectedExit 1
    if ($checks[-1].result.stderr -notmatch 'conflicting types') {throw 'Unrelated ABI rejection'}

    $cOutput = Run-C 'C basic example' 'release/extlibs/snappy/examples/basic.c'
    if ($cOutput -cne 'hello snappy') {throw 'Unexpected basic C output'}
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/snappy/examples/framed_records.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($name in @('basic','framed_records_c')) {
        $source = "release/extlibs/snappy/examples/$name.c"
        $binary = Join-Path $work ($name + '.exe')
        $expected = if ($name -eq 'basic') {$cOutput} else {$golden}
        $null = Run "GCC $name example build" $gcc ($flags + @($source) + $objects + $links + @('-o',$binary))
        $null = Run "GCC $name example" $binary @() $expected
        $null = Run "TCC $name example" $xl @('run',$source) $expected
    }
    $null = Run 'XLang basic example' $xl @('run','release/examples/extlibs/snappy_basic.xl') $cOutput
    $null = Run 'XLang framed records' $xl @('run','release/extlibs/snappy/examples/framed_records.xl') $golden
    if (!$SkipAot) {
        foreach ($source in @('test/cases/extlib_snappy.xl','test/cases/extlib_snappy_progress.xl',
            'release/extlibs/snappy/test/file_ops.xl','release/extlibs/snappy/test/allocations.xl',
            'release/extlibs/snappy/test/text_transfer.xl','release/extlibs/snappy/test/empty_chunks.xl',
            'release/extlibs/snappy/test/empty_chunks_c.c','release/extlibs/snappy/test/abi_surface.xl',
            'release/examples/extlibs/snappy_basic.xl',
            'release/examples/extlibs/snappy.xl','release/extlibs/snappy/examples/basic.c',
            'release/extlibs/snappy/examples/framed_records_c.c','release/extlibs/snappy/examples/framed_records.xl')) {
            $binary = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            $null = Run "$source AOT build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match '/framed_records') {
                if ($output -cne $golden) {throw 'AOT framed records output differs'}
            } elseif ($source -match '/basic\.c$|/snappy_basic\.xl$') {
                if ($output -cne $cOutput) {throw 'AOT basic output differs'}
            }
        }
    }
    # 原顶层正例仍要求自然退役成功，不把 M2 改成预期退出 1。
    $lifecycle = @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/check_extlib_native_globals.ps1',
        '-Root',$root,'-Package','snappy')
    if ($SkipAot) {$lifecycle += '-SkipAot'}
    $null = Run 'top-level lifecycle' $shell $lifecycle
    $passed = $true
    Write-Output 'snappy Windows package gate: PASS'
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    VerifyInputs
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 9 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    $safeParent = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($safeParent,[StringComparison]::OrdinalIgnoreCase) -or
        !([IO.Path]::GetFileName($resolved)).StartsWith('run_')) {throw 'Unsafe Snappy cleanup path'}
    if ($passed) {Remove-Item -LiteralPath $resolved -Recurse -Force}
    else {Write-Warning "snappy failure artifacts: $resolved"}
}

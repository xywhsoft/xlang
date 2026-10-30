param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test/cbor_guard'
$work = Join-Path $workRoot ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$compiler = if ($env:CC) { $env:CC } else { 'gcc' }
$gccCommand = Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($gccCommand) { $gccCommand.Source } else { $compiler }
$interpreter = if ($env:PYTHON) { $env:PYTHON } else { 'python' }
$pythonCommand = Get-Command $interpreter -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$python = if ($pythonCommand) { $pythonCommand.Source } else { $interpreter }
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'; PYTHONDONTWRITEBYTECODE='1'}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/cbor')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
function Normalize([string]$value) { $value.Replace("`r`n","`n").TrimEnd("`n") }
$golden = Normalize ([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../examples/basic.expected.txt')))
$wireGolden = Normalize ([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../examples/validate_wire.expected.txt')))

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/cbor') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'test/cases/extlib_cbor.xl','release/examples/extlibs/cbor.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    if ($gccCommand) { $gcc }
    if ($pythonCommand) { $python }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[cbor] ${name}: PASS"
    return Normalize $result.stdout
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'Python version' $python @('--version')
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/cbor/cbor_native.h'))
    $manifest = Get-Content -LiteralPath release/extlibs/cbor/xlang.package.json -Raw | ConvertFrom-Json
    $modules = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest C syntax' $gcc ($flags + $modules + @('-fsyntax-only','release/extlibs/cbor/cbor_native.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/cbor/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/cbor/cbor_native.c','-o',$native))
    foreach ($name in @('test_cbor','test_boundaries','test_allocations','wellformed_c')) {
        $source = "release/extlibs/cbor/test/$name.c"
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @('-DNDEBUG',$source,$native,$runtime) + $links + @('-o',$binary))
        $output = Run $name $binary @()
        if ($name -eq 'wellformed_c' -and $output -cne 'cbor well-formedness: 2591 checks, 0 failures') { throw 'Incomplete wire checks' }
    }
    $half = Join-Path $work 'half.tsv'
    $records = Run 'all binary16 records' (Join-Path $work 'wellformed_c.exe') @('--half-records')
    [IO.File]::WriteAllText($half,$records+"`n",$utf8)
    $null = Run 'Python binary16 oracle' $python @('release/extlibs/cbor/test/check_half.py',$half)
    $output = Run 'wire checks direct C' $xl @('run','release/extlibs/cbor/test/wellformed_c.c')
    if ($output -cne 'cbor well-formedness: 2591 checks, 0 failures') { throw 'TCC wire checks differ' }
    $sources = @('release/extlibs/cbor/test/aot_core.xl','release/extlibs/cbor/test/wellformed.xl')
    foreach ($source in $sources) { $null = Run "$source JIT" $xl @('run',$source) }

    foreach ($pair in @(@('release/extlibs/cbor/examples/cbor_c.c','',$golden),
        @('release/extlibs/cbor/examples/validate_wire_c.c','release/extlibs/cbor/examples/validate_wire.xl',$wireGolden))) {
        $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $objects = @($runtime)
        if ($pair[0] -match '/validate_wire_c.c$') { $objects += $native }
        $null = Run "$($pair[0]) compile" $gcc ($flags + @('-DNDEBUG',$pair[0]) + $objects + $links + @('-o',$binary))
        $c = Run "$($pair[0]) GCC" $binary @()
        $direct = Run "$($pair[0]) direct" $xl @('run',$pair[0])
        if ($c -cne $pair[2] -or $direct -cne $pair[2]) { throw 'C example golden differs' }
        if ($pair[1]) {
            $language = Run "$($pair[1]) JIT" $xl @('run',$pair[1])
            if ($language -cne $pair[2]) { throw 'Bilingual wire example differs' }
        }
    }
    $probe = Join-Path $work 'probe.c'
    $null = Run 'complete API cgen' $xl @('cgen','release/extlibs/cbor/examples/validate_wire.xl','--emit',$probe)
    $text = [IO.File]::ReadAllText($probe)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxcbor_[a-z_]+\(' } | Sort-Object -Unique)
    $expected = @('xcbor_decode_abi','xcbor_decode_bytes_abi','xcbor_encode_abi','xcbor_encode_bytes_abi',
        'xcbor_error_message','xcbor_failed','xcbor_from_hex_abi','xcbor_to_hex_abi')
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxcbor_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 8 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete generated ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"cbor_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'eight actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    if (-not $SkipAot) {
        foreach ($source in ($sources + @('release/extlibs/cbor/test/wellformed_c.c','release/extlibs/cbor/examples/cbor_c.c',
            'release/extlibs/cbor/examples/validate_wire.xl','release/extlibs/cbor/examples/validate_wire_c.c'))) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match 'validate_wire' -and $output -cne $wireGolden) { throw 'Wire AOT golden differs' }
            if ($source -match '/examples/cbor_c.c$' -and $output -cne $golden) { throw 'C example AOT golden differs' }
            if ($source -match '/wellformed_c.c$' -and $output -cne 'cbor well-formedness: 2591 checks, 0 failures') { throw 'AOT wire checks differ' }
        }
    }
    # 完整旧合同仍必须通过；M1不能被标记为expected failure或搬走容器掩盖。
    $full = @('release/extlibs/cbor/test/boundaries.xl','release/extlibs/cbor/test/allocations.xl',
        'release/extlibs/cbor/examples/cbor.xl','release/examples/extlibs/cbor.xl','test/cases/extlib_cbor.xl')
    foreach ($source in $full) {
        $output = Run "$source complete JIT" $xl @('run',$source)
        if ($source -eq 'release/extlibs/cbor/examples/cbor.xl' -and $output -cne $golden) { throw 'Full bilingual example differs' }
        if (-not $SkipAot) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source complete build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source complete AOT" $binary @()
            if ($source -eq 'release/extlibs/cbor/examples/cbor.xl' -and $output -cne $golden) { throw 'Full AOT example differs' }
        }
    }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[cbor] report: $work.json"
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'CBOR work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'CBOR inputs changed during gate' }
}

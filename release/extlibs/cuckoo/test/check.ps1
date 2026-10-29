param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test/cuckoo_guard'
$work = Join-Path $workRoot ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$compiler = if ($env:CC) { $env:CC } else { 'gcc' }
$gccCommand = Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($gccCommand) { $gccCommand.Source } else { $compiler }
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/cuckoo','-Irelease/extlibs/cuckoo/include')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
$golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/cuckoo/examples/cuckoo.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/cuckoo') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($item in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'test/cases/extlib_cuckoo.xl','test/cases/extlib_cuckoo_boundaries.xl',
        'release/examples/extlibs/cuckoo.xl','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $item }
    if ($gccCommand) { $gcc }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Observe([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    Write-Host "[cuckoo] ${name}: $($result.status) exit=$($result.exitCode)"
    return $result
}
function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Observe $name $program $arguments
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/cuckoo/include/cuckoo_native.h'))
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_HASH64','-fsyntax-only','release/extlibs/cuckoo/cuckoo_native.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/cuckoo/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/cuckoo/cuckoo_native.c','-o',$native))
    $binary = Join-Path $work 'c_test.exe'
    $null = Run 'C boundaries compile' $gcc ($flags + @('-DNDEBUG','release/extlibs/cuckoo/test/test_cuckoo.c',$native,$runtime) + $links + @('-o',$binary))
    $null = Run 'C collisions and failure rollback' $binary @()
    $ordinary = @('test/cases/extlib_cuckoo.xl','test/cases/extlib_cuckoo_boundaries.xl',
        'release/extlibs/cuckoo/test/high_load.xl','release/extlibs/cuckoo/test/allocations.xl',
        'release/extlibs/cuckoo/test/aot_core.xl','release/extlibs/cuckoo/test/local_lifetime_control.xl')
    foreach ($source in ($ordinary + @('release/extlibs/cuckoo/test/global_valid_only.xl','release/extlibs/cuckoo/test/global_lifetime.xl'))) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    $negative = Observe 'private raw add rejected' $xl @('check','release/extlibs/cuckoo/test/private_native_negative.xl')
    if ($negative.status -ne 'fail' -or $negative.exitCode -eq 0 -or $negative.stderr -notmatch 'addNative' -or
        $negative.stderr -notmatch 'access') { throw 'Missing private-access diagnostic' }
    $module = Join-Path $work 'module.c'
    $null = Run 'full API cgen' $xl @('cgen','release/examples/extlibs/cuckoo.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxcuckoo_[a-z_]+\(' } | Sort-Object -Unique)
    $expected = @('xcuckoo_add','xcuckoo_buckets','xcuckoo_clear','xcuckoo_contains','xcuckoo_count','xcuckoo_create','xcuckoo_destroy','xcuckoo_remove')
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxcuckoo_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 8 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete generated C ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/cuckoo_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'eight actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('-DNDEBUG','release/extlibs/cuckoo/examples/cuckoo.c',$native,$runtime) + $links + @('-o',$example))
    $c = Run 'C example' $example @()
    $direct = Run 'C example direct' $xl @('run','release/extlibs/cuckoo/examples/cuckoo.c')
    $public = Run 'XLang example' $xl @('run','release/examples/extlibs/cuckoo.xl')
    if ($c -cne $golden -or $direct -cne $golden -or $public -cne $golden) { throw 'C/XLang golden output differs' }
    if (-not $SkipAot) {
        foreach ($source in ($ordinary + @('release/extlibs/cuckoo/examples/cuckoo.c','release/examples/extlibs/cuckoo.xl'))) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -in @('release/extlibs/cuckoo/examples/cuckoo.c','release/examples/extlibs/cuckoo.xl') -and $output -cne $golden) {
                throw 'AOT golden output differs'
            }
        }
        $failed = 0
        foreach ($name in @('global_valid_only','global_lifetime')) {
            $source = "release/extlibs/cuckoo/test/$name.xl"
            $null = Run "$name cgen" $xl @('cgen',$source,'--emit',(Join-Path $work ($name+'.c')))
            $binary = Join-Path $work ($name+'.exe')
            $null = Run "$name build" $xl @('build',$source,'-o',$binary)
            $result = Observe "$name AOT positive" $binary @()
            if ($result.status -ne 'pass') { $failed += 1 }
        }
        if ($failed) { throw "$failed valid global lifetimes failed; retain artifacts and report upstream" }
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
    Write-Host "[cuckoo] report: $work.json"
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Cuckoo work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Cuckoo inputs changed during gate' }
}

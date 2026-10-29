param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$parent = Join-Path $root 'build/test/skiplist_guard'
$work = Join-Path $parent ('run_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$xl = Join-Path $root 'release/xl.exe'
$compiler = Get-Command gcc -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($compiler) { $compiler.Source } else { 'gcc' }
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$outcome = 'failed'
$failure = ''
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/skiplist','-Irelease/extlibs/skiplist/include')
$environment = @{XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/skiplist') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
        'lib/xruntime/xruntime.h','test/cases/extlib_skiplist.xl','release/examples/extlibs/skiplist.xl',
        'release/examples/extlibs/skiplist_c.c','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {
        Join-Path $root $path
    }
    if ($compiler) { $gcc }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expectedExit = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expectedExit;result=$result})
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "${name}: $($result.status) exit=$($result.exitCode), expected=$expectedExit`n$($result.stderr)`n$($result.stdout)"
    }
    Write-Host "[skiplist] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

function Golden([string]$output, [string]$path) {
    if ($output -cne [IO.File]::ReadAllText((Join-Path $root $path)).Replace("`r`n","`n").TrimEnd("`n")) {
        throw "SkipList output differs: $path"
    }
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $manifest = Get-Content -LiteralPath 'release/extlibs/skiplist/xlang.package.json' -Raw | ConvertFrom-Json
    $modules = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest native syntax' $gcc ($flags + $modules + @('-fsyntax-only','release/extlibs/skiplist/skiplist_native.c'))
    $null = Run 'public ABI contract' $gcc ($flags + @('-fsyntax-only','release/extlibs/skiplist/test/abi_contract.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c','release/extlibs/skiplist/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/skiplist/skiplist_native.c','-o',$native))
    foreach ($name in @('test_skiplist','entropy_failure','snapshot_failures','example_failure')) {
        $binary = Join-Path $work ($name+'.exe')
        $objects = @($runtime)
        if ($name -eq 'test_skiplist') { $objects += $native }
        $null = Run "$name compile" $gcc ($flags + @("release/extlibs/skiplist/test/$name.c") + $objects + @('-lm','-o',$binary))
        $null = Run $name $binary @()
    }
    foreach ($name in @('snapshot_failures','example_failure')) {
        $null = Run "$name direct C" $xl @('run',"release/extlibs/skiplist/test/$name.c")
    }
    foreach ($source in @('test/cases/extlib_skiplist.xl','release/extlibs/skiplist/test/allocations.xl','release/extlibs/skiplist/test/snapshots.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    $null = Run 'private native entry' $xl @('check','release/extlibs/skiplist/test/private_native.xl') 1
    $negative = $checks[$checks.Count-1].result
    $diagnostic = $negative.stdout + $negative.stderr
    if (!$diagnostic.Contains('setNative') -or !$diagnostic.Contains('access')) { throw 'Expected private-access diagnostic' }

    # 完整调用夹具覆盖十个真实外部原型，再交正式 C 头检查宽度和返回类型。
    $module = Join-Path $work 'module.c'
    $null = Run 'whole binding cgen' $xl @('cgen','release/extlibs/skiplist/examples/binary_snapshots.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxskiplist_[a-z_]+\(' } | Sort-Object -Unique)
    $sourceText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/skiplist/src/skiplist.xl'))
    $expected = @([regex]::Matches($sourceText,'extern "(xskiplist_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxskiplist_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 10 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete SkipList generated ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/skiplist_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Run 'ten actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    foreach ($pair in @(@('release/examples/extlibs/skiplist_c.c','release/examples/extlibs/skiplist.xl','release/extlibs/skiplist/test/skiplist.expected.txt'),
        @('release/extlibs/skiplist/examples/binary_snapshots_c.c','release/extlibs/skiplist/examples/binary_snapshots.xl','release/extlibs/skiplist/examples/binary_snapshots.expected.txt'))) {
        $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $null = Run "$($pair[0]) compile" $gcc ($flags + @($pair[0],$runtime,'-lm','-o',$binary))
        Golden (Run "$($pair[0]) GCC" $binary @()) $pair[2]
        Golden (Run "$($pair[0]) direct" $xl @('run',$pair[0])) $pair[2]
        Golden (Run "$($pair[1]) JIT" $xl @('run',$pair[1])) $pair[2]
    }
    if (!$SkipAot) {
        foreach ($source in @('release/extlibs/skiplist/test/aot_core.xl','test/cases/extlib_skiplist.xl',
            'release/extlibs/skiplist/test/snapshots.xl','release/extlibs/skiplist/test/snapshot_failures.c',
            'release/extlibs/skiplist/test/example_failure.c','release/examples/extlibs/skiplist.xl','release/examples/extlibs/skiplist_c.c',
            'release/extlibs/skiplist/examples/binary_snapshots.xl','release/extlibs/skiplist/examples/binary_snapshots_c.c')) {
            $binary = Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match '/binary_snapshots') { Golden $output 'release/extlibs/skiplist/examples/binary_snapshots.expected.txt' }
            elseif ($source -match '/examples/extlibs/skiplist') { Golden $output 'release/extlibs/skiplist/test/skiplist.expected.txt' }
        }
        # 原顶层正例不改为局部或预期失败，退出成功仍是发布要求。
        $binary = Join-Path $work 'original_allocations.exe'
        $null = Run 'original global allocations build' $xl @('build','release/extlibs/skiplist/test/allocations.xl','-o',$binary)
        $null = Run 'original global allocations AOT' $binary @()
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
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[skiplist] report: $work.json"
    $base = [IO.Path]::GetFullPath($parent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'SkipList work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $target -Recurse -Force }
    if ($changed.Count) { throw 'SkipList inputs changed during gate' }
}

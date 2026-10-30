param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$parent = Join-Path $root 'build/test/rtree_guard'
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
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/rtree')
$environment = @{XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/rtree') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
        'lib/xruntime/xruntime.h','test/cases/extlib_rtree.xl','release/examples/extlibs/rtree.xl',
        'release/examples/extlibs/rtree_c.c','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {
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
    Write-Host "[rtree] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

function Golden([string]$output, [string]$path) {
    if ($output -cne [IO.File]::ReadAllText((Join-Path $root $path)).Replace("`r`n","`n").TrimEnd("`n")) {
        throw "R-tree output differs: $path"
    }
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $manifest = Get-Content -LiteralPath 'release/extlibs/rtree/xlang.package.json' -Raw | ConvertFrom-Json
    $modules = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest native syntax' $gcc ($flags + $modules + @('-fsyntax-only','release/extlibs/rtree/rtree_native.c'))
    $null = Run 'public ABI contract' $gcc ($flags + @('-fsyntax-only','release/extlibs/rtree/test/abi_contract.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c','release/extlibs/rtree/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/rtree/rtree_native.c','-o',$native))
    foreach ($name in @('test_rtree','split_seeds','cascade_rollback')) {
        $binary = Join-Path $work ($name+'.exe')
        $objects = @($runtime)
        if ($name -eq 'test_rtree') { $objects += $native }
        $null = Run "$name compile" $gcc ($flags + @("release/extlibs/rtree/test/$name.c") + $objects + @('-lm','-o',$binary))
        $null = Run $name $binary @()
    }
    $null = Run 'cascade direct C' $xl @('run','release/extlibs/rtree/test/cascade_rollback.c')
    foreach ($source in @('test/cases/extlib_rtree.xl','release/extlibs/rtree/test/boundaries.xl','release/extlibs/rtree/test/allocations.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    $null = Run 'private native entry' $xl @('check','release/extlibs/rtree/test/private_native_negative.xl') 1
    $negative = $checks[$checks.Count-1].result
    $diagnostic = $negative.stdout + $negative.stderr
    if (!$diagnostic.Contains('searchNative') -or !$diagnostic.Contains('access')) { throw 'Expected private-access diagnostic' }

    # 从真实使用路径提取全部八个声明，再用正式头检查返回值和参数类型。
    $module = Join-Path $work 'module.c'
    $null = Run 'whole binding cgen' $xl @('cgen','release/extlibs/rtree/examples/scene.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxrtree_[a-z_]+\(' } | Sort-Object -Unique)
    $sourceText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/rtree/src/rtree.xl'))
    $expected = @([regex]::Matches($sourceText,'extern "(xrtree_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxrtree_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 8 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete R-tree generated ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#define XRT_MODULE_VALUE_CONTAINER`n#include `"xrt.h`"`n#include `"include/rtree_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Run 'eight actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    foreach ($pair in @(@('release/examples/extlibs/rtree_c.c','release/examples/extlibs/rtree.xl','release/extlibs/rtree/test/rtree.expected.txt'),
        @('release/extlibs/rtree/examples/scene_c.c','release/extlibs/rtree/examples/scene.xl','release/extlibs/rtree/examples/scene.expected.txt'))) {
        $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $null = Run "$($pair[0]) compile" $gcc ($flags + @($pair[0],$runtime,'-lm','-o',$binary))
        Golden (Run "$($pair[0]) GCC" $binary @()) $pair[2]
        Golden (Run "$($pair[0]) direct" $xl @('run',$pair[0])) $pair[2]
        Golden (Run "$($pair[1]) JIT" $xl @('run',$pair[1])) $pair[2]
    }
    if (!$SkipAot) {
        foreach ($source in @('test/cases/extlib_rtree.xl','release/extlibs/rtree/test/boundaries.xl',
            'release/extlibs/rtree/test/cascade_rollback.c','release/examples/extlibs/rtree.xl','release/examples/extlibs/rtree_c.c',
            'release/extlibs/rtree/examples/scene.xl','release/extlibs/rtree/examples/scene_c.c')) {
            $binary = Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match '/scene') { Golden $output 'release/extlibs/rtree/examples/scene.expected.txt' }
            elseif ($source -match '/examples/extlibs/rtree') { Golden $output 'release/extlibs/rtree/test/rtree.expected.txt' }
        }
        # 原顶层对象正例仍须退出成功；不改成局部对象，也不将错误当成通过。
        $binary = Join-Path $work 'original_allocations.exe'
        $null = Run 'original global allocations build' $xl @('build','release/extlibs/rtree/test/allocations.xl','-o',$binary)
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
    Write-Host "[rtree] report: $work.json"
    $base = [IO.Path]::GetFullPath($parent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'R-tree work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $target -Recurse -Force }
    if ($changed.Count) { throw 'R-tree inputs changed during gate' }
}

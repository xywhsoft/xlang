param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$parent = Join-Path $root 'build/test/hyperloglog_guard'
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
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/hyperloglog','-Irelease/extlibs/hyperloglog/include')
$environment = @{XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/hyperloglog') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'test/cases/extlib_hyperloglog.xl','release/examples/extlibs/hyperloglog.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
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
    Write-Host "[hyperloglog] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

function Golden([string]$output, [string]$path) {
    if ($output -cne [IO.File]::ReadAllText((Join-Path $root $path)).Replace("`r`n","`n").TrimEnd("`n")) {
        throw "HyperLogLog output differs: $path"
    }
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $manifest = Get-Content -LiteralPath 'release/extlibs/hyperloglog/xlang.package.json' -Raw | ConvertFrom-Json
    $modules = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest native syntax' $gcc ($flags + $modules + @('-fsyntax-only','release/extlibs/hyperloglog/hyperloglog_native.c'))
    $null = Run 'public ABI contract' $gcc ($flags + @('-fsyntax-only','release/extlibs/hyperloglog/test/abi_contract.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c','release/extlibs/hyperloglog/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/hyperloglog/hyperloglog_native.c','-o',$native))
    foreach ($name in @('test_hyperloglog','test_threads','register_vectors')) {
        $binary = Join-Path $work ($name+'.exe')
        $objects = @($runtime)
        if ($name -ne 'register_vectors') { $objects += $native }
        $null = Run "$name compile" $gcc ($flags + @("release/extlibs/hyperloglog/test/$name.c") + $objects + @('-lm','-o',$binary))
        $null = Run $name $binary @()
    }
    $null = Run 'controlled vectors direct C' $xl @('run','release/extlibs/hyperloglog/test/register_vectors.c')
    foreach ($source in @('test/cases/extlib_hyperloglog.xl','release/extlibs/hyperloglog/test/boundaries.xl',
        'release/extlibs/hyperloglog/test/named_type.xl','release/extlibs/hyperloglog/test/allocations.xl',
        'release/extlibs/hyperloglog/test/zero_allocations.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    foreach ($method in @('add','merge')) {
        $null = Run "private $method" $xl @('check',"release/extlibs/hyperloglog/test/private_${method}_negative.xl") 1
        $negative = $checks[$checks.Count-1].result
        $diagnostic = $negative.stdout + $negative.stderr
        if (!$diagnostic.Contains("${method}Native") -or !$diagnostic.Contains('access')) { throw 'Expected private-access diagnostic' }
    }

    # 由完整真实调用提取八个原型，与正式头交叉检查全部参数/返回类型。
    $module = Join-Path $work 'module.c'
    $null = Run 'whole binding cgen' $xl @('cgen','release/extlibs/hyperloglog/test/zero_allocations.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxhll_[a-z_]+\(' } | Sort-Object -Unique)
    $sourceText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/hyperloglog/src/hyperloglog.xl'))
    $expected = @([regex]::Matches($sourceText,'extern "(xhll_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxhll_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 8 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete HLL generated ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/hyperloglog_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Run 'eight actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    foreach ($pair in @(@('release/extlibs/hyperloglog/examples/hyperloglog_c.c','release/examples/extlibs/hyperloglog.xl','release/extlibs/hyperloglog/test/hyperloglog.expected.txt'),
        @('release/extlibs/hyperloglog/examples/shard_union_c.c','release/extlibs/hyperloglog/examples/shard_union.xl','release/extlibs/hyperloglog/examples/shard_union.expected.txt'))) {
        $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $null = Run "$($pair[0]) compile" $gcc ($flags + @($pair[0],$runtime,$native,'-lm','-o',$binary))
        Golden (Run "$($pair[0]) GCC" $binary @()) $pair[2]
        Golden (Run "$($pair[0]) direct" $xl @('run',$pair[0])) $pair[2]
        Golden (Run "$($pair[1]) JIT" $xl @('run',$pair[1])) $pair[2]
    }
    if (!$SkipAot) {
        foreach ($source in @('test/cases/extlib_hyperloglog.xl','release/extlibs/hyperloglog/test/zero_allocations.xl',
            'release/extlibs/hyperloglog/test/register_vectors.c','release/examples/extlibs/hyperloglog.xl',
            'release/extlibs/hyperloglog/examples/hyperloglog_c.c','release/extlibs/hyperloglog/examples/shard_union.xl',
            'release/extlibs/hyperloglog/examples/shard_union_c.c')) {
            $binary = Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match '/shard_union') { Golden $output 'release/extlibs/hyperloglog/examples/shard_union.expected.txt' }
            elseif ($source -in @('release/examples/extlibs/hyperloglog.xl','release/extlibs/hyperloglog/examples/hyperloglog_c.c')) {
                Golden $output 'release/extlibs/hyperloglog/test/hyperloglog.expected.txt'
            }
        }
        # 独立执行全部原顶层正例，聚合失败但每一项仍要求退出0。
        $globalFailures = @()
        foreach ($name in @('boundaries','named_type','allocations')) {
            try {
                $binary = Join-Path $work ($name+'.exe')
                $null = Run "original $name build" $xl @('build',"release/extlibs/hyperloglog/test/$name.xl",'-o',$binary)
                $null = Run "original $name AOT" $binary @()
            } catch { $globalFailures += $_.Exception.Message }
        }
        if ($globalFailures.Count) { throw ($globalFailures -join "`n") }
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
    Write-Host "[hyperloglog] report: $work.json"
    $base = [IO.Path]::GetFullPath($parent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'HLL work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $target -Recurse -Force }
    if ($changed.Count) { throw 'HLL inputs changed during gate' }
}

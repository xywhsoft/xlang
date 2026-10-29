param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/lru_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$outcome = 'failed'
$failure = ''
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/lru')
$environment = @{XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/lru') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
        'lib/xruntime/xruntime.h','test/cases/extlib_lru.xl','release/examples/extlibs/lru.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expectedExit = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expectedExit;result=$result})
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "${name}: $($result.status) exit=$($result.exitCode), expected=$expectedExit`n$($result.stderr)`n$($result.stdout)"
    }
    Write-Host "[lru] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public C header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/lru/include/lru_native.h'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $native = Join-Path $work 'lru_native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/lru/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/lru/lru_native.c','-o',$native))
    foreach ($name in @('test_lru','test_threads','input_budgets_c')) {
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @("release/extlibs/lru/test/$name.c",$native,$runtime,'-lm','-o',$binary))
        $null = Run $name $binary @()
    }
    $null = Run 'C budgets direct' $xl @('run','release/extlibs/lru/test/input_budgets_c.c')
    foreach ($source in @('test/cases/extlib_lru.xl','release/extlibs/lru/test/boundaries.xl',
        'release/extlibs/lru/test/allocations.xl','release/extlibs/lru/test/input_budgets.xl',
        'release/extlibs/lru/test/input_lifetime.xl','release/extlibs/lru/test/owned_probe.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    $null = Run 'private native entry' $xl @('check','release/extlibs/lru/test/private_native.xl') 1
    $negative = $checks[$checks.Count-1].result
    $diagnostic = $negative.stdout + $negative.stderr
    if (!$diagnostic.Contains('putNative') -or !$diagnostic.Contains('access')) { throw 'Expected private-access diagnostic' }

    # 真实 cgen 必须覆盖全部原生声明，再由 C 编译器核对类型而非只比名字。
    $module = Join-Path $work 'module.c'
    $null = Run 'whole binding cgen' $xl @('cgen','release/extlibs/lru/test/owned_probe.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxlru_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $sourceText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/lru/src/lru.xl'))
    $expected = @([regex]::Matches($sourceText,'extern "(xlru_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxlru_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 11 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete LRU generated ABI' }
    if ($text -match '=\s*xlir_string_copy\s*\(') { throw 'LRU validation or binding return still copies payload text' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/lru_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Run 'eleven actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    foreach ($pair in @(@('release/extlibs/lru/examples/lru_c.c','release/examples/extlibs/lru.xl','release/extlibs/lru/examples/basic.expected.txt'),
        @('release/extlibs/lru/examples/snapshots_c.c','release/extlibs/lru/examples/snapshots.xl','release/extlibs/lru/examples/snapshots.expected.txt'))) {
        $example = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $null = Run "$($pair[0]) compile" $gcc ($flags + @($pair[0],$runtime,'-lm','-o',$example))
        $golden = [IO.File]::ReadAllText((Join-Path $root $pair[2])).Replace("`r`n","`n").TrimEnd("`n")
        $cText = Run "$($pair[0]) GCC" $example @()
        $direct = Run "$($pair[0]) direct" $xl @('run',$pair[0])
        $language = Run "$($pair[1]) JIT" $xl @('run',$pair[1])
        if ($cText -cne $golden -or $direct -cne $golden -or $language -cne $golden) { throw 'LRU bilingual golden differs' }
    }
    if (!$SkipAot) {
        foreach ($source in @('test/cases/extlib_lru.xl','release/extlibs/lru/test/allocations.xl',
            'release/extlibs/lru/test/input_budgets.xl','release/extlibs/lru/test/input_lifetime.xl',
            'release/extlibs/lru/test/owned_probe.xl','release/extlibs/lru/test/input_budgets_c.c',
            'release/examples/extlibs/lru.xl','release/extlibs/lru/examples/lru_c.c',
            'release/extlibs/lru/examples/snapshots.xl','release/extlibs/lru/examples/snapshots_c.c')) {
            $binary = Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            $expectedPath = if ($source -match '/snapshots') { 'release/extlibs/lru/examples/snapshots.expected.txt' }
                elseif ($source -in @('release/examples/extlibs/lru.xl','release/extlibs/lru/examples/lru_c.c')) { 'release/extlibs/lru/examples/basic.expected.txt' }
                else { '' }
            if ($expectedPath -ne '' -and $output -cne [IO.File]::ReadAllText((Join-Path $root $expectedPath)).Replace("`r`n","`n").TrimEnd("`n")) { throw 'LRU AOT golden differs' }
        }
        # 原顶层正例仍是必过项；不能挪进 main 或把退出错误当预期失败。
        $boundary = Join-Path $work 'lru_boundaries.exe'
        $null = Run 'original global boundaries build' $xl @('build','release/extlibs/lru/test/boundaries.xl','-o',$boundary)
        $null = Run 'original global boundaries AOT' $boundary @()
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
    Write-Host "[lru] report: $work.json"
    $base = [IO.Path]::GetFullPath($workParent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'LRU work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $target -Recurse -Force }
    if ($changed.Count) { throw 'LRU inputs changed during gate' }
}

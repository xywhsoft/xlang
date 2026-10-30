param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$parent = Join-Path $root 'build/test/consistent_hash_guard'
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
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/consistent_hash')
$environment = @{XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs')}

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/consistent_hash') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'test/cases/extlib_consistent_hash.xl','release/examples/extlibs/consistent_hash.xl',
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
    Write-Host "[consistent_hash] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

function Golden([string]$output, [string]$path) {
    if ($output -cne [IO.File]::ReadAllText((Join-Path $root $path)).Replace("`r`n","`n").TrimEnd("`n")) {
        throw "ConsistentHash output differs: $path"
    }
}

function ConfirmOwned([string]$text) {
    if (!$text.Contains('extern char* xchash_get_node(') -or
        $text -notmatch '\.Data\s*=\s*xchash_get_node\s*\(' -or
        $text -match '=\s*xlir_string_copy\s*\(') {
        throw 'ConsistentHash result is not directly adopted or still copied'
    }
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $manifest = Get-Content -LiteralPath 'release/extlibs/consistent_hash/xlang.package.json' -Raw | ConvertFrom-Json
    $modules = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest native syntax' $gcc ($flags + $modules + @('-fsyntax-only','release/extlibs/consistent_hash/consistent_hash_native.c'))
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/consistent_hash/include/consistent_hash_native.h'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c','release/extlibs/consistent_hash/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/consistent_hash/consistent_hash_native.c','-o',$native))
    foreach ($name in @('test_consistent_hash','test_threads','owned_budgets_c')) {
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @("release/extlibs/consistent_hash/test/$name.c",$runtime,$native,'-lm','-o',$binary))
        $null = Run $name $binary @()
    }
    $null = Run 'C budgets direct' $xl @('run','release/extlibs/consistent_hash/test/owned_budgets_c.c')
    foreach ($source in @('test/cases/extlib_consistent_hash.xl','release/extlibs/consistent_hash/test/boundaries.xl',
        'release/extlibs/consistent_hash/test/allocations.xl','release/extlibs/consistent_hash/test/layout.xl',
        'release/extlibs/consistent_hash/test/owned_budgets.xl','release/extlibs/consistent_hash/test/owned_probe.xl',
        'release/extlibs/consistent_hash/test/owned_lifetime.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    $null = Run 'private native entry' $xl @('check','release/extlibs/consistent_hash/test/private_native.xl') 1
    $negative = $checks[$checks.Count-1].result
    $diagnostic = $negative.stdout + $negative.stderr
    if (!$diagnostic.Contains('addNative') -or !$diagnostic.Contains('access')) { throw 'Expected private-access diagnostic' }

    $module = Join-Path $work 'module.c'
    $null = Run 'whole binding cgen' $xl @('cgen','release/extlibs/consistent_hash/test/owned_probe.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    ConfirmOwned $text
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxchash_[a-z_]+\(' } | Sort-Object -Unique)
    $sourceText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/consistent_hash/src/consistent_hash.xl'))
    $expected = @([regex]::Matches($sourceText,'extern "(xchash_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxchash_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 7 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete ConsistentHash generated ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/consistent_hash_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Run 'seven actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    foreach ($pair in @(@('release/extlibs/consistent_hash/examples/consistent_hash_c.c','release/examples/extlibs/consistent_hash.xl','release/extlibs/consistent_hash/examples/consistent_hash.expected.txt'),
        @('release/extlibs/consistent_hash/examples/routing_changes_c.c','release/extlibs/consistent_hash/examples/routing_changes.xl','release/extlibs/consistent_hash/examples/routing_changes.expected.txt'))) {
        $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $null = Run "$($pair[0]) compile" $gcc ($flags + @($pair[0],$runtime,'-lm','-o',$binary))
        Golden (Run "$($pair[0]) GCC" $binary @()) $pair[2]
        Golden (Run "$($pair[0]) direct" $xl @('run',$pair[0])) $pair[2]
        Golden (Run "$($pair[1]) JIT" $xl @('run',$pair[1])) $pair[2]
    }
    if (!$SkipAot) {
        foreach ($source in @('test/cases/extlib_consistent_hash.xl','release/extlibs/consistent_hash/test/owned_budgets.xl',
            'release/extlibs/consistent_hash/test/owned_probe.xl','release/extlibs/consistent_hash/test/owned_lifetime.xl',
            'release/extlibs/consistent_hash/test/owned_budgets_c.c','release/examples/extlibs/consistent_hash.xl',
            'release/extlibs/consistent_hash/examples/consistent_hash_c.c','release/extlibs/consistent_hash/examples/routing_changes.xl',
            'release/extlibs/consistent_hash/examples/routing_changes_c.c')) {
            $binary = Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match '/routing_changes') { Golden $output 'release/extlibs/consistent_hash/examples/routing_changes.expected.txt' }
            elseif ($source -in @('release/examples/extlibs/consistent_hash.xl','release/extlibs/consistent_hash/examples/consistent_hash_c.c')) {
                Golden $output 'release/extlibs/consistent_hash/examples/consistent_hash.expected.txt'
            }
        }
        # 独立原正例全部执行；聚合失败仍要求每项0，不把M2改成预期失败。
        $globalFailures = @()
        foreach ($name in @('boundaries','allocations','layout')) {
            try {
                $binary = Join-Path $work ($name+'.exe')
                $null = Run "original $name build" $xl @('build',"release/extlibs/consistent_hash/test/$name.xl",'-o',$binary)
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
    Write-Host "[consistent_hash] report: $work.json"
    $base = [IO.Path]::GetFullPath($parent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'ConsistentHash work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $target -Recurse -Force }
    if ($changed.Count) { throw 'ConsistentHash inputs changed during gate' }
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('tabulate_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'tabulate.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
$bounded = 'rows: 256','oversized render: rejected','table preserved: true','recovered render: true' -join "`n"
$checks = [Collections.Generic.List[object]]::new()
$hashes = @{}
$passed = $false
$failure = ''
$inputs = @('release/xl.exe','release/xl.dll','release/xlc.dll',
    'lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h',
    'release/include/xruntime_decl.h','test/cases/extlib_tabulate.xl',
    'release/examples/extlibs/tabulate.xl','tools/run_extlib_c.ps1',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1') |
    ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $PSScriptRoot '..') -File -Recurse | ForEach-Object FullName)
$inputs += $gcc,$python,$shell

function Observe([string]$label, [string]$program, [string[]]$arguments) {
    if ($null -eq $arguments) { $arguments = @() }
    if ([IO.Path]::GetExtension($program) -ieq '.ps1') {
        $arguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120
    $checks.Add([ordered]@{label=$label;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $work 'checks.json') -Encoding UTF8
    return $result
}

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $result = Observe $label $program $arguments
    if ($result.status -ne 'pass' -or $result.exitCode -ne 0) {
        throw "[$label] $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[tabulate] $label : PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.json') -Encoding UTF8
    $null = Run 'strict C and ABI' $gcc (@('-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only',
        '-Ilib/xrt','-Irelease/extlibs/tabulate/include') + $moduleFlags + @(
        'release/extlibs/tabulate/tabulate_native.c','release/extlibs/tabulate/tabulate_render.c',
        'release/extlibs/tabulate/test/abi_contract.c','release/extlibs/tabulate/test/test_render_budget.c',
        'release/extlibs/tabulate/examples/bounded_report_c.c'))
    $budget = Join-Path $work 'render_budget.exe'
    $flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-DXRT_IMPLEMENTATION','-Ilib/xrt')
    $links = @('-lws2_32','-lbcrypt','-lshlwapi','-liphlpapi')
    $null = Run 'GCC render budgets build' $gcc ($flags + @(
        'release/extlibs/tabulate/test/test_render_budget.c','-o',$budget) + $links)
    $corpus = Join-Path $work 'corpus'
    [void][IO.Directory]::CreateDirectory($corpus)
    $null = Run 'GCC render budgets' $budget @($corpus)
    $null = Run 'independent byte layouts' $python @('-B','release/extlibs/tabulate/test/check_render_corpus.py',$corpus)
    foreach ($source in @('test_tabulate.c','test_threads.c')) {
        $null = Run "C $source" $runner @('-Package','tabulate','-Source',"release/extlibs/tabulate/test/$source")
    }
    foreach ($source in @('test/cases/extlib_tabulate.xl',
        'release/extlibs/tabulate/test/boundaries.xl','release/extlibs/tabulate/test/allocations.xl',
        'release/extlibs/tabulate/test/render_budget.xl','release/extlibs/tabulate/test/return_budget.xl',
        'release/extlibs/tabulate/test/test_render_budget.c')) {
        $null = Run "JIT $source" $xl @('run',$source)
    }
    $generated = Join-Path $work 'tabulate_owned.c'
    $null = Run 'ownership cgen' $xl @('cgen','test/cases/extlib_tabulate.xl','--emit',$generated)
    $null = Run 'complete owned return and negative controls' $python @('-B',
        'release/extlibs/tabulate/test/check_owned_codegen.py',$generated)
    $negative = Observe 'private native negative' $xl @('check','release/extlibs/tabulate/test/private_native_negative.xl')
    $diagnostic = $negative.stdout + $negative.stderr
    if ($negative.status -ne 'fail' -or $negative.exitCode -eq 0 -or
        $diagnostic -notmatch 'appendCellNative' -or $diagnostic -notmatch 'access') {
        throw "Private native access was not rejected: $diagnostic"
    }
    $cases = @(
        @{name='core';source='test/cases/extlib_tabulate.xl';golden=$null},
        @{name='basic_xl';source='release/examples/extlibs/tabulate.xl';golden=$expected},
        @{name='basic_c';source='release/extlibs/tabulate/examples/tabulate_c.c';golden=$expected},
        @{name='bounded_xl';source='release/extlibs/tabulate/examples/bounded_report.xl';golden=$bounded},
        @{name='bounded_c';source='release/extlibs/tabulate/examples/bounded_report_c.c';golden=$bounded},
        @{name='budget_xl';source='release/extlibs/tabulate/test/render_budget.xl';golden='tabulate render limits: PASS'},
        @{name='budget_c';source='release/extlibs/tabulate/test/test_render_budget.c';golden=$null},
        @{name='return_xl';source='release/extlibs/tabulate/test/return_budget.xl';golden='tabulate return budget: 8 cases PASS'}
    )
    foreach ($case in $cases) {
        if ($null -ne $case.golden -and (Run "JIT $($case.name)" $xl @('run',$case.source)) -cne $case.golden) {
            throw "Example differs from golden: $($case.name)"
        }
        if (!$SkipAot) {
            $target = Join-Path $work ($case.name + '.exe')
            $null = Run "AOT $($case.name) build" $xl @('build',$case.source,'-o',$target)
            $output = Run "AOT $($case.name)" $target @()
            if ($null -ne $case.golden -and $output -cne $case.golden) { throw "AOT golden differs: $($case.name)" }
        }
    }
    foreach ($example in @('tabulate_c.c','bounded_report_c.c')) {
        $golden = if ($example -eq 'tabulate_c.c') { $expected } else { $bounded }
        if ((Run "package C $example" $runner @('-Package','tabulate','-Source',"release/extlibs/tabulate/examples/$example")) -cne $golden) {
            throw "Package C example differs: $example"
        }
    }
    # 真实正例仍要求成功；收集两条原全局AOT的退出状态，不以局部对照替代。
    $aotFailures = [Collections.Generic.List[string]]::new()
    if (!$SkipAot) {
        foreach ($name in @('boundaries','allocations')) {
            $target = Join-Path $work ($name + '.exe')
            $null = Run "AOT $name build" $xl @('build',"release/extlibs/tabulate/test/$name.xl",'-o',$target)
            $result = Observe "AOT $name" $target @()
            if ($result.status -ne 'pass' -or $result.exitCode -ne 0) { $aotFailures.Add($name) }
        }
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) { throw "Input changed during gate: $path" }
    }
    if ($aotFailures.Count) { throw "Required global AOT tests failed: $($aotFailures -join ', ')" }
    $passed = $true
    Write-Output "tabulate Windows C/XLang gate: PASS (AOT skipped=$SkipAot)"
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or
        ![IO.Path]::GetFileName($target).StartsWith('tabulate_guard_')) { throw "Unsafe cleanup path: $target" }
    [ordered]@{passed=$passed;failure=$failure;aotSkipped=[bool]$SkipAot;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 10 | Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) { Remove-Item -LiteralPath $target -Recurse -Force }
    else { Write-Warning "tabulate failure artifacts: $target" }
}

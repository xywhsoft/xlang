param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/ansi_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$outcome = 'failed'
$failure = ''
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/ansi')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$golden = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ansi.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/ansi') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($item in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/examples/extlibs/ansi.xl','test/cases/extlib_ansi.xl','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {
        Join-Path $root $item
    }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[ansi] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/ansi/include/ansi_native.h'))
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_STRING','-DXRT_MODULE_STRING_FORMAT',
        '-DXRT_MODULE_UNICODE','-fsyntax-only','release/extlibs/ansi/ansi_native.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/ansi/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/ansi/ansi_native.c','-o',$native))
    foreach ($name in @('test_ansi','test_threads','input_budgets_c')) {
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @('-DNDEBUG',"release/extlibs/ansi/test/$name.c",$native,$runtime) + $links + @('-o',$binary))
        $null = Run $name $binary @()
    }
    $null = Run 'C input budgets direct' $xl @('run','release/extlibs/ansi/test/input_budgets_c.c')
    foreach ($source in @('test/cases/extlib_ansi.xl','release/extlibs/ansi/test/boundaries.xl',
        'release/extlibs/ansi/test/allocations.xl','release/extlibs/ansi/test/input_budgets.xl',
        'release/extlibs/ansi/test/owned_probe.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    $module = Join-Path $work 'module.c'
    $null = Run 'full API cgen' $xl @('cgen','release/extlibs/ansi/examples/ansi.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxansi_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $sourceText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/ansi/src/ansi.xl'))
    $expected = @([regex]::Matches($sourceText,'extern "(xansi_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxansi_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 30 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete ANSI generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/ansi_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'thirty actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $owned = @([regex]::Matches($sourceText,'extern "(xansi_[a-z0-9_]+)" @owned') | ForEach-Object { $_.Groups[1].Value })
    if ($owned.Count -ne 29) { throw 'Incomplete ANSI owned result coverage' }
    foreach ($name in $owned) {
        if ([regex]::Matches($text,'\b\w+\.Data\s*=\s*'+$name+'\s*\(').Count -ne 1) { throw "ANSI result not adopted directly: $name" }
    }
    $probe = Join-Path $work 'owned_probe.c'
    $null = Run 'complete input-return cgen' $xl @('cgen','release/extlibs/ansi/test/owned_probe.xl','--emit',$probe)
    if ([IO.File]::ReadAllText($probe) -match '=\s*xlir_string_copy\s*\(') { throw 'ANSI validation or return still copies text' }

    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('-DNDEBUG','release/extlibs/ansi/examples/ansi_c.c',$native,$runtime) + $links + @('-o',$example))
    $cText = Run 'C example' $example @()
    $direct = Run 'C example direct' $xl @('run','release/extlibs/ansi/examples/ansi_c.c')
    $xlText = Run 'XLang example' $xl @('run','release/extlibs/ansi/examples/ansi.xl')
    if ($cText -cne $golden -or $direct -cne $golden -or $xlText -cne $golden) { throw 'ANSI golden output differs' }
    $public = Run 'public example' $xl @('run','release/examples/extlibs/ansi.xl')
    if (-not $SkipAot) {
        foreach ($source in @('test/cases/extlib_ansi.xl','release/extlibs/ansi/test/boundaries.xl',
            'release/extlibs/ansi/test/allocations.xl','release/extlibs/ansi/test/input_budgets.xl',
            'release/extlibs/ansi/test/owned_probe.xl','release/extlibs/ansi/test/input_budgets_c.c',
            'release/extlibs/ansi/examples/ansi.xl','release/extlibs/ansi/examples/ansi_c.c','release/examples/extlibs/ansi.xl')) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match '/ansi/examples/' -and $output -cne $golden) { throw 'ANSI example AOT output differs' }
            if ($source -eq 'release/examples/extlibs/ansi.xl' -and $output -cne $public) { throw 'ANSI public JIT/AOT output differs' }
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
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;
        failure=$failure;aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[ansi] report: $work.json"
    $base = [IO.Path]::GetFullPath($workParent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'ANSI work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Inputs changed during ANSI gate' }
}

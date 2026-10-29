param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test'
$work = Join-Path $workParent ('linalg_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',
    '-Irelease/extlibs/linalg','-Irelease/extlibs/linalg/include')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$outcome = 'failed'
$failure = ''

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/linalg') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/examples/extlibs/linalg.xl','release/examples/extlibs/linalg_c.c','test/cases/extlib_linalg.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    $gcc
    if ($python) { $python }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[linalg] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $native = 'release/extlibs/linalg/linalg_native.c'
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_MATH',
        '-DXRT_MODULE_VALUE_CONTAINER','-fsyntax-only',$native))
    $generated = Join-Path $work 'module.c'
    $null = Run 'generated ABI source' $xl @('cgen','release/extlibs/linalg/test/numeric_edges.xl','--emit',$generated)
    $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxlin_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/linalg/src/linalg.xl')),'extern "(xlin_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxlin_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 13 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete linalg generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"linalg_native.c`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated thirteen prototypes with definitions' $gcc ($flags + @('-fsyntax-only',$abi))

    $runtime = Join-Path $work 'xrt.o'
    $object = Join-Path $work 'native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c','release/extlibs/linalg/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c',$native,'-o',$object))
    $test = Join-Path $work 'test.exe'
    $null = Run 'C boundaries compile' $gcc ($flags + @('release/extlibs/linalg/test/test_linalg.c',$object,$runtime) + $links + @('-o',$test))
    $null = Run 'C boundaries' $test @()
    $numericSource = 'release/extlibs/linalg/test/numeric_edges_c.c'
    $numeric = Join-Path $work 'numeric.exe'
    $null = Run 'C numeric compile' $gcc ($flags + @('-DNDEBUG',$numericSource,$runtime) + $links + @('-o',$numeric))
    $null = Run 'C numeric edges' $numeric @()
    $null = Run 'C numeric direct xl run' $xl @('run',$numericSource)
    if ($python) {
        $null = Run 'Python version' $python @('--version')
        $library = Join-Path $work 'oracle.dll'
        $null = Run 'oracle library compile' $gcc ($flags + @('-shared','-Wl,--export-all-symbols',$object,$runtime) + $links + @('-o',$library))
        $oracle = Run 'exact rational oracle' $python @('release/extlibs/linalg/test/exact_inverse.py',$library)
        $data = $oracle | ConvertFrom-Json
        if ($data.cases -ne 2161 -or $data.failures -ne 0) { throw 'Incomplete linalg oracle coverage' }
    } else { Write-Host '[linalg] optional Python exact oracle: SKIP (Python unavailable)' }

    $cExample = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('release/examples/extlibs/linalg_c.c',$object,$runtime) + $links + @('-o',$cExample))
    $expectedText = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/linalg/test/linalg.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $cText = Run 'C example' $cExample @()
    $direct = Run 'C example direct xl run' $xl @('run','release/examples/extlibs/linalg_c.c')
    $xlText = Run 'XLang example' $xl @('run','release/examples/extlibs/linalg.xl')
    if ($cText -cne $expectedText -or $direct -cne $expectedText -or $xlText -cne $expectedText) { throw 'Linalg example golden output differs' }
    $calibration = Join-Path $work 'calibration.exe'
    $calibrationC = 'release/extlibs/linalg/examples/calibration_c.c'
    $null = Run 'C calibration compile' $gcc ($flags + @($calibrationC,$object,$runtime) + $links + @('-o',$calibration))
    $calibrationText = Run 'C calibration' $calibration @()
    $calibrationDirect = Run 'C calibration direct xl run' $xl @('run',$calibrationC)
    $calibrationXL = Run 'XLang calibration' $xl @('run','release/extlibs/linalg/examples/calibration.xl')
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/linalg/examples/calibration.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($calibrationText -cne $golden -or $calibrationDirect -cne $golden -or $calibrationXL -cne $golden) { throw 'Calibration golden output differs' }
    foreach ($source in @('test/cases/extlib_linalg.xl','release/extlibs/linalg/test/boundaries.xl',
        'release/extlibs/linalg/test/allocations.xl','release/extlibs/linalg/test/numeric_edges.xl')) {
        $null = Run "$source JIT" $xl @('run',$source)
    }
    if (-not $SkipAot) {
        foreach ($source in @('release/extlibs/linalg/test/aot_core.xl','release/extlibs/linalg/test/boundaries.xl',
            'release/extlibs/linalg/test/allocations.xl','release/extlibs/linalg/test/numeric_edges.xl',
            $numericSource,'test/cases/extlib_linalg.xl','release/examples/extlibs/linalg.xl',
            'release/examples/extlibs/linalg_c.c','release/extlibs/linalg/examples/calibration.xl',$calibrationC)) {
            $target = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$target)
            $output = Run "$source AOT" $target @()
            if ($source -match '/examples/extlibs/linalg(_c)?\.(xl|c)$' -and $output -cne $expectedText) { throw 'Linalg AOT example output differs' }
            if ($source -like '*/examples/calibration*' -and $output -cne $golden) { throw 'Calibration AOT golden output differs' }
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
        aotSkipped=[bool]$SkipAot;pythonOracle=[bool]$python;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[linalg] report: $work.json"
    $base = [IO.Path]::GetFullPath($workParent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Linalg work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Inputs changed during linalg gate' }
}

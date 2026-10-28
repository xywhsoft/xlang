param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$workRelative = 'build/test/gettext_guard_' + [guid]::NewGuid().ToString('N')
$work = Join-Path $root $workRelative
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/gettext')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$failure = ''
function InputPaths {
    @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/cases/extlib_gettext.xl', 'test/fixtures/gettext_mini.mo',
    'release/examples/extlibs/gettext.xl','tools/extlib_smoke_process.ps1',
    'tools/xlir_native_process.ps1') | ForEach-Object { Join-Path $root $_ }
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/gettext') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    $gcc
}
$hashes = @{}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[gettext] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'compiler version' $gcc @('--version')
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/gettext/test/xrt_runtime.c', '-o', $runtime))
    $native = @('release/extlibs/gettext/gettext_native.c',
        'release/extlibs/gettext/gettext_plural.c')
    $test = Join-Path $work 'gettext_test.exe'
    $null = Run 'C test compile' $gcc ($flags + $native +
        @('release/extlibs/gettext/test/test_gettext.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @($workRelative)

    $contextTest = Join-Path $work 'gettext_context_test.exe'
    $null = Run 'C context compile' $gcc ($flags + $native +
        @('release/extlibs/gettext/test/test_context_queries.c', $runtime) +
        $links + @('-o', $contextTest))
    $null = Run 'C context queries' $contextTest @()
    $layoutTest = Join-Path $work 'po-layout-c.exe'
    $null = Run 'C PO layout compile' $gcc ($flags + @('release/extlibs/gettext/test/po_layout_c.c',$runtime) + $links + @('-o',$layoutTest))
    $null = Run 'C PO layout' $layoutTest @()
    $null = Run 'C PO layout direct' $xl @('run','release/extlibs/gettext/test/po_layout_c.c')
    $layoutFixture = Join-Path $root 'release/extlibs/gettext/test/po_layout.po'
    $layoutGolden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/gettext/examples/layout.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $layoutC = Join-Path $work 'layout-c.exe'
    $null = Run 'C layout example compile' $gcc ($flags + @('release/extlibs/gettext/examples/layout_c.c',$runtime) + $links + @('-o',$layoutC))
    $text = Run 'C layout example' $layoutC @($layoutFixture)
    $direct = Run 'C layout example direct' $xl @('run','release/extlibs/gettext/examples/layout_c.c','--',$layoutFixture)
    $language = Run 'XLang layout example' $xl @('run','release/extlibs/gettext/examples/layout.xl','--',$layoutFixture)
    foreach ($output in @($text,$direct,$language)) { if ($output -cne $layoutGolden) { throw 'PO layout C/XLang golden mismatch' } }
    $generated = Join-Path $work 'module.c'
    $null = Run 'generated source' $xl @('cgen','release/extlibs/gettext/test/boundaries.xl','--emit',$generated)
    $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxgt_[a-z_]+\(' } | Sort-Object -Unique)
    $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/gettext/src/gettext.xl')),'extern "(xgt_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxgt_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 7 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete gettext generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#define XRT_MODULE_ALL`n#include `"xrt.h`"`n#include `"gettext_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Run 'generated seven prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    $example = Join-Path $work 'gettext_example.exe'
    $null = Run 'C example compile' $gcc ($flags +
        @('release/extlibs/gettext/examples/gettext_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directC = Run 'C example xl run' $xl @('run',
        'release/extlibs/gettext/examples/gettext_c.c')
    if ($directC -cne $cOutput) {
        throw 'direct C gettext example differs from GCC output'
    }

    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_gettext.xl')
    $null = Run 'XLang duplicate keys' $xl @('run', 'release/extlibs/gettext/test/duplicates.xl')
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/gettext/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run', 'release/extlibs/gettext/test/allocations.xl')
    $null = Run 'XLang input limits' $xl @('run', 'release/extlibs/gettext/test/input_limits.xl')
    $null = Run 'XLang PO layout' $xl @('run','release/extlibs/gettext/test/po_layout.xl')
    if (-not $SkipAot) {
        $cases = @(
            @('central', 'test/cases/extlib_gettext.xl'),
            @('duplicates', 'release/extlibs/gettext/test/duplicates.xl'),
            @('boundaries', 'release/extlibs/gettext/test/boundaries.xl'),
            @('allocations', 'release/extlibs/gettext/test/allocations.xl'),
            @('input_limits', 'release/extlibs/gettext/test/input_limits.xl')
        )
        foreach ($case in $cases) {
            $name = $case[0]
            $target = Join-Path $work "gettext_$name.exe"
            $null = Run "XLang $name build" $xl @('build', $case[1], '-o', $target)
            $null = Run "XLang $name AOT" $target @()
        }
        foreach ($source in @('test/po_layout.xl','test/po_layout_c.c','examples/layout.xl','examples/layout_c.c')) {
            $target = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.exe')
            $null = Run "$source build" $xl @('build',"release/extlibs/gettext/$source",'-o',$target)
            $arguments = if ($source -like 'examples/*') { @($layoutFixture) } else { @() }
            $text = Run "$source AOT" $target $arguments
            if ($source -like 'examples/*' -and $text -cne $layoutGolden) { throw 'PO layout example AOT golden mismatch' }
        }
    }

    Push-Location (Join-Path $root 'release')
    try {
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/gettext/examples/gettext.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang gettext example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[gettext] C/XLang parity: PASS'
        $publicOutput = Run 'Public XLang example' $xl @('run', 'examples/extlibs/gettext.xl')

        if (-not $SkipAot) {
            $exampleAot = Join-Path $work 'gettext_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/gettext/examples/gettext.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT gettext example differs from C'
            }
            $publicAot = Join-Path $work 'gettext_public_example.exe'
            $null = Run 'Public XLang example build' $xl @('build',
                'examples/extlibs/gettext.xl', '-o', $publicAot)
            $publicAotOutput = Run 'Public XLang example AOT' $publicAot @()
            if ($publicAotOutput -cne $publicOutput) {
                throw 'public XLang AOT gettext example differs from JIT'
            }
            $cExampleAot = Join-Path $work 'gettext_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/gettext/examples/gettext_c.c', '-o', $cExampleAot)
            Push-Location $root
            try {
                $cAotOutput = Run 'C example AOT' $cExampleAot @()
            } finally {
                Pop-Location
            }
            if ($cAotOutput -cne $cOutput) {
                throw 'C AOT gettext example differs from GCC'
            }
        }
    } finally {
        Pop-Location
    }
    $passed = $true
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.ContainsKey($_) })
    if ($changed.Count) { $passed = $false }
    $report = Join-Path $testRoot (([IO.Path]::GetFileName($work)) + '.json')
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');passed=$passed;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[gettext] report: $report"
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        if ($passed) {
            Remove-Item -LiteralPath $safeWork -Recurse -Force
        } else {
            Write-Warning "gettext failure artifacts: $safeWork"
        }
    }
    if ($changed.Count) { throw 'Inputs changed during gettext gate' }
}

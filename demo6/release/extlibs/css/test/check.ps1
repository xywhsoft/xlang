param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/css_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt', '-Irelease/extlibs/css/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$outcome = 'failed'
$failure = ''

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/css') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/examples/extlibs/css.xl','release/examples/extlibs/css_c.c','test/cases/extlib_css.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[css] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $native = 'release/extlibs/css/css_native.c'
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE', '-fsyntax-only', $native))
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/css/test/abi_contract.c'))
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/css/test/runtime.c', '-o', $runtime))

    foreach ($name in @('test_css', 'test_css_threads')) {
        $target = Join-Path $work "$name.exe"
        $null = Run "$name compile" $gcc ($flags + @(
            $native, "release/extlibs/css/test/$name.c", $runtime) +
            $links + @('-o', $target))
        $null = Run $name $target @()
    }
    $edgeSource = 'release/extlibs/css/test/scanner_edges_c.c'
    $edgeExe = Join-Path $work 'scanner-c.exe'
    $null = Run 'C scanner compile' $gcc ($flags + @($edgeSource,$runtime) + $links + @('-o',$edgeExe))
    $null = Run 'C scanner edges' $edgeExe @()
    $null = Run 'C scanner direct xl run' $xl @('run',$edgeSource)

    $auditSource = 'release/extlibs/css/examples/audit_c.c'
    $auditExe = Join-Path $work 'audit-c.exe'
    $null = Run 'C audit compile' $gcc ($flags + @($auditSource,$runtime) + $links + @('-o',$auditExe))
    $auditC = Run 'C audit' $auditExe @()
    $auditDirect = Run 'C audit direct xl run' $xl @('run',$auditSource)
    $auditGolden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/css/examples/audit.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($auditC -cne $auditGolden -or $auditDirect -cne $auditGolden) { throw 'CSS audit C golden output differs' }

    $example = Join-Path $work 'css_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/examples/extlibs/css_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/examples/extlibs/css_c.c')
    if ($cOutput -cne $directCOutput) {
        throw 'direct CSS C example output differs'
    }
    $generated = Join-Path $work 'css_owned.c'
    $null = Run 'XLang direct string cgen' $xl @('cgen',
        'release/extlibs/css/test/boundaries.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxcss_[a-z_]+\(' } | Sort-Object -Unique)
    $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/css/src/css.xl')),'extern "(xcss_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxcss_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 6 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete CSS generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"css_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated six prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    foreach ($symbol in @('xcss_get', 'xcss_selector_at',
            'xcss_strip_comments')) {
        if ($generatedText -notmatch ('\.Data\s*=\s*' + $symbol + '\s*\(')) {
            throw "css $symbol must adopt the owned C result directly"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1')) {
        throw 'CSS results must not use legacy transfer or copying'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_css.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/css/test/boundaries.xl')
        $null = Run 'XLang allocations' $xl @('run', 'extlibs/css/test/allocations.xl')
        $null = Run 'XLang scanner edges' $xl @('run','extlibs/css/test/scanner_edges.xl')
        $auditXL = Run 'XLang audit' $xl @('run','extlibs/css/examples/audit.xl')
        if ($auditXL -cne $auditGolden) { throw 'CSS audit language output differs from C' }
        $xlOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/css.xl')
        if ($cOutput -cne $xlOutput) {
            throw 'C/XLang CSS example outputs differ'
        }
        Write-Host '[css] C/XLang parity: PASS'

        if (-not $SkipAot) {
            $core = Join-Path $work 'css_core.exe'
            $null = Run 'XLang core build' $xl @('build',
                'extlibs/css/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            $target = Join-Path $work 'css_boundaries.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/css/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $allocationsAot = Join-Path $work 'css_allocations.exe'
            $null = Run 'XLang allocations build' $xl @('build',
                'extlibs/css/test/allocations.xl', '-o', $allocationsAot)
            $null = Run 'XLang allocations AOT' $allocationsAot @()
            $exampleAot = Join-Path $work 'css_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/css.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT CSS example differs from C'
            }
            $cExampleAot = Join-Path $work 'css_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'examples/extlibs/css_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'C AOT CSS example differs from GCC'
            }
            foreach ($source in @('extlibs/css/test/scanner_edges.xl','extlibs/css/test/scanner_edges_c.c',
                'extlibs/css/examples/audit.xl','extlibs/css/examples/audit_c.c')) {
                $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.exe')
                $null = Run "$source build" $xl @('build',$source,'-o',$binary)
                $text = Run "$source AOT" $binary @()
                if ($source -like '*/examples/*' -and $text -cne $auditGolden) { throw 'CSS audit AOT output differs from C' }
            }
        }
    } finally {
        Pop-Location
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
    $report = Join-Path $workParent (([IO.Path]::GetFileName($work)) + '.json')
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[css] report: $report"
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "css temporary work path escaped build/test: $workPath"
    }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $workPath -Recurse -Force }
    if ($changed.Count) { throw 'Inputs changed during CSS gate' }
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('plist_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$inputs = @()
foreach ($package in @('plist', 'xml.dom', 'xml.sax', '_expat')) {
    $inputs += Get-ChildItem (Join-Path $root "release/extlibs/$package") -Recurse -File |
        ForEach-Object FullName
}
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1', 'test/cases/extlib_plist.xl',
    'release/examples/extlibs/plist.xl', 'release/examples/extlibs/plist_c.c') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[plist] ${name}: PASS"
    return $output
}

Push-Location $root
try {
    [void][IO.Directory]::CreateDirectory($work)
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'strict native C syntax' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/plist/include',
        'release/extlibs/plist/plist_native.c', 'release/extlibs/plist/test/test_native.c')
    foreach ($source in @(
        'release/extlibs/plist/test/test_plist.c',
        'release/extlibs/plist/test/test_native.c'
    )) {
        $null = Run $source $runner @('plist', $source)
    }

    foreach ($source in @('test/cases/extlib_plist.xl',
        'release/extlibs/plist/test/boundaries.xl', 'release/extlibs/plist/test/allocations.xl',
        'release/extlibs/plist/test/owned_results.xl')) {
        $null = Run $source $xl @('run', $source)
    }
    $generated = Join-Path $work 'owned_probe.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/plist/test/owned_probe.xl', '--emit', $generated)
    $text = [IO.File]::ReadAllText($generated)
    foreach ($symbol in @('xplist_error_message', 'xplist_stringify_text')) {
        $pattern = '\b\w+\.Data\s*=\s*' + $symbol + '\s*\('
        if ([regex]::Matches($text, $pattern).Count -ne 1) {
            throw "plist probe must directly adopt $symbol exactly once"
        }
    }
    # 此夹具仍有构树所需的值转换，约束的是旧的两个返回复制不能回归。
    if ([regex]::Matches($text, '=\s*xlir_string_copy\s*\(').Count -gt 5) {
        throw 'plist controlled fixture regained redundant return copies'
    }

    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $privateDiagnostic = ((& $xl check release/extlibs/plist/test/private_native_document_negative.xl 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n")
        $privateExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "private native document check: exit=$privateExitCode`n$privateDiagnostic`n")
    if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('no such exported type')) {
        throw "plist native document access check failed: $privateDiagnostic"
    }

    $cXml = Join-Path $work 'c.xml'
    $cBinary = Join-Path $work 'c.bplist'
    $xlXml = Join-Path $work 'xlang.xml'
    $xlBinary = Join-Path $work 'xlang.bplist'
    $xlangOutput = @(Run 'XLang example' $xl @('run',
        'release/examples/extlibs/plist.xl', '--', $xlXml, $xlBinary))
    $cOutput = @(Run 'C example' $xl @('run',
        'release/examples/extlibs/plist_c.c', '--', $cXml, $cBinary))
    if (($xlangOutput -join "`n") -cne ($cOutput -join "`n")) {
        throw 'plist C/XLang example output differs'
    }
    $null = Run 'C/XLang plistlib interop' 'python' @('-I',
        'release/extlibs/plist/test/check_external.py', $cXml, $xlXml, $cBinary, $xlBinary)
    if (-not $SkipAot) {
        $core = Join-Path $work 'plist_core.exe'
        $null = Run 'import AOT build' $xl @('build', 'release/extlibs/plist/test/smoke.xl', '-o', $core)
        $null = Run 'import AOT' $core @()
        $central = Join-Path $work 'central.exe'
        $null = Run 'central AOT build' $xl @('build', 'test/cases/extlib_plist.xl', '-o', $central)
        $null = Run 'central AOT' $central @()

        foreach ($case in @('boundaries', 'allocations', 'owned_results')) {
            $caseExe = Join-Path $work "plist_$case.exe"
            $null = Run "$case AOT build" $xl @('build', "release/extlibs/plist/test/$case.xl", '-o', $caseExe)
            $null = Run "$case AOT" $caseExe @()
        }

        $example = Join-Path $work 'plist_example.exe'
        $aotXml = Join-Path $work 'aot.xml'
        $aotBinary = Join-Path $work 'aot.bplist'
        $null = Run 'XLang example AOT build' $xl @('build', 'release/examples/extlibs/plist.xl', '-o', $example)
        $aotOutput = @(Run 'XLang example AOT' $example @($aotXml, $aotBinary))
        if (($aotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'plist example AOT output differs from C/XLang'
        }
        $null = Run 'XLang AOT plistlib interop' 'python' @('-I',
            'release/extlibs/plist/test/check_external.py', $cXml, $aotXml, $cBinary, $aotBinary)

        $cAot = Join-Path $work 'plist_c_example.exe'
        $cAotXml = Join-Path $work 'c_aot.xml'
        $cAotBinary = Join-Path $work 'c_aot.bplist'
        $null = Run 'C example AOT build' $xl @('build', 'release/examples/extlibs/plist_c.c', '-o', $cAot)
        $cAotOutput = @(Run 'C example AOT' $cAot @($cAotXml, $cAotBinary))
        if (($cAotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'plist C example AOT output differs'
        }
        $null = Run 'C AOT plistlib interop' 'python' @('-I',
            'release/extlibs/plist/test/check_external.py', $cXml, $cAotXml, $cBinary, $cAotBinary)
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "plist validation input changed: $path"
        }
    }
    $passed = $true
    if ($SkipAot) {
        Write-Output 'plist Windows C/XLang (AOT skipped) gate: PASS'
    } else {
        Write-Output 'plist Windows C/XLang/AOT gate: PASS'
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work) {
        $checked = [IO.Path]::GetFullPath($work)
        if (-not $checked.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw 'unsafe plist test cleanup path' }
        if ($passed) {
            Get-ChildItem -LiteralPath $checked -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $checked -Force
        } else {
            Write-Warning "plist failure artifacts retained: $checked"
        }
    }
}

param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('zip_guard_' + [guid]::NewGuid().ToString('N'))
$output = Join-Path $work 'zip_aot_core.exe'
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$label, [string]$program, [string[]]$arguments,
    [string]$expected = $null) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($lines -join "`n")" }
    $actual = ($lines -join "`n").Replace("`r", '').TrimEnd("`n")
    if ($PSBoundParameters.ContainsKey('expected') -and $actual -cne $expected) {
        throw "${label}: unexpected output`n$actual"
    }
    Write-Host "[zip] ${label}: PASS"
}

function RunNegative([string]$label, [string]$source, [string]$symbol) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $xl check $source 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    $diagnostic = $lines -join "`n"
    if ($code -eq 0 -or -not $diagnostic.Contains($symbol) -or
        -not $diagnostic.Contains('access')) {
        throw "${label}: expected access rejection`n$diagnostic"
    }
    Write-Host "[zip] ${label}: PASS"
}

Push-Location $root
try {
    Run 'libzip manifest macro compile' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt',
        '-Irelease/extlibs/_libzip/third_party/libzip/lib',
        '-DXRT_MODULE_FILE_TEMP',
        'release/extlibs/_libzip/platform/zip_source_file_xrt.c')
    Run 'Python/C ZIP interop' 'python' @(
        'release/extlibs/zip/test/interop.py') 'zip Python/C interop: PASS'
    Run 'XLang text transfer' $xl @('run', 'release/extlibs/zip/test/text_transfer.xl')
    Run 'XLang boundary' $xl @('run', 'release/extlibs/zip/test/boundaries.xl')
    RunNegative 'Native member access' 'release/extlibs/zip/test/private_native_negative.xl' 'addNative'
    Run 'XLang example' $xl @('run', 'release/examples/extlibs/zip.xl') 'zip C/XL example: PASS'
    Run 'C boundary' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'zip',
        '-Source', 'release/extlibs/zip/test/test_zip.c')
    Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'zip',
        '-Source', 'release/examples/extlibs/zip_c.c') 'zip C/XL example: PASS'
    if (-not $SkipAot) {
        Run 'XLang core AOT build' $xl @('build',
            'release/extlibs/zip/test/aot_core.xl', '-o', $output)
        Run 'XLang core AOT call' $output @() ''
        $textAot = Join-Path $work 'zip_aot_text_transfer.exe'
        Run 'XLang text transfer AOT build' $xl @('build',
            'release/extlibs/zip/test/text_transfer.xl', '-o', $textAot)
        Run 'XLang text transfer AOT' $textAot @() ''
        $boundaryAot = Join-Path $work 'zip_aot_boundaries.exe'
        Run 'XLang boundaries AOT build' $xl @('build',
            'release/extlibs/zip/test/boundaries.xl', '-o', $boundaryAot)
        Run 'XLang boundaries AOT' $boundaryAot @() 'zip boundaries: ok'
        $exampleAot = Join-Path $work 'zip_aot_example.exe'
        Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/zip.xl', '-o', $exampleAot)
        Run 'XLang example AOT' $exampleAot @() 'zip C/XL example: PASS'
        $cExampleAot = Join-Path $work 'zip_c_example.exe'
        Run 'C example AOT build' $xl @('build',
            'release/extlibs/zip/test/c_aot_import.xl',
            'release/examples/extlibs/zip_c.c', '-o', $cExampleAot)
        Run 'C example AOT' $cExampleAot @() 'zip C/XL example: PASS'
    }
} finally {
    Pop-Location
    $safeRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe ZIP work directory' }
    Remove-Item -LiteralPath $target -Recurse -Force
}

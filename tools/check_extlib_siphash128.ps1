param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$work = Join-Path $Root 'build/test/siphash128_guard'
[IO.Directory]::CreateDirectory($work) | Out-Null
$case = Join-Path $Root 'test/cases/extlib_siphash128_core.xl'
$generated = Join-Path $work 'siphash128_generated.c'
$binary = Join-Path $work 'siphash128_case.exe'
$coreBinary = Join-Path $work 'siphash128_core.exe'
$private = Join-Path $work '__xl_runtime'
[IO.Directory]::CreateDirectory($private) | Out-Null
foreach ($name in @('xl_runtime.h', 'xl_extlib_abi.h', 'xrt_decl.h', 'xruntime_decl.h')) {
    Copy-Item -LiteralPath (Join-Path $Root "release/include/$name") -Destination (Join-Path $private $name) -Force
}
Get-ChildItem -LiteralPath (Join-Path $Root 'src/runtime') -Filter 'xl6_*.h' -File |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $private $_.Name) -Force }

function Check([string]$name, [string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$name failed (exit $LASTEXITCODE)" }
    Write-Host "[siphash128] ${name}: PASS"
}

Push-Location $Root
try {
    $manifest = Get-Content 'release/extlibs/siphash128/xlang.package.json' -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.name -ne 'siphash128') { throw 'siphash128 package name mismatch' }
    $sources = @($manifest.artifact.cSources)
    if ($sources.Count -ne 2 -or
        $sources -notcontains 'third_party/siphash.c' -or
        $sources -notcontains 'siphash128_native.c' -or
        @($manifest.artifact.xrtModules) -contains 'XRT_MODULE_HASH_KEYED') {
        throw 'siphash128 manifest unexpectedly compiles duplicate 64-bit SipHash code'
    }
    Check 'language-check' $xl @('check', $case)
    Check 'language-JIT' $xl @('run', $case)
    Check 'language-AOT-build' $xl @('build', $case, '-o', $binary)
    Check 'language-AOT-run' $binary @()
    Check 'language-Cgen' $xl @('cgen', $case, '--emit', $generated)
    Check 'generated-C-syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable',
        '-Wno-missing-field-initializers', '-fsyntax-only', "-I$work",
        '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/include', '-I.', $generated)
    Check 'native-C-syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', '-Ilib/xruntime',
        'release/extlibs/siphash128/siphash128_native.c')
    Check 'C-core-build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        'release/extlibs/siphash128/test/test_siphash128_core.c',
        'release/extlibs/siphash128/third_party/siphash.c', '-o', $coreBinary)
    Check 'C-core-run' $coreBinary @()
} finally {
    Pop-Location
}

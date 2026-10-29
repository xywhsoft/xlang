param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/netrc_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[netrc] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $runtimeSource = Join-Path $root 'release/extlibs/netrc/test/xrt_runtime.c'
    $xrtHeader = Join-Path $root 'lib/xrt/xrt.h'
    if (-not (Test-Path -LiteralPath $runtime) -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt
            (Get-Item -LiteralPath $runtimeSource).LastWriteTimeUtc -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt
            (Get-Item -LiteralPath $xrtHeader).LastWriteTimeUtc) {
        $null = Run 'XRT compile' $gcc ($flags + @('-c',
            'release/extlibs/netrc/test/xrt_runtime.c', '-o', $runtime))
    }
    $nativeFlags = $flags + @('-Irelease/extlibs/netrc', '-fsyntax-only')
    $null = Run 'native ABI compile' $gcc ($nativeFlags + @(
        'release/extlibs/netrc/netrc_native.c'))
    $null = Run 'manifest macro compile' $gcc ($nativeFlags + @(
        '-DXRT_MODULE_VALUE_CONTAINER',
        'release/extlibs/netrc/netrc_native.c'))

    foreach ($name in @('test_netrc', 'test_netrc_boundaries', 'test_native_tls')) {
        $source = "release/extlibs/netrc/test/$name.c"
        $target = Join-Path $work "$name.exe"
        $null = Run "$name compile" $gcc ($flags + @($source, $runtime) +
            $links + @('-o', $target))
        Push-Location $work
        try {
            $null = Run $name $target @()
        } finally {
            Pop-Location
        }
    }

    $ownedSource = 'release/extlibs/netrc/test/owned_text.xl'
    $null = Run 'owned text JIT' $xl @('run', $ownedSource)
    $ownedC = Join-Path $work 'owned_text.c'
    $null = Run 'owned text cgen' $xl @('cgen', $ownedSource, '--emit', $ownedC)
    $generated = Get-Content -LiteralPath $ownedC -Raw
    if ($generated -cnotmatch 'extern char\* xnetrc_error_message\(' -or
        $generated -cnotmatch '\bv\d+\.Data\s*=\s*xnetrc_error_message\s*\(' -or
        $generated -cnotmatch 'xl6_xrt_take_cstr\(') {
        throw 'netrc text results were not adopted through their declared ownership paths'
    }
    if (-not $SkipAot) {
        $ownedExe = Join-Path $work 'owned_text.exe'
        $null = Run 'owned text AOT build' $xl @('build', $ownedSource, '-o', $ownedExe)
        $null = Run 'owned text AOT' $ownedExe @()
    }

    $example = Join-Path $work 'netrc_c_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/netrc/examples/basic.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/netrc.xl')
    $goldenPath = Join-Path $root 'release/extlibs/netrc/examples/basic.expected.txt'
    $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
        throw 'C/XLang example output differs from the common golden file'
    }
    $centralOutput = Run 'central C example' $xl @(
        'run', 'release/examples/extlibs/netrc_c.c')
    if ($centralOutput -cne $golden) {
        throw 'central C example output differs from the common golden file'
    }

    $tag = [Guid]::NewGuid().ToString('N')
    $cPath = Join-Path $work "netrc_c_$tag.netrc"
    $xlPath = Join-Path $work "netrc_凭据_$tag.netrc"
    try {
        $cFileOutput = Run 'C file example' $example @($cPath)
        $xlFileOutput = Run 'XLang UTF-8 file example' $xl @(
            'run', 'release/examples/extlibs/netrc.xl', '--', $xlPath)
        if ($cFileOutput -cne $golden -or $xlFileOutput -cne $golden -or
            (Get-FileHash -LiteralPath $cPath -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $xlPath -Algorithm SHA256).Hash) {
            throw 'C/XLang netrc file examples differ'
        }
        $python = Get-Command python -CommandType Application -All `
            -ErrorAction SilentlyContinue |
            Where-Object { $_.Version -ne [version]'0.0.0.0' } |
            Select-Object -First 1
        if ($null -ne $python) {
            $null = Run 'Python netrc file interop' $python.Source @(
                'release/extlibs/netrc/test/interop.py', $cPath, $xlPath)
        } else {
            Write-Warning 'Python 3 unavailable; skipping independent netrc interop check'
        }
    } finally {
        Remove-Item -LiteralPath $cPath, $xlPath -Force -ErrorAction SilentlyContinue
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_netrc.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/netrc/test/boundaries.xl')
        $null = Run 'XLang named type' $xl @('run', 'extlibs/netrc/test/named_type.xl')
        $null = Run 'XLang allocations' $xl @('run', 'extlibs/netrc/test/allocations.xl')
        if (-not $SkipAot) {
            $testAot = Join-Path $work 'netrc_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/netrc/test/boundaries.xl', '-o', $testAot)
            $null = Run 'XLang boundaries AOT' $testAot @()
            $exampleAot = Join-Path $work 'netrc_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/netrc.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $golden) { throw 'XLang AOT output differs from golden file' }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "netrc temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}

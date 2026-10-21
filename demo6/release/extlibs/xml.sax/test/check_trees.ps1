param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $workRoot ('xml_trees_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/xml.sax/include',
    '-Irelease/extlibs/xml.dom/include',
    '-Irelease/extlibs/xml.etree/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[xml] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function ExpectPrivate([string]$name, [string]$path,
    [string]$symbol, [string]$detail) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $xl check $path 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    $diagnostic = $output -join "`n"
    if ($LASTEXITCODE -eq 0 -or -not $diagnostic.Contains($symbol) -or
        -not $diagnostic.Contains($detail)) {
        throw "${name}: expected private-access diagnostic`n$diagnostic"
    }
    Write-Host "[xml] ${name}: PASS"
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/xml.sax/test/xrt_runtime.c', '-o', $runtime))
    foreach ($kind in @('sax', 'dom', 'etree')) {
        $target = Join-Path $work "test_xml_$kind.exe"
        $null = Run "$kind C compile" $gcc ($flags + @(
            "release/extlibs/xml.$kind/test/test_xml_$kind.c", $runtime) +
            $links + @('-o', $target))
        $null = Run "$kind C boundaries" $target @()
        $null = Run "$kind XLang central" $xl @('run',
            "test/cases/extlib_xml_$kind.xl")
        $null = Run "$kind XLang boundaries" $xl @('run',
            "release/extlibs/xml.$kind/test/boundaries.xl")
        $null = Run "$kind XLang allocations" $xl @('run',
            "release/extlibs/xml.$kind/test/allocations.xl")
    }

    ExpectPrivate 'DOM native entry' `
        'release/extlibs/xml.dom/test/private_native_negative.xl' `
        'createElementNative' 'access'
    ExpectPrivate 'DOM pointer entry' `
        'release/extlibs/xml.dom/test/private_pointer_negative.xl' `
        'appendChildNative' 'access'
    ExpectPrivate 'ElementTree constructor' `
        'release/extlibs/xml.etree/test/constructor_access_negative.xl' `
        'no such exported symbol' '[import]'
    ExpectPrivate 'ElementTree native entry' `
        'release/extlibs/xml.etree/test/private_native_negative.xl' `
        'setTextNative' 'access'
    ExpectPrivate 'ElementTree pointer entry' `
        'release/extlibs/xml.etree/test/private_pointer_negative.xl' `
        'nameNative' 'access'

    $goldenByKind = @{}
    foreach ($kind in @('sax', 'dom', 'etree')) {
        $goldenPath = Join-Path $root "release/extlibs/xml.sax/test/xml_$kind.expected.txt"
        $golden = [IO.File]::ReadAllText($goldenPath,
            [Text.UTF8Encoding]::new($false)).Replace("`r`n", "`n").TrimEnd("`n")
        $goldenByKind[$kind] = $golden
        $cOutput = Run "$kind C example" $xl @('run',
            "release/examples/extlibs/xml_${kind}_c.c")
        $xlOutput = Run "$kind XLang example" $xl @('run',
            "release/examples/extlibs/xml_$kind.xl")
        if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
            throw "xml.$kind examples differ from fixed output:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host "[xml] $kind fixed C/XLang output: PASS"
    }
    if (-not $SkipAot) {
        foreach ($kind in @('sax', 'dom', 'etree')) {
            $target = Join-Path $work "xml_${kind}_aot.exe"
            $null = Run "$kind XLang AOT build" $xl @('build',
                "release/examples/extlibs/xml_$kind.xl", '-o', $target)
            $aotOutput = Run "$kind XLang AOT" $target @()
            if ($aotOutput -cne $goldenByKind[$kind]) {
                throw "xml.$kind AOT output differs from fixed output"
            }
        }
    }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($workRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe XML trees work directory: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}

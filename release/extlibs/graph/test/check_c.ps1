$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$parent = Join-Path $root 'build/test/graph_c_guard'
$work = Join-Path $parent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/graph/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[graph C] ${name}: PASS"
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    Run 'XRT runtime compile' $gcc ($flags + @('-c',
        'test/support/extlib_graph_runtime.c', '-o', $runtime))
    $manifest = Get-Content 'release/extlibs/graph/xlang.package.json' -Raw |
        ConvertFrom-Json
    $objects = @()
    foreach ($source in $manifest.artifact.cSources) {
        $object = Join-Path $work "native_$($objects.Count).o"
        Run "native $source compile" $gcc ($flags + @('-c',
            "release/extlibs/graph/$source", '-o', $object))
        $objects += $object
    }
    foreach ($name in @('test_graph', 'test_graph_alloc', 'test_graph_threads')) {
        $target = Join-Path $work "$name.exe"
        Run "$name compile" $gcc ($flags + @('-DNDEBUG') + @(
            "release/extlibs/graph/test/$name.c") + $objects +
            @($runtime) + $links + @('-o', $target))
        Run $name $target @()
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($parent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "graph C test path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}

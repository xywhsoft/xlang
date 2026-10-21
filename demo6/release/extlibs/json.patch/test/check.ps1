param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/json_patch_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-I', 'release/extlibs/json.patch/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$native = 'release/extlibs/json.patch/json_patch_native.c'

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $ErrorActionPreference = 'Continue'
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[json.patch] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $runtimeSource = 'release/extlibs/json.patch/test/xrt_runtime.c'
    $null = Run 'XRT compile' $gcc ($flags + @('-c', $runtimeSource,
        '-o', $runtime))
    $test = Join-Path $work 'boundaries.exe'
    $null = Run 'C boundaries compile' $gcc ($flags + @($native,
        'release/extlibs/json.patch/test/test_boundaries.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C boundaries' $test @()
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/json.patch/examples/basic_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/json.patch/examples/basic_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }
    $workflow = Join-Path $work 'workflow.exe'
    $null = Run 'C workflow compile' $gcc ($flags + @(
        'release/extlibs/json.patch/examples/workflow_c.c', $runtime) +
        $links + @('-o', $workflow))
    $cWorkflow = Run 'C workflow' $workflow @()
    $directWorkflow = Run 'C workflow direct xl run' $xl @('run',
        'release/extlibs/json.patch/examples/workflow_c.c')
    if ($directWorkflow -cne $cWorkflow) {
        throw 'direct C workflow output differs'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run',
            'extlibs/json.patch/test/boundaries.xl')
        $null = Run 'XLang allocation faults' $xl @('run',
            'extlibs/json.patch/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run',
            'extlibs/json.patch/examples/basic.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang output differs:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[json.patch] C/XLang parity: PASS'
        $xlWorkflow = Run 'XLang workflow' $xl @('run',
            'extlibs/json.patch/examples/workflow.xl')
        if ($cWorkflow -cne $xlWorkflow -or
            $xlWorkflow -cne 'json.patch workflow: ok') {
            throw "C/XLang workflow differs:`nC:`n$cWorkflow`nXL:`n$xlWorkflow"
        }
        Write-Host '[json.patch] C/XLang workflow parity: PASS'
        $null = Run 'Public example' $xl @('run',
            'examples/extlibs/json_patch.xl')
        if (-not $SkipAot) {
            $target = Join-Path $work 'boundaries_aot.exe'
            $null = Run 'XLang AOT build' $xl @('build',
                'extlibs/json.patch/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang AOT' $target @()
            $basicAot = Join-Path $work 'basic_aot.exe'
            $null = Run 'XLang example AOT build' $xl @('build',
                'extlibs/json.patch/examples/basic.xl', '-o', $basicAot)
            $basicAotOutput = Run 'XLang example AOT' $basicAot @()
            if ($basicAotOutput -cne $cOutput) {
                throw 'XLang AOT example output differs from C'
            }
            $workflowAot = Join-Path $work 'workflow_aot.exe'
            $null = Run 'XLang workflow AOT build' $xl @('build',
                'extlibs/json.patch/examples/workflow.xl', '-o', $workflowAot)
            $workflowAotOutput = Run 'XLang workflow AOT' $workflowAot @()
            if ($workflowAotOutput -cne $cWorkflow -or
                $workflowAotOutput -cne 'json.patch workflow: ok') {
                throw 'XLang AOT workflow output differs from C or fixed result'
            }
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
        throw "json.patch temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}

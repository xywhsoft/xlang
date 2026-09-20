param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$tempRoot = [System.IO.Path]::GetFullPath((Join-Path $Root "release\tmp"))
$trimSourcePath = Join-Path $Root "src\compile\xl6_xrt_trim.c"

foreach ( $requiredPath in @($xl, $trimSourcePath) ) {
    if ( ! (Test-Path -LiteralPath $requiredPath -PathType Leaf) ) {
        throw "product trim contract input is missing: $requiredPath"
    }
}
$trimSource = Get-Content -LiteralPath $trimSourcePath -Raw
if ( $trimSource -match 'xl6XrtTrimNeedsRuntimeProduct' -or
    $trimSource -match '#define XRT_IMPLEMENTATION' -or
    $trimSource -match '#include \\"xrt[.]h\\"' ) {
    throw "xlang compiler retained a second core-only implementation path"
}
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$cases = @(
    @{
        Name = "minimal"
        Source = "test\cases\cgen_basic.xl"
        Required = @("XRT_MODULE_VALUE_GRAPH", "XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE")
        Forbidden = @("XRUNTIME_MODULE_TYPED_")
    },
    @{
        Name = "typed_set"
        Source = "test\cases\cgen_nested_typed_literal_trim.xl"
        Required = @("XRUNTIME_MODULE_TYPED_SET_VALUE")
        Forbidden = @()
    }
)

foreach ( $case in $cases ) {
    $exe = Join-Path $outputDir ("xrt_product_trim_{0}.exe" -f $case.Name)
    $stdout = Join-Path $outputDir ("xrt_product_trim_{0}.out.txt" -f $case.Name)
    $stderr = Join-Path $outputDir ("xrt_product_trim_{0}.err.txt" -f $case.Name)
    $runStdout = Join-Path $outputDir ("xrt_product_trim_{0}.run.out.txt" -f $case.Name)
    $runStderr = Join-Path $outputDir ("xrt_product_trim_{0}.run.err.txt" -f $case.Name)
    $tempDir = $null

    try {
        foreach ( $path in @($exe, $stdout, $stderr, $runStdout, $runStderr) ) {
            if ( Test-Path -LiteralPath $path -PathType Leaf ) {
                Remove-Item -LiteralPath $path -Force
            }
        }

        $process = Start-Process `
            -FilePath $xl `
            -ArgumentList @(
                "build",
                (Join-Path $Root $case.Source),
                "--save-temps",
                "-o",
                $exe
            ) `
            -WorkingDirectory $Root `
            -NoNewWindow `
            -PassThru `
            -Wait `
            -RedirectStandardOutput $stdout `
            -RedirectStandardError $stderr
        if ( $process.ExitCode -ne 0 ) {
            throw "xlang build failed for $($case.Name): $(Get-Content -LiteralPath $stderr -Raw)"
        }

        $buildLog = (Get-Content -LiteralPath $stdout -Raw) +
            (Get-Content -LiteralPath $stderr -Raw)
        $match = [regex]::Match($buildLog, "temporary files kept:\s*(.+)")
        if ( ! $match.Success ) {
            throw "xlang build did not report its retained temp directory: $($case.Name)"
        }
        $tempDir = [System.IO.Path]::GetFullPath($match.Groups[1].Value.Trim())
        if ( ! $tempDir.StartsWith($tempRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) ) {
            throw "xlang temp directory escaped the release temp root: $tempDir"
        }

        $implementationPath = Join-Path $tempDir "xl6_xrt_impl.c"
        if ( ! (Test-Path -LiteralPath $implementationPath -PathType Leaf) ) {
            throw "generated runtime product source is missing: $implementationPath"
        }
        $implementation = Get-Content -LiteralPath $implementationPath -Raw
        if ( $implementation -notmatch "(?m)^#define XRUNTIME_IMPLEMENTATION\r?$" -or
            $implementation -notmatch "(?m)^#include `"xruntime[.]h`"\r?$" ) {
            throw "generated program did not select the xruntime product: $($case.Name)"
        }
        if ( $implementation -match "(?m)^#define XRT_IMPLEMENTATION\r?$" -or
            $implementation -match "(?m)^#include `"xrt[.]h`"\r?$" ) {
            throw "generated program compiled a second XRT implementation: $($case.Name)"
        }
        foreach ( $required in $case.Required ) {
            if ( $implementation -notmatch [regex]::Escape($required) ) {
                throw "generated program is missing required module ${required}: $($case.Name)"
            }
        }
        foreach ( $forbidden in $case.Forbidden ) {
            if ( $implementation -match [regex]::Escape($forbidden) ) {
                throw "generated program retained forbidden module family ${forbidden}: $($case.Name)"
            }
        }

        $run = Start-Process `
            -FilePath $exe `
            -WorkingDirectory $Root `
            -NoNewWindow `
            -PassThru `
            -Wait `
            -RedirectStandardOutput $runStdout `
            -RedirectStandardError $runStderr
        if ( $run.ExitCode -ne 0 ) {
            throw "generated program failed for $($case.Name): $($run.ExitCode) $(Get-Content -LiteralPath $runStderr -Raw)"
        }
    } finally {
        if ( $null -ne $tempDir -and
            (Test-Path -LiteralPath $tempDir -PathType Container) -and
            $tempDir.StartsWith($tempRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) ) {
            Remove-Item -LiteralPath $tempDir -Recurse -Force
        }
        foreach ( $path in @($exe, $stdout, $stderr, $runStdout, $runStderr) ) {
            if ( Test-Path -LiteralPath $path -PathType Leaf ) {
                Remove-Item -LiteralPath $path -Force
            }
        }
    }
}

Write-Host "XRT/xruntime product trim contract ok: $($cases.Count) representative programs"
exit 0

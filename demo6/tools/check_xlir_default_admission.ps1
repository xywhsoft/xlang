param(
    [string]$Root = "",
    [int]$MinimumSourceCount = 531,
    # xlir_scope_exit_plan adds a default-backend candidate but its nested
    # generic call is XLIR-only. The return_commit fixture adds one supported
    # legacy intersection source; keep that distinction in the floors.
    [int]$MinimumLegacySuccess = 514,
    [int]$MinimumXlirSuccess = 514,
    [int]$MaximumAdmissionGaps = 0,
    [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$compiler = Join-Path $Root "release\xl.exe"
$compilerDll = Join-Path $Root "release\xlc.dll"
$suiteDir = Join-Path $Root "test\suites"
$tempDir = Join-Path $Root "build\tmp"
$legacyOutput = Join-Path $tempDir "xlir_default_admission_legacy.c"
$xlirOutput = Join-Path $tempDir "xlir_default_admission_xlir.c"
if ( $ReportPath -eq "" ) {
    $ReportPath = Join-Path $tempDir "xlir_default_admission.results.json"
}

foreach ( $path in @($compiler, $compilerDll, $suiteDir) ) {
    if ( ! (Test-Path -LiteralPath $path) ) {
        throw "XLIR default admission input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($tempDir) | Out-Null
$compilerHashes = @((Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash,
    (Get-FileHash -LiteralPath $compilerDll -Algorithm SHA256).Hash)

function Invoke-Cgen {
    param(
        [string]$Source,
        [string]$Backend,
        [string]$Output
    )

    $savedErrorActionPreference = $ErrorActionPreference
    $savedPackagePath = $env:XLANG_PACKAGE_PATH
    Push-Location $Root
    try {
        # Match suites/common.bat even when this guard runs standalone.
        $env:XLANG_PACKAGE_PATH = (Join-Path $Root "release\extlibs") + ";" +
            (Join-Path $Root "test\fixtures")
        # A failed invocation must never inherit a previous source's artifact.
        if ( Test-Path -LiteralPath $Output ) {
            Remove-Item -LiteralPath $Output
        }
        $ErrorActionPreference = "Continue"
        $global:LASTEXITCODE = $null
        $text = @(& $compiler "cgen" $Source "--backend" $Backend "--emit" $Output 2>&1)
        $exitCode = $global:LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
        $env:XLANG_PACKAGE_PATH = $savedPackagePath
        Pop-Location
    }
    $diagnostic = ($text -join "`n").Trim()
    $artifactExists = (Test-Path -LiteralPath $Output -PathType Leaf) -and
        (Get-Item -LiteralPath $Output).Length -gt 0
    $executionFailure = ""
    if ( $null -eq $exitCode -or $exitCode -notin @(0, 1) ) {
        $executionFailure = "unexpected compiler exit code: $exitCode"
    } elseif ( $exitCode -eq 1 -and $diagnostic -eq "" ) {
        $executionFailure = "compiler rejected input without a diagnostic"
    } elseif ( $exitCode -eq 0 -and ! $artifactExists ) {
        $executionFailure = "compiler succeeded without a generated C artifact"
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $diagnostic
        ExecutionFailure = $executionFailure
    }
}

$sources = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$generatedSources = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
# All three production commands default to XLIR.  Cgen-only extraction misses
# runtime/build-only fixtures and cannot prove the default backend's admission
# boundary.  Code generation is sufficient for this gate: suite hosts still
# own linkage and observable runtime behavior.
$pattern = [regex]'(?i)release\\xl\.exe\s+(?:cgen|run|build)\s+(?:"([^"]+\.xl)"|([^\s>]+\.xl))'

Get-ChildItem -LiteralPath $suiteDir -Filter "*.bat" -File | ForEach-Object {
    foreach ( $line in [System.IO.File]::ReadAllLines($_.FullName) ) {
        if ( $line -match '(?i)--backend\s+xlir' ) {
            continue
        }
        $match = $pattern.Match($line)
        if ( ! $match.Success ) {
            continue
        }
        $source = if ( $match.Groups[1].Success ) {
            $match.Groups[1].Value
        } else {
            $match.Groups[2].Value
        }
        $source = $source.Replace('/', '\')
        if ( $source -match '^(?i:build|tmp)\\' ) {
            # These fixtures are produced by their owning suite. An old temp
            # artifact must not become part of the durable source baseline.
            [void]$generatedSources.Add($source)
            continue
        }
        if ( Test-Path -LiteralPath (Join-Path $Root $source) -PathType Leaf ) {
            [void]$sources.Add($source)
        }
    }
}

$legacySuccess = 0
$xlirSuccess = 0
$gaps = [System.Collections.Generic.List[object]]::new()
$legacyFailures = [System.Collections.Generic.List[object]]::new()
$executionFailures = [System.Collections.Generic.List[object]]::new()
$results = [System.Collections.Generic.List[object]]::new()
foreach ( $source in @($sources | Sort-Object) ) {
    $legacy = Invoke-Cgen -Source $source -Backend "legacy" -Output $legacyOutput
    $xlir = Invoke-Cgen -Source $source -Backend "xlir" -Output $xlirOutput
    $results.Add([pscustomobject]@{
        Source = $source
        LegacyExitCode = $legacy.ExitCode
        XlirExitCode = $xlir.ExitCode
    })
    foreach ( $invocation in @(
        [pscustomobject]@{ Backend = "legacy"; Result = $legacy },
        [pscustomobject]@{ Backend = "xlir"; Result = $xlir }) ) {
        if ( $invocation.Result.ExecutionFailure -ne "" ) {
            $executionFailures.Add([pscustomobject]@{
                Source = $source
                Backend = $invocation.Backend
                Diagnostic = $invocation.Result.ExecutionFailure
                Output = $invocation.Result.Output
            })
        }
    }
    if ( $legacy.ExecutionFailure -ne "" -or $xlir.ExecutionFailure -ne "" ) {
        continue
    }
    if ( $legacy.ExitCode -ne 0 ) {
        $diagnostic = @($legacy.Output -split "`r?`n" |
            Where-Object { $_.Trim() }) | Select-Object -Last 1
        if ( $null -eq $diagnostic ) {
            $diagnostic = "legacy cgen exited without a diagnostic"
        }
        $legacyFailures.Add([pscustomobject]@{
            Source = $source
            Diagnostic = $diagnostic.Trim()
        })
        continue
    }
    $legacySuccess += 1
    if ( $xlir.ExitCode -eq 0 ) {
        $xlirSuccess += 1
        continue
    }

    $diagnostic = @($xlir.Output -split "`r?`n" | Where-Object { $_.Trim() }) |
        Select-Object -Last 1
    if ( $null -eq $diagnostic ) {
        $diagnostic = "XLIR cgen exited without a diagnostic"
    }
    $kind = $diagnostic.Trim() -replace '^.*?:\d+:\d+:\s*', ''
    $gaps.Add([pscustomobject]@{
        Source = $source
        Kind = $kind
    })
}

# A concurrent rebuild invalidates a cross-backend admission comparison, even
# when every individual process returned a normal exit code.
$compilerStable = (Test-Path -LiteralPath $compiler) -and
    (Test-Path -LiteralPath $compilerDll)
if ( $compilerStable ) {
    $compilerStable = (Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash -eq
        $compilerHashes[0] -and
        (Get-FileHash -LiteralPath $compilerDll -Algorithm SHA256).Hash -eq
        $compilerHashes[1]
}

$summary = "XLIR default admission: sources={0}, legacy-ok={1}, xlir-ok={2}, gaps={3}" -f
    $sources.Count, $legacySuccess, $xlirSuccess, $gaps.Count
[System.IO.Directory]::CreateDirectory(
    [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($ReportPath))) | Out-Null
$report = [ordered]@{
    Commands = @("cgen", "run", "build")
    SourceCount = $sources.Count
    LegacySuccess = $legacySuccess
    XlirSuccess = $xlirSuccess
    GapCount = $gaps.Count
    ExecutionFailureCount = $executionFailures.Count
    CompilerStable = $compilerStable
    CompilerSha256 = $compilerHashes[0]
    CompilerDllSha256 = $compilerHashes[1]
    Sources = @($sources | Sort-Object)
    SuiteOwnedGeneratedSources = @($generatedSources | Sort-Object)
    Gaps = @($gaps.ToArray())
    LegacyFailures = @($legacyFailures.ToArray())
    ExecutionFailures = @($executionFailures.ToArray())
    Results = @($results.ToArray())
}
[System.IO.File]::WriteAllText(
    $ReportPath, ($report | ConvertTo-Json -Depth 6),
    [System.Text.UTF8Encoding]::new($false))
Write-Output $summary
if ( $legacyFailures.Count -gt 0 ) {
    Write-Output "  legacy baseline failures:"
    $legacyFailures | ForEach-Object {
        Write-Output ("    {0} [{1}]" -f $_.Diagnostic, $_.Source)
    }
}
$gaps | Group-Object Kind | Sort-Object Count -Descending | ForEach-Object {
    $examples = @($_.Group | Select-Object -First 3 -ExpandProperty Source) -join ", "
    Write-Output ("  {0,3}  {1} [{2}]" -f $_.Count, $_.Name, $examples)
}
$executionFailures | Select-Object -First 10 | ForEach-Object {
    Write-Output ("  execution failure: {0} [{1}, {2}]" -f
        $_.Diagnostic, $_.Backend, $_.Source)
}
if ( $executionFailures.Count -gt 10 ) {
    Write-Output "  additional execution failures are recorded in $ReportPath"
}

if ( ! $compilerStable ) {
    throw "XLIR default admission compiler changed during the audit; results are invalid"
}
if ( $executionFailures.Count -gt 0 ) {
    throw "XLIR default admission encountered $($executionFailures.Count) compiler execution failures"
}

if ( $sources.Count -lt $MinimumSourceCount ) {
    throw "XLIR default admission source coverage regressed below $MinimumSourceCount"
}
if ( $legacySuccess -lt $MinimumLegacySuccess ) {
    throw "XLIR default admission legacy baseline regressed below $MinimumLegacySuccess"
}
if ( $xlirSuccess -lt $MinimumXlirSuccess ) {
    throw "XLIR default admission success count regressed below $MinimumXlirSuccess"
}
if ( $gaps.Count -gt $MaximumAdmissionGaps ) {
    throw "XLIR default admission gap count regressed above $MaximumAdmissionGaps"
}

exit 0

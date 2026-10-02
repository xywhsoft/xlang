param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$toolsDir = Join-Path $Root "tools"
$toolsReadme = Join-Path $toolsDir "README.md"
$testBat = Join-Path $Root "test.bat"
$testSuiteDir = Join-Path $Root "test\suites"
$rc0Guard = Join-Path $toolsDir "rc0_guard.ps1"
$minCheckScripts = 28
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Normalize-ToolPath($path)
{
    return $path.Replace("/", "\").ToLowerInvariant()
}

function Get-TestRegressionText()
{
    $parts = New-Object System.Collections.Generic.List[string]
    $parts.Add((Read-Utf8Text $script:testBat)) | Out-Null

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $parts.Add((Read-Utf8Text $suite.FullName)) | Out-Null
        }
    }

    return [string]::Join("`n", $parts)
}

if ( ! (Test-Path -LiteralPath $toolsDir -PathType Container) ) {
    Write-Host "tools directory missing: $toolsDir"
    exit 1
}

if ( ! (Test-Path -LiteralPath $toolsReadme -PathType Leaf) ) {
    Write-Host "tools README missing: $toolsReadme"
    exit 1
}

if ( ! (Test-Path -LiteralPath $testBat -PathType Leaf) ) {
    Write-Host "test.bat missing: $testBat"
    exit 1
}

if ( ! (Test-Path -LiteralPath $rc0Guard -PathType Leaf) ) {
    Write-Host "rc0 guard missing: $rc0Guard"
    exit 1
}

$testText = Get-TestRegressionText
$rc0Text = Read-Utf8Text $rc0Guard
$toolsReadmeText = Read-Utf8Text $toolsReadme
$trackedCheckScripts = @()
$retiredCheckScripts = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
foreach ( $relative in @(
    'tools\check_typed_hir_contract.ps1',
    'tools\check_xlir_cgen_differential.ps1'
) ) {
    $retiredCheckScripts.Add((Normalize-ToolPath $relative)) | Out-Null
}
$gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
if ( $null -ne $gitCommand ) {
    $trackedCheckScripts = @(& $gitCommand.Source -c "core.quotepath=false" -C $Root `
        ls-files -- "tools/check_*.ps1" 2>$null)
}
if ( $null -ne $gitCommand -and $LASTEXITCODE -eq 0 -and
     $trackedCheckScripts.Count -gt 0 ) {
    $checkScripts = New-Object System.Collections.Generic.List[object]
    foreach ( $relative in $trackedCheckScripts ) {
        $normalized = Normalize-ToolPath $relative
        if ( $retiredCheckScripts.Contains($normalized) ) {
            continue
        }
        $path = Join-Path $Root $relative
        if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
            Add-Failure "versioned check script is missing: $relative"
            continue
        }
        $checkScripts.Add((Get-Item -LiteralPath $path)) | Out-Null
    }
    $checkScripts = @($checkScripts | Sort-Object Name)
} else {
    # Source archives have no Git index; every packaged check script is then
    # part of the product inventory.
    $checkScripts = @(Get-ChildItem -LiteralPath $toolsDir -Filter "check_*.ps1" -File |
        Sort-Object Name)
}

foreach ( $relative in $retiredCheckScripts ) {
    $path = Join-Path $Root $relative
    $name = [System.IO.Path]::GetFileName($relative)
    if ( Test-Path -LiteralPath $path ) {
        Add-Failure "retired backend check script returned: $relative"
    }
    if ( $toolsReadmeText.Contains($name) -or $testText.Contains($name) -or $rc0Text.Contains($name) ) {
        Add-Failure "retired backend check script is still wired or documented: $relative"
    }
}
$xlirRunner = Join-Path $toolsDir "run_xlir_guards.ps1"
$xlirParameterizedRunner = Join-Path $toolsDir "run_xlir_parameterized_guards.ps1"
$fullRegressionHasXlirRunner = $testText -match 'tools[\\/]run_xlir_guards\.ps1\s+-Mode\s+FullRegressionSupplement'
$rc0HasXlirRunner = $rc0Text -match 'tools[\\/]run_xlir_guards\.ps1`?",\s*`?"-Mode`?",\s*`?"Rc0Supplement'
$testWiredScripts = New-Object System.Collections.Generic.HashSet[string]
$rc0WiredScripts = New-Object System.Collections.Generic.HashSet[string]
$fullRegressionOnly = New-Object System.Collections.Generic.HashSet[string]
$fullRegressionOnly.Add((Normalize-ToolPath "tools\check_stdlib_examples.ps1")) | Out-Null
# These current-source/JIT/AOT/OOM gates remain mandatory in full regression;
# the lightweight RC0 static/smoke gate does not duplicate their heavy builds.
foreach ($name in @('check_stdlib_logging.ps1','check_stdlib_text_values.ps1',
    'check_stdlib_time_core.ps1','check_stdlib_time_calendar.ps1')) {
    $fullRegressionOnly.Add((Normalize-ToolPath ("tools\" + $name))) | Out-Null
}

if ( $checkScripts.Count -lt $minCheckScripts ) {
    Add-Failure "check script baseline regressed: $($checkScripts.Count) < $minCheckScripts"
}

if ( ! $toolsReadmeText.Contains("tools/check_*.ps1") ) {
    Add-Failure "tools README does not declare the canonical check-script inventory"
}

if ( ! $toolsReadmeText.Contains("check_tool_wiring.ps1") ) {
    Add-Failure "tools README does not mention the exhaustive wiring guard"
}

if ( ! (Test-Path -LiteralPath $xlirRunner -PathType Leaf) ) {
    Add-Failure "central XLIR guard runner is missing: tools\run_xlir_guards.ps1"
}

if ( ! (Test-Path -LiteralPath $xlirParameterizedRunner -PathType Leaf) ) {
    Add-Failure "parameterized XLIR scenario runner is missing: tools\run_xlir_parameterized_guards.ps1"
} elseif ( (Read-Utf8Text $xlirRunner) -notmatch 'run_xlir_parameterized_guards\.ps1' ) {
    Add-Failure "central XLIR guard runner does not invoke the parameterized scenario runner"
}

if ( ! $fullRegressionHasXlirRunner ) {
    Add-Failure "full regression does not invoke the central XLIR guard runner"
}

if ( ! $rc0HasXlirRunner ) {
    Add-Failure "rc0_guard.ps1 does not invoke the central XLIR guard runner"
}

$parameterizedInventory = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
if ( Test-Path -LiteralPath $xlirParameterizedRunner -PathType Leaf ) {
    $listed = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $xlirParameterizedRunner -Root $Root -ListOnly)
    if ( $LASTEXITCODE -ne 0 ) {
        Add-Failure "parameterized XLIR scenario inventory cannot be loaded"
    } else {
        foreach ( $name in $listed ) { $parameterizedInventory.Add([string]$name) | Out-Null }
    }
}

foreach ( $match in [regex]::Matches($testText, 'tools[\\/](check_[^"''\s\^<>|&]+\.ps1)') ) {
    $relative = Normalize-ToolPath ("tools\" + $match.Groups[1].Value)
    $testWiredScripts.Add($relative) | Out-Null

    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "test regression entry references missing check script: $relative"
    }
}

foreach ( $match in [regex]::Matches($rc0Text, 'check_[A-Za-z0-9_]+\.ps1') ) {
    $relative = Normalize-ToolPath ("tools\" + $match.Value)
    $rc0WiredScripts.Add($relative) | Out-Null

    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "rc0_guard.ps1 references missing check script: $relative"
    }
}

foreach ( $script in $checkScripts ) {
    $relative = Normalize-ToolPath ("tools\" + $script.Name)
    $isXlirGuard = $script.Name.StartsWith("check_xlir_", [System.StringComparison]::OrdinalIgnoreCase)

    if ( $isXlirGuard ) {
        $tokens = $null
        $parseErrors = $null
        $ast = [Management.Automation.Language.Parser]::ParseFile(
            $script.FullName, [ref]$tokens, [ref]$parseErrors)
        if ( $parseErrors.Count ) {
            Add-Failure "XLIR guard cannot be parsed for wiring: $relative"
        } else {
            $required = @($ast.ParamBlock.Parameters | Where-Object {
                @($_.Attributes.NamedArguments | Where-Object {
                    $_.ArgumentName -eq "Mandatory" -and
                    ($null -eq $_.Argument -or $_.Argument.Extent.Text -match '^\$true$')
                }).Count -ne 0
            })
            if ( $required.Count -and ! $parameterizedInventory.Contains($script.Name) ) {
                Add-Failure "parameterized XLIR guard lacks a reviewed scenario invocation: $relative"
            }
            if ( !$required.Count -and $parameterizedInventory.Contains($script.Name) ) {
                Add-Failure "standalone XLIR guard is incorrectly listed as parameterized: $relative"
            }
        }
    }

    if ( ! $testWiredScripts.Contains($relative) -and ! ($isXlirGuard -and $fullRegressionHasXlirRunner) ) {
        Add-Failure "check script is not wired into test.bat/test suites: $relative"
    }

    if ( ! $rc0WiredScripts.Contains($relative) -and ! ($isXlirGuard -and $rc0HasXlirRunner) -and ! $fullRegressionOnly.Contains($relative) ) {
        Add-Failure "check script is not wired into rc0_guard.ps1 or a full-regression-only exception: $relative"
    }
}

foreach ( $relative in $fullRegressionOnly ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "full-regression-only check script is missing: $relative"
    }
    if ( ! $testWiredScripts.Contains($relative) ) {
        Add-Failure "full-regression-only check script is not wired into test.bat/test suites: $relative"
    }
}

if ( ! $toolsReadmeText.Contains("xl6_toolchain_env.bat") ) {
    Add-Failure "tools README does not mention tools\xl6_toolchain_env.bat"
}

if ( ! $toolsReadmeText.Contains("rc0_guard.ps1") ) {
    Add-Failure "tools README does not mention tools\rc0_guard.ps1"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Tool wiring ok: $($checkScripts.Count) check scripts; $($parameterizedInventory.Count) XLIR workers have reviewed scenario inputs."
exit 0

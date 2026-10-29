param(
    [ValidateSet("FullRegressionSupplement", "Rc0Supplement")]
    [string]$Mode = "FullRegressionSupplement",
    [string]$Root = "",
    [string]$StartAt = "",
    [string]$ReportPath = "",
    [string]$NativeRoot = "",
    [string]$Tcc = "",
    [ValidateRange(1, 8)][int]$MaxParallel = $(if ($env:XL6_TEST_MAX_PARALLEL) { [int]$env:XL6_TEST_MAX_PARALLEL } else { 3 }),
    [switch]$ListOnly
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$toolsDir = Join-Path $Root "tools"
$wiringFiles = New-Object System.Collections.Generic.List[string]

if ( $Mode -eq "FullRegressionSupplement" ) {
    $wiringFiles.Add((Join-Path $Root "test.bat")) | Out-Null
    $suiteDir = Join-Path $Root "test\suites"
    foreach ( $file in @(Get-ChildItem -LiteralPath $suiteDir -Filter "*.bat" -File | Sort-Object Name) ) {
        $wiringFiles.Add($file.FullName) | Out-Null
    }
}
else {
    $wiringFiles.Add((Join-Path $toolsDir "rc0_guard.ps1")) | Out-Null
}

$explicit = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
foreach ( $file in $wiringFiles ) {
    if ( ! (Test-Path -LiteralPath $file -PathType Leaf) ) {
        throw "XLIR guard wiring source is missing: $file"
    }

    $text = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
    foreach ( $match in [regex]::Matches($text, 'check_xlir_[A-Za-z0-9_]+\.ps1') ) {
        $explicit.Add($match.Value) | Out-Null
    }
}

$allGuards = @(Get-ChildItem -LiteralPath $toolsDir -Filter "check_xlir_*.ps1" -File | Sort-Object Name)
$pending = @($allGuards | Where-Object { ! $explicit.Contains($_.Name) })
function Get-RequiredParameters([string]$Path) {
    $tokens = $null
    $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile(
        $Path, [ref]$tokens, [ref]$parseErrors)
    if ( $parseErrors.Count ) {
        throw "Cannot classify XLIR guard parameters: $Path ($($parseErrors[0].Message))"
    }

    $required = New-Object System.Collections.Generic.List[string]
    foreach ( $parameter in $ast.ParamBlock.Parameters ) {
        foreach ( $attribute in $parameter.Attributes ) {
            foreach ( $argument in $attribute.NamedArguments ) {
                if ( $argument.ArgumentName -ne "Mandatory" ) { continue }
                if ( $null -eq $argument.Argument -or
                     $argument.Argument.Extent.Text -match '^\$true$' ) {
                    $required.Add($parameter.Name.VariablePath.UserPath) | Out-Null
                }
            }
        }
    }
    return @($required | Sort-Object -Unique)
}

$pendingInventory = @($pending | ForEach-Object {
    [pscustomobject]@{
        File = $_
        RequiredParameters = @(Get-RequiredParameters $_.FullName)
    }
})
$standalone = @($pendingInventory | Where-Object { $_.RequiredParameters.Count -eq 0 })
$parameterized = @($pendingInventory | Where-Object { $_.RequiredParameters.Count -ne 0 })
$selected = @($standalone)

if ( $StartAt -ne "" ) {
    $startIndex = -1
    for ( $index = 0; $index -lt $standalone.Count; $index++ ) {
        if ( $standalone[$index].File.Name.Equals($StartAt, [System.StringComparison]::OrdinalIgnoreCase) ) {
            $startIndex = $index
            break
        }
    }
    if ( $startIndex -lt 0 ) {
        throw "XLIR standalone supplemental start gate is not pending in ${Mode}: $StartAt"
    }
    $selected = @($standalone | Select-Object -Skip $startIndex)
}

Write-Host "XLIR guard inventory: $($allGuards.Count) total, $($explicit.Count) explicit, $($standalone.Count) standalone supplemental, $($parameterized.Count) parameterized workers, $($selected.Count) selected ($Mode)."
if ( $parameterized.Count ) {
    Write-Host "Parameterized XLIR workers require an explicit scenario runner and are not invoked without their declared inputs."
}

if ( $ListOnly ) {
    foreach ( $record in $selected ) {
        Write-Host $record.File.Name
    }
    foreach ( $record in $parameterized ) {
        Write-Host "parameterized|$($record.File.Name)|$($record.RequiredParameters -join ',')"
    }
    exit 0
}

$runReport = [ordered]@{
    Mode = $Mode
    StartAt = $StartAt
    StartedUtc = [DateTime]::UtcNow.ToString("o")
    CompletedUtc = $null
    TotalGuards = $allGuards.Count
    ExplicitGuards = $explicit.Count
    SupplementalGuards = $pending.Count
    StandaloneSupplementalGuards = $standalone.Count
    ParameterizedGuards = @($parameterized | ForEach-Object {
        [ordered]@{ Guard = $_.File.Name; RequiredParameters = @($_.RequiredParameters) }
    })
    ParameterizedReport = $null
    SelectedGuards = $selected.Count
    Status = "running"
    Results = @()
}
if ( $ReportPath -eq "" ) {
    $ReportPath = Join-Path $Root "build\tmp\xlir_guards.$Mode.results.json"
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)
[System.IO.Directory]::CreateDirectory(
    [System.IO.Path]::GetDirectoryName($ReportPath)) | Out-Null
function Save-RunReport {
    [System.IO.File]::WriteAllText(
        $ReportPath, ($runReport | ConvertTo-Json -Depth 6),
        [System.Text.UTF8Encoding]::new($false))
}
Save-RunReport

$previousLocation = Get-Location
try {
    Set-Location $Root
    for ( $index = 0; $index -lt $selected.Count; $index++ ) {
        $guard = $selected[$index].File
        Write-Host "[$($index + 1)/$($selected.Count)] $($guard.Name)"
        $result = [pscustomobject]@{
            Guard = $guard.Name
            ScriptSha256 = (Get-FileHash -LiteralPath $guard.FullName -Algorithm SHA256).Hash
            StartedUtc = [DateTime]::UtcNow.ToString("o")
            CompletedUtc = $null
            Status = "running"
            ExitCode = $null
        }
        $runReport.Results += $result
        Save-RunReport
        $previousPreference = $ErrorActionPreference
        try {
            # Native stderr is diagnostic data, not an early-exit signal.
            $ErrorActionPreference = 'Continue'
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $guard.FullName
            $result.ExitCode = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        $result.CompletedUtc = [DateTime]::UtcNow.ToString("o")
        $result.Status = if ( $result.ExitCode -eq 0 ) { "passed" } else { "failed" }
        Save-RunReport
        if ( $result.ExitCode -ne 0 ) {
            Write-Host "FAIL: XLIR guard exited $($result.ExitCode): $($guard.Name)"
        }
    }
    if ( $parameterized.Count ) {
        $scenarioRoot = Join-Path $Root ("build\tmp\xlir_parameterized_{0}_{1}" -f
            $Mode, [Guid]::NewGuid().ToString('N').Substring(0, 8))
        $scenarioArguments = @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
            (Join-Path $toolsDir 'run_xlir_parameterized_guards.ps1'),
            '-Root', $Root, '-OutputRoot', $scenarioRoot,
            '-MaxParallel', [string]$MaxParallel
        )
        if ( $NativeRoot ) { $scenarioArguments += @('-NativeRoot', $NativeRoot) }
        if ( $Tcc ) { $scenarioArguments += @('-Tcc', $Tcc) }
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & powershell.exe @scenarioArguments
            $scenarioExitCode = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previousPreference }
        $runReport.ParameterizedReport = Join-Path $scenarioRoot 'results.json'
        Save-RunReport
        if ( $scenarioExitCode -ne 0 ) {
            throw "XLIR parameterized scenario runner failed with code $scenarioExitCode; standalone failures are retained in the report"
        }
    }
    $failedGuards = @($runReport.Results | Where-Object { $_.Status -eq 'failed' })
    if ( $failedGuards.Count ) {
        throw "XLIR supplemental batch has $($failedGuards.Count) failed guards; all selected standalone results were recorded"
    }
    $runReport.Status = "passed"
}
catch {
    $runReport.Status = "failed"
    throw
}
finally {
    $runReport.CompletedUtc = [DateTime]::UtcNow.ToString("o")
    Save-RunReport
    Set-Location $previousLocation
}

Write-Host "XLIR standalone supplemental guards ok: $($selected.Count) scripts ($Mode); $($parameterized.Count) parameterized workers require scenario orchestration."
exit 0

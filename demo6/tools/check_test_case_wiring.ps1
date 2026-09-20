param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$caseDir = Join-Path $Root "test\cases"
$readmePath = Join-Path $Root "test\README.md"
$manualProbeManifestPath = Join-Path $Root "test\manual_probes.txt"
$testSuiteDir = Join-Path $Root "test\suites"
$minTestCaseFiles = 607
$minCasePrefixes = 28
$wiringFiles = @(
    "test.bat",
    "test_tutorial.bat",
    "release\examples\feature_suite\run_feature_suite.bat",
    "tools\run_extlib_tests.ps1"
)

$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Get-TestSuiteWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File |
            Where-Object { $_.Extension -eq ".bat" -or $_.Extension -eq ".ps1" } |
            Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $files.Add($suite.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
}

function Get-CheckScriptWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]
    $toolDir = Join-Path $script:Root "tools"

    if ( Test-Path -LiteralPath $toolDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        foreach ( $scriptFile in @(Get-ChildItem -LiteralPath $toolDir -File -Filter "check_*.ps1" |
            Sort-Object Name) ) {
            $files.Add($scriptFile.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
}

function Normalize-PathText($path)
{
    return $path.Replace("\", "/")
}

function Test-CaseReference($text, $relative)
{
    $slashPath = Normalize-PathText $relative
    $backslashPath = $slashPath.Replace("/", "\")

    if ( $text -match [regex]::Escape($slashPath) ) {
        return $true
    }
    if ( $text -match [regex]::Escape($backslashPath) ) {
        return $true
    }
    return $false
}

function Get-CasePrefix($baseName)
{
    if ( $baseName -match '^(cgen|sem|stdlib|import|export|extern|invalid|cli|control|call)' ) {
        return $Matches[1]
    }
    if ( $baseName -match '^[^_]+' ) {
        return $Matches[0]
    }
    return $baseName
}

function Get-ManualProbePaths($manifestPath, $rootPath, $caseRootPath)
{
    $paths = New-Object System.Collections.Generic.HashSet[string]

    if ( ! (Test-Path -LiteralPath $manifestPath -PathType Leaf) ) {
        Add-Failure "manual probe manifest missing: test/manual_probes.txt"
        return $paths
    }

    $caseRootFullPath = [System.IO.Path]::GetFullPath($caseRootPath).TrimEnd('\')
    foreach ( $line in [System.IO.File]::ReadAllLines($manifestPath, [System.Text.Encoding]::UTF8) ) {
        $relative = Normalize-PathText $line.Trim()
        if ( ($relative.Length -eq 0) -or $relative.StartsWith("#") ) {
            continue
        }
        if ( ! $relative.StartsWith("test/cases/", [System.StringComparison]::OrdinalIgnoreCase) -or
             ! $relative.EndsWith(".xl", [System.StringComparison]::OrdinalIgnoreCase) ) {
            Add-Failure "manual probe path must be an .xl file under test/cases: $relative"
            continue
        }
        if ( [System.IO.Path]::GetFileNameWithoutExtension($relative) -notmatch '_probe_' ) {
            Add-Failure "manual probe name must contain _probe_: $relative"
            continue
        }

        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $rootPath $relative))
        if ( ! $fullPath.StartsWith($caseRootFullPath + '\', [System.StringComparison]::OrdinalIgnoreCase) ) {
            Add-Failure "manual probe path escapes test/cases: $relative"
            continue
        }
        if ( ! (Test-Path -LiteralPath $fullPath -PathType Leaf) ) {
            Add-Failure "manual probe file is missing: $relative"
            continue
        }
        if ( ! $paths.Add($relative) ) {
            Add-Failure "manual probe is listed more than once: $relative"
        }
    }

    return $paths
}

function Expand-BatchCaseMatrices($text)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    $matches = [regex]::Matches(
        $text,
        '(?ims)^\s*for\s+%%(?<var>[A-Za-z])\s+in\s*\(\s*\r?\n(?<items>.*?)^\s*\)\s+do\s*\(\s*\r?\n(?<body>.*?)^\s*\)')

    foreach ( $match in $matches ) {
        $varName = $match.Groups['var'].Value
        $body = $match.Groups['body'].Value
        $variablePattern = [regex]::Escape("%%${varName}")
        $caseTemplates = [regex]::Matches(
            $body,
            "test[\\/]+cases[\\/]+(?<path>[^`"'\s>]*${variablePattern}[^`"'\s>]*\.xl)")

        if ( $caseTemplates.Count -eq 0 ) {
            continue
        }
        foreach ( $itemMatch in [regex]::Matches($match.Groups['items'].Value, '(?m)^\s*([A-Za-z0-9_.-]+)\s*$') ) {
            foreach ( $caseTemplate in $caseTemplates ) {
                $casePath = Normalize-PathText $caseTemplate.Groups['path'].Value
                $casePath = $casePath.Replace("%%${varName}", $itemMatch.Groups[1].Value)
                $expanded.Add("test/cases/$casePath") | Out-Null
            }
        }
    }
    return @($expanded)
}

function Expand-CaseGlobs($text, $cases, $rootPath)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    $patterns = [regex]::Matches(
        $text,
        'test[\\/]+cases[\\/]+([^"''\s>]*[*?][^"''\s>]*\.xl)')

    foreach ( $patternMatch in $patterns ) {
        $pattern = Normalize-PathText $patternMatch.Groups[1].Value
        foreach ( $case in $cases ) {
            $relative = Normalize-PathText ($case.FullName.Substring($rootPath.Length + 1))
            $caseRelative = $relative.Substring("test/cases/".Length)
            if ( $caseRelative -like $pattern ) {
                $expanded.Add($relative) | Out-Null
            }
        }
    }
    return @($expanded)
}

function Expand-PowerShellCaseNames($text)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    foreach ( $match in [regex]::Matches(
        $text, '["''](?<name>[A-Za-z0-9_.-]+\.xl)["'']') ) {
        $name = $match.Groups['name'].Value
        if ( Test-Path -LiteralPath (Join-Path $script:caseDir $name) -PathType Leaf ) {
            $expanded.Add("test/cases/$name") | Out-Null
        }
    }
    return @($expanded)
}

function Expand-PowerShellCaseFilters($text, $cases, $rootPath)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    foreach ( $match in [regex]::Matches(
        $text, '-Filter\s+["''](?<pattern>xlir_[^"'']*[*?][^"'']*\.xl)["'']') ) {
        $pattern = $match.Groups['pattern'].Value
        foreach ( $case in $cases ) {
            if ( $case.Name -like $pattern ) {
                $expanded.Add((Normalize-PathText (
                    $case.FullName.Substring($rootPath.Length + 1)))) | Out-Null
            }
        }
    }
    return @($expanded)
}

function Get-ImportedCaseReferences($cases, $caseRoot)
{
    $references = New-Object System.Collections.Generic.List[string]
    $caseRootPath = [System.IO.Path]::GetFullPath($caseRoot).TrimEnd('\')

    foreach ( $case in $cases ) {
        $text = Read-Utf8Text $case.FullName
        foreach ( $match in [regex]::Matches($text, '(?m)^\s*import\s+["'']([^"'']+)["'']') ) {
            $importPath = $match.Groups[1].Value
            if ( $importPath.StartsWith('.') ) {
                $candidate = Join-Path $case.DirectoryName $importPath
            } elseif ( $importPath.Contains('/') -or $importPath.Contains('\') ) {
                $candidate = Join-Path $caseRootPath $importPath
            } else {
                continue
            }
            if ( [System.IO.Path]::GetExtension($candidate) -eq '' ) {
                $candidate += '.xl'
            }
            $candidate = [System.IO.Path]::GetFullPath($candidate)
            if ( $candidate.StartsWith($caseRootPath + '\', [System.StringComparison]::OrdinalIgnoreCase) -and
                 (Test-Path -LiteralPath $candidate -PathType Leaf) ) {
                $relative = $candidate.Substring($caseRootPath.Length + 1).Replace('\', '/')
                $references.Add("test/cases/$relative") | Out-Null
            }
        }
    }
    return @($references)
}

if ( ! (Test-Path -LiteralPath $caseDir -PathType Container) ) {
    Write-Host "test cases directory missing: $caseDir"
    exit 1
}

$wiringFiles += Get-TestSuiteWiringFiles
$wiringFiles += Get-CheckScriptWiringFiles

if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
    Add-Failure "test README missing: test/README.md"
}

$textParts = New-Object System.Collections.Generic.List[string]
foreach ( $relative in $wiringFiles ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wiring file missing: $relative"
        continue
    }
    $textParts.Add((Read-Utf8Text $path)) | Out-Null
}

$wiringText = [string]::Join("`n", $textParts)
$rootPath = (Resolve-Path -LiteralPath $Root).Path
$cases = @(Get-ChildItem -LiteralPath $caseDir -Filter "*.xl" -File -Recurse | Sort-Object FullName)
$manualProbePaths = Get-ManualProbePaths $manualProbeManifestPath $rootPath $caseDir
$automatedCases = @($cases | Where-Object {
    $relative = Normalize-PathText ($_.FullName.Substring($rootPath.Length + 1))
    ! $manualProbePaths.Contains($relative)
})
$casePrefixes = New-Object System.Collections.Generic.HashSet[string]
$wiringText += "`n" + [string]::Join("`n", @(Expand-BatchCaseMatrices $wiringText))
$wiringText += "`n" + [string]::Join("`n", @(Expand-CaseGlobs $wiringText $automatedCases $rootPath))
$wiringText += "`n" + [string]::Join("`n", @(Expand-PowerShellCaseNames $wiringText))
$wiringText += "`n" + [string]::Join("`n", @(
    Expand-PowerShellCaseFilters $wiringText $automatedCases $rootPath))
$wiringText += "`n" + [string]::Join("`n", @(Get-ImportedCaseReferences $automatedCases $caseDir))

foreach ( $case in $cases ) {
    $relative = Normalize-PathText ($case.FullName.Substring($rootPath.Length + 1))
    $isManualProbe = $manualProbePaths.Contains($relative)

    if ( ($case.BaseName -match '_probe_') -and ! $isManualProbe ) {
        Add-Failure "manual probe is not listed in test/manual_probes.txt: $relative"
    }
    if ( $isManualProbe ) {
        if ( Test-CaseReference $wiringText $relative ) {
            Add-Failure "manual probe must not be wired into automated suites: $relative"
        }
        continue
    }
    if ( $case.DirectoryName -eq $caseDir ) {
        $casePrefixes.Add((Get-CasePrefix $case.BaseName)) | Out-Null
    }

    if ( ! (Test-CaseReference $wiringText $relative) ) {
        Add-Failure "test case is not wired: $relative"
    }
}

if ( $cases.Count -lt $minTestCaseFiles ) {
    Add-Failure "test case baseline regressed: $($cases.Count) < $minTestCaseFiles"
}

if ( $casePrefixes.Count -lt $minCasePrefixes ) {
    Add-Failure "test case prefix baseline regressed: $($casePrefixes.Count) < $minCasePrefixes"
}

if ( Test-Path -LiteralPath $readmePath -PathType Leaf ) {
    $readmeText = Read-Utf8Text $readmePath
    $startMarker = "<!-- case-prefix-index:start -->"
    $endMarker = "<!-- case-prefix-index:end -->"
    $start = $readmeText.IndexOf($startMarker)
    $end = $readmeText.IndexOf($endMarker)
    $readmePrefixes = New-Object System.Collections.Generic.HashSet[string]

    if ( $start -lt 0 -or $end -lt 0 -or $end -le $start ) {
        Add-Failure "test README case prefix index markers are missing or invalid"
    } else {
        $indexStart = $start + $startMarker.Length
        $indexText = $readmeText.Substring($indexStart, $end - $indexStart)
        $matches = [regex]::Matches($indexText, '(?m)^-\s+`([^`]+)`')

        foreach ( $match in $matches ) {
            $prefix = $match.Groups[1].Value
            if ( ! $readmePrefixes.Add($prefix) ) {
                Add-Failure "test README lists case prefix more than once: $prefix"
            }
        }

        foreach ( $prefix in $casePrefixes ) {
            if ( ! $readmePrefixes.Contains($prefix) ) {
                Add-Failure "test README does not list case prefix: $prefix"
            }
        }

        foreach ( $prefix in $readmePrefixes ) {
            if ( ! $casePrefixes.Contains($prefix) ) {
                Add-Failure "test README references missing case prefix: $prefix"
            }
        }
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($wiringText, 'test[\\/]+cases[\\/]+([^"''\s>]+\.xl)')
foreach ( $match in $matches ) {
    $relative = Normalize-PathText $match.Groups[1].Value
    if ( $relative.Length -gt 0 -and ! $relative.Contains("%") -and
         ! $relative.Contains("$") -and ! $relative.Contains("*") -and
         ! $relative.Contains("{") -and ! $relative.Contains("}") ) {
        $referenced.Add($relative) | Out-Null
    }
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    $path = Join-Path $caseDir ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired test case file is missing: test/cases/$relative"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Test case wiring ok: $($automatedCases.Count) automated files, $($manualProbePaths.Count) manual probes, $($casePrefixes.Count) prefixes."
exit 0

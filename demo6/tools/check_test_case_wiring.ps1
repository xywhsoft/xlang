param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$caseDir = Join-Path $Root "test\cases"
$readmePath = Join-Path $Root "test\README.md"
$testSuiteDir = Join-Path $Root "test\suites"
$minTestCaseFiles = 607
$minCasePrefixes = 28
$wiringFiles = @(
    "test.bat",
    "test_tutorial.bat",
    "release\examples\feature_suite\run_feature_suite.bat"
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

function Expand-BatchCaseMatrices($text)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    $matches = [regex]::Matches(
        $text,
        '(?is)for\s+%%(?<var>[A-Za-z])\s+in\s*\((?<items>.*?)\)\s+do\s*\((?<body>.*?)\)')

    foreach ( $match in $matches ) {
        $varName = $match.Groups['var'].Value
        $body = $match.Groups['body'].Value
        $casePattern = "test\cases\%%${varName}.xl"

        if ( ! $body.Contains($casePattern) ) {
            continue
        }
        foreach ( $itemMatch in [regex]::Matches($match.Groups['items'].Value, '(?m)^\s*([A-Za-z0-9_.-]+)\s*$') ) {
            $expanded.Add("test/cases/$($itemMatch.Groups[1].Value).xl") | Out-Null
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
$casePrefixes = New-Object System.Collections.Generic.HashSet[string]
$wiringText += "`n" + [string]::Join("`n", @(Expand-BatchCaseMatrices $wiringText))
$wiringText += "`n" + [string]::Join("`n", @(Get-ImportedCaseReferences $cases $caseDir))

foreach ( $case in $cases ) {
    if ( $case.DirectoryName -eq $caseDir ) {
        $casePrefixes.Add((Get-CasePrefix $case.BaseName)) | Out-Null
    }
    $relative = Normalize-PathText ($case.FullName.Substring($rootPath.Length + 1))

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
    if ( $relative.Length -gt 0 -and ! $relative.Contains("%") -and ! $relative.Contains("*") ) {
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

Write-Host "Test case wiring ok: $($cases.Count) files, $($casePrefixes.Count) prefixes."
exit 0

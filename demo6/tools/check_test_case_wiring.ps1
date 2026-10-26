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
    "test_linux.sh",
    "test_tutorial.bat",
    "release\examples\feature_suite\run_feature_suite.bat",
    "tools\run_extlib_tests.ps1"
)

$failures = New-Object System.Collections.Generic.List[string]
$trackedFiles = @()
$retiredTestAssets = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ( $relative in @(
    'tools/check_typed_hir_contract.ps1',
    'tools/check_xlir_cgen_differential.ps1',
    'test/cases/cgen_base_method_typed_hir.xl',
    # SipHash64 moved to the stdlib; the extension regression is SipHash128.
    'test/cases/extlib_siphash.xl',
    # These extension fixtures were removed together with their unsupported
    # package surfaces. Keep their retirement explicit so an unrelated missing
    # tracked test still fails this inventory check.
    'test/cases/extlib_ceres.xl',
    'test/cases/extlib_eigen.xl',
    'test/cases/extlib_fftw.xl',
    'test/cases/extlib_gdal.xl',
    'test/cases/extlib_geos.xl',
    'test/cases/extlib_gmp.xl',
    'test/cases/extlib_gsl.xl',
    'test/cases/extlib_kafka.xl',
    'test/cases/extlib_lapack.xl',
    'test/cases/extlib_lightgbm.xl',
    'test/cases/extlib_mpfr.xl',
    'test/cases/extlib_ncurses.xl',
    'test/cases/extlib_nlopt.xl',
    'test/cases/extlib_parquet.xl',
    'test/cases/extlib_plot.xl',
    'test/cases/extlib_proj.xl',
    'test/cases/extlib_pulsar.xl',
    'test/cases/extlib_rdkit.xl',
    'test/cases/extlib_s2geometry.xl',
    'test/cases/extlib_ssh.xl',
    'test/cases/extlib_ta_lib.xl',
    'test/cases/extlib_unqlite.xl',
    'test/cases/extlib_xgboost.xl'
) ) {
    $retiredTestAssets.Add($relative) | Out-Null
}
$replacementTestAssets = @('test/cases/cgen_base_method_xlir.xl')
$gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
if ( $null -ne $gitCommand ) {
    $trackedFiles = @(& $gitCommand.Source -c "core.quotepath=false" -C $Root ls-files 2>$null)
    $trackedFiles = @($trackedFiles | Where-Object {
        !$retiredTestAssets.Contains($_.Replace("\", "/"))
    })
    foreach ( $relative in $replacementTestAssets ) {
        if ( (Test-Path -LiteralPath (Join-Path $Root $relative) -PathType Leaf) -and
             $relative -notin $trackedFiles ) {
            $trackedFiles += $relative
        }
    }
}
$gitInventoryAvailable = ($null -ne $gitCommand -and $LASTEXITCODE -eq 0 -and
    $trackedFiles.Count -gt 0)
$trackedFileSet = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ( $relative in $trackedFiles ) {
    $trackedFileSet.Add($relative.Replace("\", "/")) | Out-Null
}

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

foreach ( $relative in $retiredTestAssets ) {
    if ( Test-Path -LiteralPath (Join-Path $Root $relative) ) {
        Add-Failure "retired test asset returned: $relative"
    }
}
foreach ( $relative in $replacementTestAssets ) {
    if ( ! (Test-Path -LiteralPath (Join-Path $Root $relative) -PathType Leaf) ) {
        Add-Failure "replacement XLIR test asset is missing: $relative"
    }
}

function Get-VersionedFilesUnder(
    [string]$relativePrefix,
    [string[]]$extensions,
    [string]$namePattern = "*",
    [switch]$Recurse)
{
    if ( ! $gitInventoryAvailable ) {
        $parameters = @{
            LiteralPath = (Join-Path $script:Root $relativePrefix)
            File = $true
            ErrorAction = "Stop"
        }
        if ( $Recurse ) { $parameters["Recurse"] = $true }
        $items = @(Get-ChildItem @parameters | Where-Object {
            $_.Name -like $namePattern -and $_.Extension -in $extensions
        })
        return @($items | Sort-Object FullName)
    }

    $prefix = $relativePrefix.Replace("\", "/").TrimEnd("/") + "/"
    $files = New-Object System.Collections.Generic.List[object]
    foreach ( $relative in $trackedFiles ) {
        $normalized = $relative.Replace("\", "/")
        if ( ! $normalized.StartsWith(
                $prefix, [System.StringComparison]::OrdinalIgnoreCase) ) {
            continue
        }
        $tail = $normalized.Substring($prefix.Length)
        if ( ! $Recurse -and $tail.Contains("/") ) { continue }
        $name = [System.IO.Path]::GetFileName($tail)
        $extension = [System.IO.Path]::GetExtension($name)
        if ( $name -notlike $namePattern -or $extension -notin $extensions ) {
            continue
        }
        $path = Join-Path $script:Root $relative
        if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
            Add-Failure "versioned test asset is missing: $relative"
            continue
        }
        $files.Add((Get-Item -LiteralPath $path)) | Out-Null
    }
    return @($files | Sort-Object FullName)
}

function Get-TestSuiteWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]

    $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
    $suiteFiles = @(Get-VersionedFilesUnder "test\suites" @(".bat", ".ps1"))
    foreach ( $suite in $suiteFiles ) {
        $files.Add($suite.FullName.Substring($rootPath.Length + 1)) | Out-Null
    }

    return @($files)
}

function Get-CheckScriptWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]

    $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
    foreach ( $scriptFile in @(Get-VersionedFilesUnder "tools" @(".ps1", ".sh", ".py") "check_*") ) {
        $files.Add($scriptFile.FullName.Substring($rootPath.Length + 1)) | Out-Null
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
        '(?ims)^\s*for\s+%%(?<var>[A-Za-z])\s+in\s*\((?<items>[^)]*)\)\s+do\s*\(\s*\r?\n(?<body>.*?)^\s*\)')

    foreach ( $match in $matches ) {
        $varName = $match.Groups['var'].Value
        $body = $match.Groups['body'].Value
        $variablePattern = [regex]::Escape("%%${varName}")
        $caseTemplates = [regex]::Matches(
            $body,
            "test[\\/]+cases[\\/]+(?<path>[^,`"'\s>]*${variablePattern}[^,`"'\s>]*\.xl)")

        if ( $caseTemplates.Count -eq 0 ) {
            continue
        }
        foreach ( $itemMatch in [regex]::Matches($match.Groups['items'].Value, '(?<![A-Za-z0-9_.-])([A-Za-z0-9_.-]+)(?![A-Za-z0-9_.-])') ) {
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
        'test[\\/]+cases[\\/]+([^,"''\s>]*[*?][^,"''\s>]*\.xl)')

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
        $text, '["''](?<name>[A-Za-z0-9_.-]+(?:\.xl)?)["'']') ) {
        $name = $match.Groups['name'].Value
        if (!$name.EndsWith('.xl', [StringComparison]::OrdinalIgnoreCase)) { $name += '.xl' }
        if ( Test-Path -LiteralPath (Join-Path $script:caseDir $name) -PathType Leaf ) {
            $expanded.Add("test/cases/$name") | Out-Null
        }
    }
    return @($expanded)
}

function Expand-ShellCaseMatrices($text)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    # Only literal test matrices, not arbitrary identifiers or shell expansion.
    foreach ($matrix in [regex]::Matches($text,
        '(?m)\b(?:cases|fixtures|negative|negatives)=\((?<names>[^)]*)\)')) {
        foreach ($item in [regex]::Matches($matrix.Groups['names'].Value,
            '(?<![A-Za-z0-9_./$-])(?<name>[A-Za-z0-9_.-]+)(?![A-Za-z0-9_./$-])')) {
            $name = $item.Groups['name'].Value
            if (!$name.EndsWith('.xl', [StringComparison]::OrdinalIgnoreCase)) { $name += '.xl' }
            if (Test-Path -LiteralPath (Join-Path $script:caseDir $name) -PathType Leaf) {
                $expanded.Add("test/cases/$name") | Out-Null
            }
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

function Expand-ReferencedCaseManifests($text, $cases, $rootPath)
{
    $expanded = New-Object System.Collections.Generic.List[string]
    $casePaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ( $case in $cases ) {
        $relative = Normalize-PathText ($case.FullName.Substring($rootPath.Length + 1))
        $casePaths.Add($relative) | Out-Null
    }
    $seenManifests = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ( $match in [regex]::Matches(
        $text, 'test[\\/]+cases[\\/]+(?<path>[^,"''\s>]+\.json)') ) {
        $manifestRelative = "test/cases/" +
            (Normalize-PathText $match.Groups['path'].Value)
        if ( ! $seenManifests.Add($manifestRelative) ) { continue }
        if ( $gitInventoryAvailable -and
             ! $trackedFileSet.Contains($manifestRelative) ) {
            Add-Failure "referenced test case manifest is not versioned: $manifestRelative"
            continue
        }
        $manifestPath = Join-Path $script:Root $manifestRelative
        if ( ! (Test-Path -LiteralPath $manifestPath -PathType Leaf) ) {
            Add-Failure "referenced test case manifest is missing: $manifestRelative"
            continue
        }
        $manifestText = Read-Utf8Text $manifestPath
        try { $null = $manifestText | ConvertFrom-Json -ErrorAction Stop }
        catch {
            Add-Failure "referenced test case manifest is invalid JSON: $manifestRelative"
            continue
        }
        foreach ( $nameMatch in [regex]::Matches(
            $manifestText, '"(?<name>[A-Za-z0-9_.\/-]+)"') ) {
            $name = Normalize-PathText $nameMatch.Groups['name'].Value
            if ( $name.Contains("..") ) { continue }
            if ( ! $name.EndsWith(".xl", [System.StringComparison]::OrdinalIgnoreCase) ) {
                $name += ".xl"
            }
            $candidate = "test/cases/$name"
            if ( $casePaths.Contains($candidate) ) {
                $expanded.Add($candidate) | Out-Null
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
$cases = @(Get-VersionedFilesUnder "test\cases" @(".xl") "*" -Recurse)
$manifestIsVersioned = !$gitInventoryAvailable -or
    $trackedFileSet.Contains("test/manual_probes.txt")
$manualProbePaths = if ( $manifestIsVersioned ) {
    Get-ManualProbePaths $manualProbeManifestPath $rootPath $caseDir
} else {
    New-Object System.Collections.Generic.HashSet[string]
}
$automatedCases = @($cases | Where-Object {
    $relative = Normalize-PathText ($_.FullName.Substring($rootPath.Length + 1))
    ! $manualProbePaths.Contains($relative)
})
$casePrefixes = New-Object System.Collections.Generic.HashSet[string]
$wiringText += "`n" + [string]::Join("`n", @(Expand-BatchCaseMatrices $wiringText))
$wiringText += "`n" + [string]::Join("`n", @(Expand-CaseGlobs $wiringText $automatedCases $rootPath))
$wiringText += "`n" + [string]::Join("`n", @(Expand-PowerShellCaseNames $wiringText))
$wiringText += "`n" + [string]::Join("`n", @(Expand-ShellCaseMatrices $wiringText))
$wiringText += "`n" + [string]::Join("`n", @(
    Expand-PowerShellCaseFilters $wiringText $automatedCases $rootPath))
$wiringText += "`n" + [string]::Join("`n", @(
    Expand-ReferencedCaseManifests $wiringText $automatedCases $rootPath))
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
$matches = [regex]::Matches($wiringText, 'test[\\/]+cases[\\/]+([^,"''\s>]+\.xl)')
foreach ( $match in $matches ) {
    $relative = Normalize-PathText $match.Groups[1].Value
    if ( $relative.Length -gt 0 -and ! $relative.Contains("%") -and
         ! $relative.Contains("$") -and ! $relative.Contains("*") -and
         ! $relative.Contains("{") -and ! $relative.Contains("}") ) {
        $referenced.Add($relative) | Out-Null
    }
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    if ( $retiredTestAssets.Contains("test/cases/$relative") ) {
        continue
    }
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

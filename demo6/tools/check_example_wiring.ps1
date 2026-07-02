param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$exampleDir = Join-Path $Root "examples"
$minExampleFiles = 73
$wiringFiles = @(
    "test.bat",
    "test_all.bat",
    "test_tutorial.bat",
    "test_feature_suite.bat",
    "tools\check_stdlib_examples.ps1",
    "examples\extlibs\run_extlibs.bat",
    "examples\feature_suite\run_feature_suite.bat"
)
$draftExampleFiles = @()

$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Normalize-ExamplePath($relative)
{
    return $relative.Replace("\", "/")
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Test-WildcardCovered($relative)
{
    if ( $relative -like "examples/feature_suite/*.xl" ) {
        return $true
    }
    if ( $relative -like "examples/tutorial/*.xl" ) {
        return $true
    }
    if ( $relative -like "examples/stdlib_*.xl" ) {
        return $true
    }
    return $false
}

function Test-DraftExample($relative)
{
    $normalized = Normalize-ExamplePath $relative
    foreach ( $draft in $draftExampleFiles ) {
        if ( $normalized -eq (Normalize-ExamplePath $draft) ) {
            return $true
        }
    }
    return $false
}

function Test-DirectReference($text, $relative)
{
    $slashPath = $relative.Replace("\", "/")
    $backslashPath = $relative.Replace("/", "\")

    if ( $text -match [regex]::Escape($slashPath) ) {
        return $true
    }
    if ( $text -match [regex]::Escape($backslashPath) ) {
        return $true
    }
    return $false
}

function Test-ExampleReadmeIndex($relativeDir)
{
    $dir = Join-Path $Root $relativeDir
    $readmePath = Join-Path $dir "README.md"

    if ( ! (Test-Path -LiteralPath $dir -PathType Container) ) {
        Add-Failure "example README directory missing: $relativeDir"
        return
    }

    if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
        Add-Failure "example README missing: $relativeDir/README.md"
        return
    }

    $readme = Read-Utf8Text $readmePath
    $files = @(
        Get-ChildItem -LiteralPath $dir -File -Filter "*.xl" |
            Where-Object {
                $relative = ($relativeDir + "/" + $_.Name).Replace("\", "/")
                ! (Test-DraftExample $relative)
            } |
            Sort-Object Name
    )
    $known = New-Object System.Collections.Generic.HashSet[string]

    foreach ( $file in $files ) {
        $known.Add($file.Name) | Out-Null
        if ( ! $readme.Contains($file.Name) ) {
            Add-Failure "example README does not list ${relativeDir}/$($file.Name)"
        }
    }

    $matches = [regex]::Matches($readme, '[A-Za-z0-9_]+[.]xl')
    foreach ( $match in $matches ) {
        $name = $match.Value
        if ( ! $known.Contains($name) ) {
            Add-Failure "example README references missing file ${relativeDir}/${name}"
        }
    }
}

function Test-RootExampleReadmeIndex()
{
    $readmePath = Join-Path $exampleDir "README.md"

    if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
        Add-Failure "examples README missing: examples/README.md"
        return
    }

    $readme = Read-Utf8Text $readmePath
    $files = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Sort-Object Name)
    $known = New-Object System.Collections.Generic.HashSet[string]

    foreach ( $file in $files ) {
        $known.Add($file.Name) | Out-Null
        if ( ! $readme.Contains($file.Name) ) {
            Add-Failure "examples README does not list examples/$($file.Name)"
        }
    }

    $matches = [regex]::Matches($readme, '[A-Za-z0-9_]+[.]xl')
    foreach ( $match in $matches ) {
        $name = $match.Value
        if ( ! $known.Contains($name) ) {
            Add-Failure "examples README references missing root example examples/${name}"
        }
    }

    $cFiles = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.c" | Sort-Object Name)
    $knownC = New-Object System.Collections.Generic.HashSet[string]

    foreach ( $file in $cFiles ) {
        $knownC.Add($file.Name) | Out-Null
        if ( ! $readme.Contains($file.Name) ) {
            Add-Failure "examples README does not list examples/$($file.Name)"
        }
    }

    $cMatches = [regex]::Matches($readme, '[A-Za-z0-9_]+[.]c')
    foreach ( $match in $cMatches ) {
        $name = $match.Value
        if ( ! $knownC.Contains($name) ) {
            Add-Failure "examples README references missing root C host examples/${name}"
        }
    }

    foreach ( $required in @("tutorial/README.md", "feature_suite/README.md")) {
        if ( ! $readme.Contains($required) ) {
            Add-Failure "examples README does not reference ${required}"
        }
    }
}

function Test-RootExampleCoverageModes()
{
    $readmePath = Join-Path $exampleDir "README.md"
    $testBatPath = Join-Path $Root "test.bat"

    if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
        Add-Failure "examples README missing: examples/README.md"
        return
    }

    if ( ! (Test-Path -LiteralPath $testBatPath -PathType Leaf) ) {
        Add-Failure "test.bat missing for root example coverage mode check"
        return
    }

    $readme = Read-Utf8Text $readmePath
    $testText = Read-Utf8Text $testBatPath
    $coverageMarker = [string]::Concat([char]0x56DE, [char]0x5F52, [char]0xFF1A)
    $modeMap = [ordered]@{
        "hello.xl" = @("lex", "parse", "check", "cgen", "ir")
        "literals.xl" = @("lex", "parse")
        "containers.xl" = @("check", "run")
        "anonymous_functions.xl" = @("check")
        "generic_identity.xl" = @("check")
        "generics.xl" = @("check")
        "generic_constraints.xl" = @("check", "run")
        "generic_class_iter.xl" = @("run")
        "runtime_type.xl" = @("run")
        "class_point.xl" = @("check")
        "class_field_persistence_attrs.xl" = @("run")
        "object_protocol.xl" = @("check")
        "oop_protocol_overload.xl" = @("check", "run")
        "oop_property_get_let.xl" = @("check", "run")
        "oop_static_access.xl" = @("check", "run")
        "oop_protected_access.xl" = @("check", "run")
        "oop_virtual_dispatch.xl" = @("check", "run")
        "copy_clone.xl" = @("check")
        "class_persistence.xl" = @("run")
        "class_custom.xl" = @("check")
        "class_native_bridge.xl" = @("check")
        "custom_counter.xl" = @("check")
        "second_stage.xl" = @("check", "run")
        "concurrency.xl" = @("run")
        "task_api.xl" = @("run")
        "async_generator.xl" = @("run")
        "union.xl" = @("run")
        "c_hello.xl" = @("run")
        "c_variables.xl" = @("check")
        "c_export.xl" = @("abi")
        "class_c_export.xl" = @("cgen", "abi")
    }

    $rootExamples = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Where-Object { $_.Name -notlike "stdlib_*.xl" } | Sort-Object Name)
    $rootNames = New-Object System.Collections.Generic.HashSet[string]

    foreach ( $example in $rootExamples ) {
        $rootNames.Add($example.Name) | Out-Null
        if ( ! $modeMap.Contains($example.Name) ) {
            Add-Failure "root example has no coverage mode declaration in guard: examples/$($example.Name)"
        }
    }

    foreach ( $name in $modeMap.Keys ) {
        if ( ! $rootNames.Contains($name) ) {
            Add-Failure "root example coverage mode guard references missing file: examples/${name}"
        }
    }

    foreach ( $name in $modeMap.Keys ) {
        $readmePattern = '(?m)^-\s+`' + [regex]::Escape($name) + '`[^\r\n]*$'
        $match = [regex]::Match($readme, $readmePattern)

        if ( ! $match.Success ) {
            Add-Failure "examples README does not declare coverage modes for examples/${name}"
            continue
        }

        $lineText = $match.Value
        if ( ! $lineText.Contains($coverageMarker) ) {
            Add-Failure "examples README coverage line for examples/${name} misses coverage marker"
            continue
        }

        foreach ( $mode in $modeMap[$name] ) {
            $modeToken = [string]::Concat([char]0x60, $mode, [char]0x60)
            if ( ! $lineText.Contains($modeToken) ) {
                Add-Failure "examples README coverage for examples/${name} misses mode: $mode"
            }

            $commandPattern = '(?m)release\\xl[.]exe\s+' + [regex]::Escape($mode) + '\s+examples\\' + [regex]::Escape($name)
            if ( ! ($testText -match $commandPattern) ) {
                Add-Failure "test.bat does not cover examples/${name} with mode: $mode"
            }
        }
    }
}

function Test-ExampleExecutionIndex($relativeDir, $batchRelative, $exportOnlyNames)
{
    $dir = Join-Path $Root $relativeDir
    $batchPath = Join-Path $Root $batchRelative

    if ( ! (Test-Path -LiteralPath $dir -PathType Container) ) {
        Add-Failure "example execution directory missing: $relativeDir"
        return
    }

    if ( ! (Test-Path -LiteralPath $batchPath -PathType Leaf) ) {
        Add-Failure "example execution batch missing: $batchRelative"
        return
    }

    $batchText = Read-Utf8Text $batchPath
    $files = @(
        Get-ChildItem -LiteralPath $dir -File -Filter "*.xl" |
            Where-Object {
                $relative = ($relativeDir + "/" + $_.Name).Replace("\", "/")
                -not (Test-DraftExample $relative)
            } |
            Sort-Object Name
    )
    $known = New-Object System.Collections.Generic.HashSet[string]

    foreach ( $file in $files ) {
        $known.Add($file.Name) | Out-Null
        if ( ! $batchText.Contains($file.Name) ) {
            Add-Failure "example batch does not explicitly execute or export ${relativeDir}/$($file.Name)"
        }
    }

    $matches = [regex]::Matches($batchText, '[A-Za-z0-9_]+[.]xl')
    foreach ( $match in $matches ) {
        $name = $match.Value
        if ( ! $known.Contains($name) ) {
            Add-Failure "example batch references missing file ${relativeDir}/${name}"
        }
    }

    foreach ( $name in $exportOnlyNames ) {
        if ( ! $known.Contains($name) ) {
            Add-Failure "example export-only file is missing: ${relativeDir}/${name}"
            continue
        }

        $cgenPattern = '(?m)cgen\s+.*' + [regex]::Escape($name) + '.*--emit-header'
        $exportCallPattern = '(?m)call\s+:export_example\s+[^\r\n]*' + [regex]::Escape($name) + '(\s|$)'
        $exportHelperPattern = '(?ms):export_example\b.*?cgen\s+.*?--emit-header'
        if ( ! (($batchText -match $cgenPattern) -or (($batchText -match $exportCallPattern) -and ($batchText -match $exportHelperPattern))) ) {
            Add-Failure "example export-only file is not cgen/header verified: ${relativeDir}/${name}"
        }
    }
}

if ( ! (Test-Path -LiteralPath $exampleDir -PathType Container) ) {
    Write-Host "examples directory missing: $exampleDir"
    exit 1
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
$examples = @(Get-ChildItem -LiteralPath $exampleDir -Filter "*.xl" -File -Recurse | Sort-Object FullName)
$rootPath = (Resolve-Path -LiteralPath $Root).Path

if ( $examples.Count -lt $minExampleFiles ) {
    Add-Failure "example file baseline regressed: $($examples.Count) < $minExampleFiles"
}

foreach ( $example in $examples ) {
    $relative = $example.FullName.Substring($rootPath.Length + 1).Replace("\", "/")

    if ( Test-WildcardCovered $relative ) {
        continue
    }

    if ( Test-DraftExample $relative ) {
        continue
    }

    if ( ! (Test-DirectReference $wiringText $relative) ) {
        Add-Failure "example is not wired: $relative"
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($wiringText, 'examples[\\/]+([^"''\s>]+\.xl)')
foreach ( $match in $matches ) {
    $relative = ("examples/" + $match.Groups[1].Value).Replace("\", "/")
    if ( $relative.Contains("%") -or $relative.Contains("*") ) {
        continue
    }
    $referenced.Add($relative) | Out-Null
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    $path = Join-Path $Root ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired example file is missing: $relative"
    }
}

Test-RootExampleReadmeIndex
Test-RootExampleCoverageModes
Test-ExampleReadmeIndex "examples\tutorial"
Test-ExampleReadmeIndex "examples\feature_suite"
Test-ExampleExecutionIndex "examples\tutorial" "test_tutorial.bat" @("11_export_to_c.xl")
Test-ExampleExecutionIndex "examples\feature_suite" "examples\feature_suite\run_feature_suite.bat" @("09_c_export_abi.xl")
Test-ExampleExecutionIndex "examples\extlibs" "examples\extlibs\run_extlibs.bat" @("x_com.xl")

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Example wiring ok: $($examples.Count) files."
exit 0

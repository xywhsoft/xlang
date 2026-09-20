param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$srcDir = Join-Path $Root "src"
$srcReadme = Join-Path $srcDir "README.md"
$testSuiteDir = Join-Path $Root "test\suites"
$minSourceFiles = 120
$minDirectSources = 34
$minIncludedSources = 85
$minGeneratedDirectSources = 2
$wiringFiles = @(
    "build.bat",
    "build_dll.bat",
    "test.bat",
    "tools\rc0_guard.ps1"
)
$generatedSourceInputs = @{
    "src/lexer/xl6_lexer_gen.c" = "src/lexer/xl6_lexer.l"
    "src/parser/xl6_parser.c" = "src/parser/xl6_parser.y"
}
$embeddedRuntimeManifest = Join-Path $Root "src\api\xl_api.c"
$embeddedRuntimeRequired = @(
    "src/api/xl_api_symbols.c",
    "src/api/xl_api_path.c",
    "src/sem/xl6_sem_docs.c",
    "src/sem/xl6_sem_field_attrs.c"
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
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $files.Add($suite.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
}

function Normalize-RelativePath($path)
{
    return $path.Replace("\", "/")
}

function Relative-From-Root($path)
{
    $rootPath = (Resolve-Path -LiteralPath $Root).Path
    return (Normalize-RelativePath $path.FullName.Substring($rootPath.Length + 1))
}

function Test-GeneratedSource($relative)
{
    return $generatedSourceInputs.ContainsKey($relative)
}

if ( ! (Test-Path -LiteralPath $srcDir -PathType Container) ) {
    Write-Host "src directory missing: $srcDir"
    exit 1
}

if ( ! (Test-Path -LiteralPath $srcReadme -PathType Leaf) ) {
    Write-Host "src README missing: $srcReadme"
    exit 1
}

$srcReadmeText = Read-Utf8Text $srcReadme
$srcSubdirs = @(Get-ChildItem -LiteralPath $srcDir -Directory | Sort-Object Name)

foreach ( $dir in $srcSubdirs ) {
    if ( ! $srcReadmeText.Contains($dir.Name) ) {
        Add-Failure "src README does not list source directory: src/$($dir.Name)"
    }
}

if ( ! (Test-Path -LiteralPath $embeddedRuntimeManifest -PathType Leaf) ) {
    Add-Failure "embedded xlang runtime source manifest is missing: src/api/xl_api.c"
} else {
    $embeddedRuntimeText = Read-Utf8Text $embeddedRuntimeManifest
    foreach ( $source in $embeddedRuntimeRequired ) {
        if ( $embeddedRuntimeText.IndexOf(('"' + $source + '"'), [System.StringComparison]::Ordinal) -lt 0 ) {
            Add-Failure "embedded xlang runtime source manifest is missing split unit: $source"
        }
    }
}

$wiringFiles += Get-TestSuiteWiringFiles
$wiringTextParts = New-Object System.Collections.Generic.List[string]
foreach ( $relative in $wiringFiles ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "source wiring file missing: $relative"
        continue
    }
    $wiringTextParts.Add((Read-Utf8Text $path)) | Out-Null
}

$wiringText = [string]::Join("`n", $wiringTextParts)
$directSources = New-Object System.Collections.Generic.HashSet[string]
$directMatches = [regex]::Matches($wiringText, 'src[\\/][^"''\s\^<>|]+\.c')

foreach ( $match in $directMatches ) {
    $relative = Normalize-RelativePath $match.Value
    $directSources.Add($relative) | Out-Null

    $path = Join-Path $Root ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        if ( Test-GeneratedSource $relative ) {
            $inputRelative = $generatedSourceInputs[$relative]
            $inputPath = Join-Path $Root ($inputRelative.Replace("/", "\"))
            if ( ! (Test-Path -LiteralPath $inputPath -PathType Leaf) ) {
                Add-Failure "generated source input is missing: $relative <- $inputRelative"
            }
        } else {
            Add-Failure "wired source file is missing: $relative"
        }
    }
}

$includedSources = New-Object System.Collections.Generic.HashSet[string]
$embeddedSources = New-Object System.Collections.Generic.HashSet[string]
$srcFiles = @(Get-ChildItem -LiteralPath $srcDir -File -Recurse -Filter "*.c" | Sort-Object FullName)

$embeddedRuntimeText = Read-Utf8Text $embeddedRuntimeManifest
$embeddedMatches = [regex]::Matches(
	$embeddedRuntimeText,
	'xlAddTccSourceFile\s*\([^;]+"(src[\\/][^"]+\.c)"\s*\)'
)
foreach ( $match in $embeddedMatches ) {
	$relative = Normalize-RelativePath $match.Groups[1].Value
	$embeddedSources.Add($relative) | Out-Null
}

foreach ( $file in $srcFiles ) {
    $text = Read-Utf8Text $file.FullName
    $matches = [regex]::Matches($text, '(?m)^\s*#\s*include\s+"([^"]+\.c)"')

    foreach ( $match in $matches ) {
        $includeText = Normalize-RelativePath $match.Groups[1].Value
        if ( $includeText -like "src/*" ) {
            $includePath = Join-Path $Root ($includeText.Replace("/", "\"))
        } else {
            $includePath = Join-Path $file.DirectoryName ($includeText.Replace("/", "\"))
        }

        if ( ! (Test-Path -LiteralPath $includePath -PathType Leaf) ) {
            Add-Failure "included source file is missing: $(Relative-From-Root $file): $includeText"
            continue
        }

        $includedSources.Add((Relative-From-Root (Get-Item -LiteralPath $includePath))) | Out-Null
    }
}

foreach ( $file in $srcFiles ) {
    $relative = Relative-From-Root $file
    if ( ! $directSources.Contains($relative) -and
         ! $includedSources.Contains($relative) -and
         ! $embeddedSources.Contains($relative) ) {
        Add-Failure "source file is not wired: $relative"
    }
}

foreach ( $relative in ($directSources | Sort-Object) ) {
    if ( $includedSources.Contains($relative) ) {
        Add-Failure "source file is both directly compiled and included: $relative"
    }
}

$generatedDirectCount = @($directSources | Where-Object { Test-GeneratedSource $_ }).Count

if ( $srcFiles.Count -lt $minSourceFiles ) {
    Add-Failure "source file baseline regressed: $($srcFiles.Count) < $minSourceFiles"
}

if ( $directSources.Count -lt $minDirectSources ) {
    Add-Failure "direct source baseline regressed: $($directSources.Count) < $minDirectSources"
}

if ( $includedSources.Count -lt $minIncludedSources ) {
    Add-Failure "included source baseline regressed: $($includedSources.Count) < $minIncludedSources"
}

if ( $generatedDirectCount -lt $minGeneratedDirectSources ) {
    Add-Failure "generated direct source baseline regressed: $generatedDirectCount < $minGeneratedDirectSources"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Source wiring ok: $($srcFiles.Count) source files, $($directSources.Count) direct, $($includedSources.Count) included, $($embeddedSources.Count) embedded, $generatedDirectCount generated direct."
exit 0

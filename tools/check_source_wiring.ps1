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
    "tools\runtime_dll.rsp.in",
    "tools\compiler_dll.rsp.in",
    "tools\rc0_guard.ps1",
    "tools\precompile_builtin.py"
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
$linuxBuildPath = Join-Path $Root "build_linux.sh"
if ( ! (Test-Path -LiteralPath $linuxBuildPath -PathType Leaf) ) {
    Add-Failure "Linux build source manifest is missing: build_linux.sh"
} else {
    $linuxBuildText = Read-Utf8Text $linuxBuildPath
    # Resident ownership drivers must be available from both platform hosts.
    # A generated standalone artifact passing does not prove libxl.so exports it.
    $residentUnits = @([regex]::Matches($wiringText, 'src[\\/]runtime[\\/][A-Za-z0-9_]+\.c') |
        ForEach-Object { Normalize-RelativePath $_.Value } | Sort-Object -Unique)
    $linuxRuntimeSources=@(& python (Join-Path $Root 'tools/runtime_sources.py') --root $Root)
    if($LASTEXITCODE -ne 0){throw 'Cannot resolve the ordinary Linux runtime source manifest'}
    $linuxNativeSet=New-Object System.Collections.Generic.HashSet[string]
    foreach($source in $linuxRuntimeSources){
        $relative=[IO.Path]::GetFullPath($source).Substring(([IO.Path]::GetFullPath($Root)).Length+1)
        $linuxNativeSet.Add((Normalize-RelativePath $relative))|Out-Null
    }
    foreach ( $unit in $residentUnits ) {
        if ( ! $linuxBuildText.Contains($unit) -and ! $linuxNativeSet.Contains($unit) ) {
            Add-Failure "Linux build omits resident runtime source: $unit"
        }
    }
    if ( ! (Test-Path -LiteralPath (Join-Path $Root "lib/tcc/conftest.c") -PathType Leaf) ) {
        Add-Failure "vendored TinyCC bootstrap source is missing: lib/tcc/conftest.c"
    }
}
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

$pendingIncludes=New-Object 'System.Collections.Generic.Queue[string]'
$visitedIncludes=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
foreach($relative in @($directSources)+@($embeddedSources)) {
    $path=Join-Path $Root $relative
    if(Test-Path -LiteralPath $path -PathType Leaf){$pendingIncludes.Enqueue([IO.Path]::GetFullPath($path))}
}
while($pendingIncludes.Count) {
    $path=$pendingIncludes.Dequeue()
    if(!$visitedIncludes.Add($path)){continue}
    $file=Get-Item -LiteralPath $path
    $text = Read-Utf8Text $file.FullName
    $matches = [regex]::Matches($text, '(?m)^\s*#\s*include\s+"([^"]+\.(?:c|h|inc))"')

    foreach ( $match in $matches ) {
        $includeText = Normalize-RelativePath $match.Groups[1].Value
        $includePath=$null
        foreach($base in @($file.DirectoryName,$Root,(Join-Path $Root 'release'),(Join-Path $Root 'src'))) {
            $candidate=Join-Path $base $includeText
            if(Test-Path -LiteralPath $candidate -PathType Leaf){$includePath=[IO.Path]::GetFullPath($candidate);break}
        }

        if ( !$includePath ) {
            if($includeText.EndsWith('.c') -or $includeText.EndsWith('.inc')) {
                Add-Failure "included source file is missing: $(Relative-From-Root $file): $includeText"
            }
            continue
        }

        if($includePath.EndsWith('.c')) {
            $includedSources.Add((Relative-From-Root (Get-Item -LiteralPath $includePath))) | Out-Null
        }
        if($includePath.StartsWith(([IO.Path]::GetFullPath($srcDir)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)) {
            $pendingIncludes.Enqueue($includePath)
        }
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

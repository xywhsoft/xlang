param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$srcDir = Join-Path $Root "src"
$wiringFiles = @(
    "build.bat",
    "build_dll.bat",
    "test.bat",
    "tools\rc0_guard.ps1"
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

function Normalize-RelativePath($path)
{
    return $path.Replace("\", "/")
}

function Relative-From-Root($path)
{
    $rootPath = (Resolve-Path -LiteralPath $Root).Path
    return (Normalize-RelativePath $path.FullName.Substring($rootPath.Length + 1))
}

if ( ! (Test-Path -LiteralPath $srcDir -PathType Container) ) {
    Write-Host "src directory missing: $srcDir"
    exit 1
}

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
        Add-Failure "wired source file is missing: $relative"
    }
}

$includedSources = New-Object System.Collections.Generic.HashSet[string]
$srcFiles = @(Get-ChildItem -LiteralPath $srcDir -File -Recurse -Filter "*.c" | Sort-Object FullName)

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
    if ( ! $directSources.Contains($relative) -and ! $includedSources.Contains($relative) ) {
        Add-Failure "source file is not wired: $relative"
    }
}

foreach ( $relative in ($directSources | Sort-Object) ) {
    if ( $includedSources.Contains($relative) ) {
        Add-Failure "source file is both directly compiled and included: $relative"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Source wiring ok: $($srcFiles.Count) source files, $($directSources.Count) direct, $($includedSources.Count) included."
exit 0

param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$scanRoots = @(
    "src",
    "examples",
    "test\cases",
    "packages"
)
$failures = New-Object System.Collections.Generic.List[string]
$generatedFiles = @(
    "xl6_lexer_gen.c",
    "xl6_lexer_gen.h",
    "xl6_parser.c",
    "xl6_parser.h"
)
$unsafeFunctions = @(
    "strcpy",
    "strcat",
    "sprintf",
    "vsprintf",
    "gets",
    "scanf",
    "sscanf",
    "tmpnam",
    "tempnam",
    "mktemp",
    "putenv",
    "strncpy",
    "strncat",
    "strdup",
    "_strdup",
    "strtok",
    "strtok_s",
    "strerror",
    "strerror_s",
    "atoi",
    "atol",
    "atoll",
    "atof",
    "itoa",
    "ltoa",
    "ultoa",
    "gcvt",
    "ecvt",
    "fcvt",
    "stricmp",
    "strcmpi",
    "strnicmp",
    "_stricmp",
    "_strcmpi",
    "_strnicmp"
)
$unsafePattern = '\b(' + (($unsafeFunctions | ForEach-Object { [regex]::Escape($_) }) -join '|') + ')\s*\('

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

$files = @(
    foreach ( $scanRoot in $scanRoots ) {
        $dir = Join-Path $Root $scanRoot
        if ( ! (Test-Path -LiteralPath $dir -PathType Container) ) {
            Add-Failure "scan root missing: $scanRoot"
            continue
        }

        Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object {
            ($_.Extension -eq ".c" -or $_.Extension -eq ".h") -and
            ($generatedFiles -notcontains $_.Name)
        }
    }
) | Sort-Object FullName

foreach ( $file in $files ) {
    $matches = Select-String -LiteralPath $file.FullName -Pattern $unsafePattern
    foreach ( $match in $matches ) {
        $relative = $file.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1).Replace("\", "/")
        Add-Failure "unsafe C/CRT API in handwritten source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "C string safety ok: $($files.Count) handwritten/fixture C/H files."
exit 0

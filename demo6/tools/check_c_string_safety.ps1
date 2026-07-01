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
$minSafetyCFiles = 203
$minSafetySpecFiles = 2
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
$staleMarkerPattern = '\b(TODO|FIXME|TBD)\b|placeholder|code generation error|unsupported statement kind in C generator|unsupported expression kind in C generator|unsupported type in C generation|cannot box unsupported type into var|unsupported typed index compound assignment operator'
$directConsolePattern = '\b(printf|puts|putchar|fprintf|fputs|fputc)\s*\('
$driverCrtTimePattern = '\btime\s*\('
$commandExecPattern = '\b(system|popen|_popen|WinExec|ShellExecute[A-Za-z]*|CreateProcess[A-Za-z]*)\s*\('
$processControlPattern = '\b(exit|abort|assert)\s*\('
$manualCrtPrototypePattern = '(?m)^\s*(void|int|char\s*\*)\s+(exit|malloc|free|printf|fprintf)\s*\('

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Remove-CCommentsAndStrings($text)
{
    $builder = New-Object System.Text.StringBuilder
    $state = "normal"
    $escape = $false
    $i = 0

    while ( $i -lt $text.Length ) {
        $ch = $text[$i]
        $next = [char]0
        if ( $i + 1 -lt $text.Length ) {
            $next = $text[$i + 1]
        }

        if ( $state -eq "normal" ) {
            if ( $ch -eq [char]47 -and $next -eq [char]47 ) {
                [void]$builder.Append("  ")
                $state = "line-comment"
                $i += 2
                continue
            }

            if ( $ch -eq [char]47 -and $next -eq [char]42 ) {
                [void]$builder.Append("  ")
                $state = "block-comment"
                $i += 2
                continue
            }

            if ( $ch -eq [char]34 ) {
                [void]$builder.Append(" ")
                $state = "string"
                $escape = $false
                $i++
                continue
            }

            if ( $ch -eq [char]39 ) {
                [void]$builder.Append(" ")
                $state = "char"
                $escape = $false
                $i++
                continue
            }

            [void]$builder.Append($ch)
            $i++
            continue
        }

        if ( $state -eq "line-comment" ) {
            if ( $ch -eq [char]10 -or $ch -eq [char]13 ) {
                [void]$builder.Append($ch)
                $state = "normal"
            } else {
                [void]$builder.Append(" ")
            }
            $i++
            continue
        }

        if ( $state -eq "block-comment" ) {
            if ( $ch -eq [char]42 -and $next -eq [char]47 ) {
                [void]$builder.Append("  ")
                $state = "normal"
                $i += 2
                continue
            }
            if ( $ch -eq [char]10 -or $ch -eq [char]13 ) {
                [void]$builder.Append($ch)
            } else {
                [void]$builder.Append(" ")
            }
            $i++
            continue
        }

        if ( $state -eq "string" -or $state -eq "char" ) {
            if ( $ch -eq [char]10 -or $ch -eq [char]13 ) {
                [void]$builder.Append($ch)
                $state = "normal"
                $escape = $false
                $i++
                continue
            }

            [void]$builder.Append(" ")
            if ( $escape ) {
                $escape = $false
            } elseif ( $ch -eq [char]92 ) {
                $escape = $true
            } elseif ( ($state -eq "string" -and $ch -eq [char]34) -or ($state -eq "char" -and $ch -eq [char]39) ) {
                $state = "normal"
            }
            $i++
            continue
        }
    }

    return $builder.ToString()
}

function Get-LineText($text, $lineNumber)
{
    $lines = $text -split "`r?`n"
    if ( $lineNumber -lt 1 -or $lineNumber -gt $lines.Count ) {
        return ""
    }
    return $lines[$lineNumber - 1].Trim()
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

$specFiles = @(
    foreach ( $scanRoot in @("src") ) {
        $dir = Join-Path $Root $scanRoot
        if ( ! (Test-Path -LiteralPath $dir -PathType Container) ) {
            continue
        }

        Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object {
            $_.Extension -eq ".l" -or $_.Extension -eq ".y"
        }
    }
) | Sort-Object FullName

if ( $files.Count -lt $minSafetyCFiles ) {
    Add-Failure "C safety scan file baseline regressed: $($files.Count) < $minSafetyCFiles"
}

if ( $specFiles.Count -lt $minSafetySpecFiles ) {
    Add-Failure "lexer/parser spec scan baseline regressed: $($specFiles.Count) < $minSafetySpecFiles"
}

foreach ( $file in $files ) {
    $relative = $file.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1).Replace("\", "/")
    $rawText = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)

    $manualPrototypeMatches = @($rawText | Select-String -Pattern $manualCrtPrototypePattern -AllMatches)
    foreach ( $match in $manualPrototypeMatches ) {
        Add-Failure "manual CRT prototype in handwritten C/H source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
    }

    $matches = Select-String -LiteralPath $file.FullName -Pattern $unsafePattern
    foreach ( $match in $matches ) {
        Add-Failure "unsafe C/CRT API in handwritten source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
    }

    $staleMatches = Select-String -LiteralPath $file.FullName -Pattern $staleMarkerPattern -CaseSensitive:$false
    foreach ( $match in $staleMatches ) {
        Add-Failure "stale marker in handwritten C/H source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
    }

    if ( $relative.StartsWith("src/") ) {
        $consoleMatches = Select-String -LiteralPath $file.FullName -Pattern $directConsolePattern
        foreach ( $match in $consoleMatches ) {
            Add-Failure "direct console output in compiler source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
        }

        $commandMatches = Select-String -LiteralPath $file.FullName -Pattern $commandExecPattern
        foreach ( $match in $commandMatches ) {
            Add-Failure "direct CRT/Win32 command execution in compiler source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
        }

        $codeOnly = Remove-CCommentsAndStrings $rawText
        $processMatches = @($codeOnly | Select-String -Pattern $processControlPattern -AllMatches)
        foreach ( $match in $processMatches ) {
            Add-Failure "direct process control or assert in compiler source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
        }
    }

    if ( $relative -eq "src/driver/xl6_cli.c" ) {
        $timeMatches = Select-String -LiteralPath $file.FullName -Pattern $driverCrtTimePattern
        foreach ( $match in $timeMatches ) {
            Add-Failure "CRT time in CLI source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
        }
    }
}

foreach ( $file in $specFiles ) {
    $relative = $file.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1).Replace("\", "/")
    $staleMatches = Select-String -LiteralPath $file.FullName -Pattern $staleMarkerPattern -CaseSensitive:$false
    foreach ( $match in $staleMatches ) {
        Add-Failure "stale marker in handwritten lexer/parser source: ${relative}:$($match.LineNumber):$($match.Line.Trim())"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "C string safety ok: $($files.Count) handwritten/fixture C/H files, $($specFiles.Count) lexer/parser specs."
exit 0

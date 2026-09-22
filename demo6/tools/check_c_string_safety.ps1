param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

# Enforce the compiler project's strict CRT policy here.
# Native extlibs use their dependencies' C APIs and have package-local tests.
$scanRoots = @(
    "src"
    "release\include"
    "release\examples"
    "test\cases"
    "test\fixtures"
)
$minSafetyCFiles = 260
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
$staleMarkerPattern = '\b(TODO|FIXME|TBD)\b|code generation error|unsupported statement kind in C generator|unsupported expression kind in C generator|unsupported type in C generation|cannot box unsupported type into var|unsupported typed index compound assignment operator'
$directConsolePattern = '\b(printf|puts|putchar|fprintf|fputs|fputc)\s*\('
$driverCrtTimePattern = '\btime\s*\('
$commandExecPattern = '\b(system|popen|_popen|WinExec|ShellExecute[A-Za-z]*|CreateProcess[A-Za-z]*)\s*\('
# `abort()` is the canonical fail-stop for verified XLIR trap instructions and
# impossible resident-runtime ownership states.  It must remain observable and
# non-recoverable; direct `exit()` and debug-only `assert()` are not valid
# compiler/library control flow.
$processControlPattern = '\b(exit|assert)\s*\('
$manualCrtPrototypePattern = '(?m)^\s*(void|int|char\s*\*)\s+(exit|malloc|free|printf|fprintf)\s*\('
$mutableFunctionStaticArrayPattern = '(?m)^[ \t]+static\s+(?!const\b)[^;\r\n=]+\[[^\]\r\n]+\]\s*(?:=|;)'
$removedCgenInferencePattern = '\bxl6CgenInferExprType\b'
$rootFullPath = (Resolve-Path -LiteralPath $Root).Path

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

function Get-TextMatches($text, $pattern)
{
    $matches = [regex]::Matches(
        $text, $pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline)
    $lineNumber = 1
    $cursor = 0
    foreach ( $match in $matches ) {
        while ( $cursor -lt $match.Index ) {
            $newline = $text.IndexOf([char]10, $cursor)
            if ( $newline -lt 0 -or $newline -ge $match.Index ) { break }
            ++$lineNumber
            $cursor = $newline + 1
        }
        [pscustomobject]@{
            LineNumber = $lineNumber
            Value = $match.Value
        }
    }
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

    $manualPrototypeMatches = @(Get-TextMatches $rawText $manualCrtPrototypePattern)
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
        $codeOnly = Remove-CCommentsAndStrings $rawText
        $consoleMatches = @(Get-TextMatches $codeOnly $directConsolePattern)
        foreach ( $match in $consoleMatches ) {
            Add-Failure "direct console output in compiler source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
        }

        $commandMatches = @(Get-TextMatches $codeOnly $commandExecPattern)
        foreach ( $match in $commandMatches ) {
            Add-Failure "direct CRT/Win32 command execution in compiler source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
        }

        $processMatches = @(Get-TextMatches $codeOnly $processControlPattern)
        foreach ( $match in $processMatches ) {
            Add-Failure "direct process exit or assert in compiler source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
        }

        if ( $relative.StartsWith("src/cgen/") ) {
            $mutableStaticMatches = @(Get-TextMatches $rawText $mutableFunctionStaticArrayPattern)
            foreach ( $match in $mutableStaticMatches ) {
                Add-Failure "mutable function-static array in cgen source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
            }

            # Lowered XLIR 是代码生成阶段唯一的表达式类型来源，禁止恢复 C 字符串推断旁路。
            $removedInferenceMatches = @(Get-TextMatches $codeOnly $removedCgenInferencePattern)
            foreach ( $match in $removedInferenceMatches ) {
                Add-Failure "removed C-string type inference reintroduced in cgen source: ${relative}:$($match.LineNumber):$(Get-LineText $rawText $match.LineNumber)"
            }
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

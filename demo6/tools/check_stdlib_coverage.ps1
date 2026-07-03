param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$stdlibPath = Join-Path $Root "src\stdlib\xl6_stdlib.c"
$docDir = Join-Path $Root "docs"
$testDir = Join-Path $Root "test\cases"
$exampleDir = Join-Path $Root "release\examples"
$minPublicStdlibFunctions = 180

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Read-ManyUtf8Text($dir, $filter)
{
    $items = Get-ChildItem -Path $dir -Filter $filter -File | Sort-Object FullName
    $parts = New-Object System.Collections.Generic.List[string]

    foreach ( $item in $items ) {
        $parts.Add((Read-Utf8Text $item.FullName))
    }
    return [string]::Join("`n", $parts)
}

function Remove-XlangCommentsAndStrings($text)
{
    $builder = New-Object System.Text.StringBuilder
    $i = 0
    $length = $text.Length
    $blockDepth = 0

    while ( $i -lt $length ) {
        $ch = $text[$i]

        if ( $blockDepth -gt 0 ) {
            if ( $ch -eq "`n" ) {
                $null = $builder.Append($ch)
                $i++
                continue
            }

            if ( $i + 1 -lt $length -and $ch -eq "/" -and $text[$i + 1] -eq "*" ) {
                $blockDepth++
                $i += 2
                continue
            }

            if ( $i + 1 -lt $length -and $ch -eq "*" -and $text[$i + 1] -eq "/" ) {
                $blockDepth--
                $i += 2
                continue
            }

            $i++
            continue
        }

        if ( $i + 1 -lt $length -and $ch -eq "/" -and $text[$i + 1] -eq "/" ) {
            $i += 2
            while ( $i -lt $length -and $text[$i] -ne "`n" ) {
                $i++
            }
            if ( $i -lt $length ) {
                $null = $builder.Append($text[$i])
                $i++
            }
            continue
        }

        if ( $i + 1 -lt $length -and $ch -eq "/" -and $text[$i + 1] -eq "*" ) {
            $blockDepth = 1
            $i += 2
            continue
        }

        if ( $ch -eq "'" -or $ch -eq '"' ) {
            $quote = $ch

            if ( $i + 2 -lt $length -and $text[$i + 1] -eq $quote -and $text[$i + 2] -eq $quote ) {
                $i += 3
                while ( $i -lt $length ) {
                    if ( $text[$i] -eq "`n" ) {
                        $null = $builder.Append($text[$i])
                        $i++
                        continue
                    }
                    if ( $i + 2 -lt $length -and $text[$i] -eq $quote -and $text[$i + 1] -eq $quote -and $text[$i + 2] -eq $quote ) {
                        $i += 3
                        break
                    }
                    $i++
                }
                $null = $builder.Append(" ")
                continue
            }

            $i++
            while ( $i -lt $length ) {
                if ( $text[$i] -eq "`n" ) {
                    $null = $builder.Append($text[$i])
                    $i++
                    break
                }
                if ( $text[$i] -eq "\" -and $i + 1 -lt $length ) {
                    $i += 2
                    continue
                }
                if ( $text[$i] -eq $quote ) {
                    $i++
                    break
                }
                $i++
            }
            $null = $builder.Append(" ")
            continue
        }

        $null = $builder.Append($ch)
        $i++
    }

    return $builder.ToString()
}

function Read-ManyXlangCoverageText($dir, $filter)
{
    $items = Get-ChildItem -Path $dir -Filter $filter -File | Sort-Object FullName
    $parts = New-Object System.Collections.Generic.List[string]

    foreach ( $item in $items ) {
        $parts.Add((Remove-XlangCommentsAndStrings (Read-Utf8Text $item.FullName))) | Out-Null
    }
    return [string]::Join("`n", $parts)
}

function Print-Missing($title, $items)
{
    if ( $items.Count -eq 0 ) {
        return
    }

    Write-Host $title
    foreach ( $item in $items ) {
        Write-Host "  $item"
    }
}

function Add-Failure($items, $message)
{
    $items.Add($message) | Out-Null
}

$stdlib = Read-Utf8Text $stdlibPath
$docs = Read-ManyUtf8Text $docDir "*.md"
$tests = Read-ManyXlangCoverageText $testDir "stdlib_*.xl"
$examples = Read-ManyXlangCoverageText $exampleDir "stdlib_*.xl"

$funcs = New-Object System.Collections.Generic.List[string]
$consts = New-Object System.Collections.Generic.List[string]
$failures = New-Object System.Collections.Generic.List[string]
$paramArrayCounts = @{}
$paramArrayMatches = [regex]::Matches($stdlib, '(?s)static const XL6_StdlibParam\s+(\w+)\[\]\s*=\s*\{(.*?)\};')
$registrationMatches = [regex]::Matches($stdlib, '\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*("[^"]+"|NULL),\s*([^,]+),\s*([0-9]+),\s*"([^"]*)",')
$constMatches = [regex]::Matches($stdlib, '\{"([^"]+)",\s*"([^"]+)",\s*XL6_AST_[A-Z_]+,\s*"([^"]+)"\}')
$publicKeys = New-Object System.Collections.Generic.HashSet[string]
$internalNames = New-Object System.Collections.Generic.HashSet[string]
$cSymbols = New-Object System.Collections.Generic.HashSet[string]

foreach ( $match in $paramArrayMatches ) {
    $name = $match.Groups[1].Value
    $body = $match.Groups[2].Value
    $count = [regex]::Matches($body, '\{\s*"[^"]*"\s*,\s*"[^"]*"\s*\}').Count

    if ( $paramArrayCounts.ContainsKey($name) ) {
        Add-Failure $failures "duplicate stdlib param array: $name"
    } else {
        $paramArrayCounts[$name] = $count
    }
}

foreach ( $match in $registrationMatches ) {
    $ns = $match.Groups[1].Value
    $name = $match.Groups[2].Value
    $internalName = $match.Groups[3].Value
    $cSymbol = $match.Groups[4].Value.Trim('"')
    $paramsName = $match.Groups[5].Value.Trim()
    $paramCount = [int]$match.Groups[6].Value
    $returnType = $match.Groups[7].Value

    if ( ! $internalName.StartsWith("__xl_std_") ) {
        Add-Failure $failures "stdlib internal name must start with __xl_std_: $ns.$name -> $internalName"
    }
    if ( ! $internalNames.Add($internalName) ) {
        Add-Failure $failures "duplicate stdlib internal name: $internalName"
    }
    if ( $returnType -eq "void" ) {
        Add-Failure $failures "stdlib return type must not use void keyword: $ns.$name"
    }

    if ( $paramsName -eq "NULL" ) {
        if ( $paramCount -ne 0 ) {
            Add-Failure $failures "stdlib NULL params must have paramCount 0: $ns.$name"
        }
    } elseif ( ! $paramArrayCounts.ContainsKey($paramsName) ) {
        Add-Failure $failures "stdlib params array missing: $ns.$name -> $paramsName"
    } elseif ( $paramArrayCounts[$paramsName] -ne $paramCount ) {
        Add-Failure $failures "stdlib paramCount mismatch: $ns.$name -> $paramsName has $($paramArrayCounts[$paramsName]), registered $paramCount"
    }

    if ( $cSymbol -ne "NULL" -and ! $cSymbols.Add($cSymbol) ) {
        Add-Failure $failures "duplicate stdlib C symbol: $cSymbol"
    }

    if ( ! $ns.StartsWith("__") ) {
        if ( $ns -ne "" -and $ns -notmatch '^[a-z][a-z0-9]*(?:[.][a-z][a-z0-9]*)?$' ) {
            Add-Failure $failures "invalid public stdlib namespace: $ns"
        }
        if ( $name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$' ) {
            Add-Failure $failures "invalid public stdlib function name: $ns.$name"
        }

        $key = if ( $ns -eq "" ) { $name } else { "$ns.$name" }
        if ( ! $publicKeys.Add($key) ) {
            Add-Failure $failures "duplicate public stdlib function: $key"
        }
        $funcs.Add($key)
    }
}

foreach ( $match in $constMatches ) {
    $ns = $match.Groups[1].Value
    $name = $match.Groups[2].Value

    if ( $ns.StartsWith("__") ) {
        continue
    }
    if ( $ns -eq "" ) {
        Add-Failure $failures "public stdlib constant must be namespaced: $name"
    }
    if ( $ns -notmatch '^[a-z][a-z0-9]*(?:[.][a-z][a-z0-9]*)?$' ) {
        Add-Failure $failures "invalid public stdlib namespace: $ns"
    }
    if ( $name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$' ) {
        Add-Failure $failures "invalid public stdlib constant name: $ns.$name"
    }

    $key = "$ns.$name"
    if ( ! $publicKeys.Add($key) ) {
        Add-Failure $failures "duplicate public stdlib symbol: $key"
    }
    $consts.Add($key)
}

$rawPublicFuncs = $funcs | Sort-Object
$publicFuncs = $funcs | Sort-Object -Unique
$publicConsts = $consts | Sort-Object -Unique
$missingDocs = New-Object System.Collections.Generic.List[string]
$missingTests = New-Object System.Collections.Generic.List[string]
$missingExamples = New-Object System.Collections.Generic.List[string]

if ( $publicFuncs.Count -lt $minPublicStdlibFunctions ) {
    Add-Failure $failures "public stdlib function baseline regressed: $($publicFuncs.Count) < $minPublicStdlibFunctions"
}

foreach ( $func in $publicFuncs ) {
    $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($func) + '\s*\('

    if ( $docs -notmatch $pattern ) {
        $missingDocs.Add($func)
    }
    if ( $tests -notmatch $pattern ) {
        $missingTests.Add($func)
    }
    if ( $examples -notmatch $pattern ) {
        $missingExamples.Add($func)
    }
}

foreach ( $const in $publicConsts ) {
    $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($const) + '(?![A-Za-z0-9_\(])'

    if ( $docs -notmatch $pattern ) {
        $missingDocs.Add($const)
    }
    if ( $tests -notmatch $pattern ) {
        $missingTests.Add($const)
    }
    if ( $examples -notmatch $pattern ) {
        $missingExamples.Add($const)
    }
}

if ( $registrationMatches.Count -eq 0 ) {
    Add-Failure $failures "stdlib registration table entries not found"
}

if ( $rawPublicFuncs.Count -ne $publicFuncs.Count ) {
    Add-Failure $failures "public stdlib function count mismatch after de-duplication"
}

if ( $failures.Count -ne 0 -or $missingDocs.Count -ne 0 -or $missingTests.Count -ne 0 -or $missingExamples.Count -ne 0 ) {
    Write-Host "Standard library coverage check failed."
    Print-Missing "Registration failures:" $failures
    Print-Missing "Missing docs:" $missingDocs
    Print-Missing "Missing tests:" $missingTests
    Print-Missing "Missing examples:" $missingExamples
    exit 1
}

Write-Host "Standard library coverage ok: $($publicFuncs.Count) public functions."
exit 0

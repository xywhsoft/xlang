param(
    [string]$Root = "",
    [int]$ExpectedRuleCount = 834
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$cgenruleDir = Join-Path $Root "cgenrule"
$srcDir = Join-Path $Root "src"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Get-RuleIdFromLine($line)
{
    $m = [regex]::Match($line, '^## (CGEN-[A-Z]+-[0-9]+)')
    if ( $m.Success ) {
        return $m.Groups[1].Value
    }
    return $null
}

if ( ! (Test-Path -LiteralPath $cgenruleDir -PathType Container) ) {
    Write-Host "cgenrule docs directory missing: $cgenruleDir"
    exit 1
}

$docs = @(Get-ChildItem -LiteralPath $cgenruleDir -Filter "*.md" -File | Sort-Object Name)
if ( $docs.Count -eq 0 ) {
    Write-Host "No cgenrule markdown files found."
    exit 1
}

$ruleIds = New-Object System.Collections.Generic.HashSet[string]
$ruleLines = New-Object System.Collections.Generic.List[object]
$allTextParts = New-Object System.Collections.Generic.List[string]

foreach ( $doc in $docs ) {
    $text = Read-Utf8Text $doc.FullName
    $allTextParts.Add($text) | Out-Null

    if ( $text.Contains([char]0) ) {
        Add-Failure "NUL/control character found in $($doc.Name)"
    }

    $lines = $text -split "`r?`n"
    for ( $i = 0; $i -lt $lines.Count; $i++ ) {
        $id = Get-RuleIdFromLine $lines[$i]
        if ( $null -eq $id ) {
            continue
        }

        if ( ! $ruleIds.Add($id) ) {
            Add-Failure "duplicate rule id: $id at $($doc.Name):$($i + 1)"
        }
        $ruleLines.Add([pscustomobject]@{
            Id = $id
            File = $doc.Name
            Line = $i + 1
        }) | Out-Null
    }
}

if ( $ruleIds.Count -ne $ExpectedRuleCount ) {
    Add-Failure "rule definition count mismatch: expected $ExpectedRuleCount, actual $($ruleIds.Count)"
}

$allText = [string]::Join("`n", $allTextParts)

$stalePatterns = @(
    '852',
    'TODO',
    'TBD',
    'FIXME',
    'placeholder',
    '\u5F85\u586B',
    '\u672A\u6574\u7406',
    '\u5F85\u786E\u8BA4',
    '\u4E0B\u4E00\u6279\u6838\u9A8C\u8303\u56F4',
    '\u672C\u6279\u5F85\u8865\u5F3A\u70B9',
    '\u5269\u4F59\u9AD8\u4F18\u5148\u7EA7',
    'dependency dump',
    'return void',
    'xlUninit',
    'xlUnInit',
    '\.\.\\demo5\\release'
)

foreach ( $pattern in $stalePatterns ) {
    if ( $allText -match $pattern ) {
        Add-Failure "stale marker remains: $pattern"
    }
}

if ( $allText -notmatch '834\s*\u6761' ) {
    Add-Failure "README/summary rule count wording missing: 834 count text"
}

$definedRuleIds = New-Object System.Collections.Generic.HashSet[string]
foreach ( $id in $ruleIds ) {
    $definedRuleIds.Add($id) | Out-Null
}

foreach ( $doc in $docs ) {
    $text = Read-Utf8Text $doc.FullName

    foreach ( $match in [regex]::Matches($text, '(CGEN-[A-Z]+)-([0-9]+)\.\.([0-9]+)') ) {
        $prefix = $match.Groups[1].Value
        $start = [int]$match.Groups[2].Value
        $end = [int]$match.Groups[3].Value
        if ( $start -gt $end ) {
            Add-Failure "invalid descending rule range $($match.Value) in $($doc.Name)"
            continue
        }

        for ( $n = $start; $n -le $end; $n++ ) {
            $id = "{0}-{1:D3}" -f $prefix, $n
            if ( ! $definedRuleIds.Contains($id) ) {
                Add-Failure "rule range references missing id $id via $($match.Value) in $($doc.Name)"
            }
        }
    }

    foreach ( $match in [regex]::Matches($text, 'CGEN-[A-Z]+-[0-9]+') ) {
        $id = $match.Value
        if ( ! $definedRuleIds.Contains($id) ) {
            Add-Failure "rule reference missing definition: $id in $($doc.Name)"
        }
    }
}

$sourceFiles = @{}
Get-ChildItem -LiteralPath $srcDir -Recurse -Filter "*.c" -File | ForEach-Object {
    if ( ! $sourceFiles.ContainsKey($_.Name) ) {
        $sourceFiles[$_.Name] = $_.FullName
    }
}

$lineCountCache = @{}
foreach ( $doc in $docs ) {
    $text = Read-Utf8Text $doc.FullName
    foreach ( $match in [regex]::Matches($text, '(xl6_[A-Za-z0-9_]+\.c):([0-9]+)(?:-([0-9]+))?') ) {
        $name = $match.Groups[1].Value
        $start = [int]$match.Groups[2].Value
        $end = $start
        if ( $match.Groups[3].Success ) {
            $end = [int]$match.Groups[3].Value
        }

        if ( ! $sourceFiles.ContainsKey($name) ) {
            Add-Failure "source reference missing file: $name in $($doc.Name)"
            continue
        }

        $sourcePath = $sourceFiles[$name]
        if ( ! $lineCountCache.ContainsKey($sourcePath) ) {
            $lineCountCache[$sourcePath] = ([System.IO.File]::ReadAllLines($sourcePath)).Count
        }

        $lineCount = $lineCountCache[$sourcePath]
        if ( $start -lt 1 -or $end -lt $start -or $end -gt $lineCount ) {
            Add-Failure "source reference out of range: $($match.Value) in $($doc.Name), file lines=$lineCount"
        }
    }
}

if ( $failures.Count -ne 0 ) {
    Write-Host "cgenrule docs check failed."
    foreach ( $failure in ($failures | Select-Object -First 80) ) {
        Write-Host "  $failure"
    }
    if ( $failures.Count -gt 80 ) {
        Write-Host "  ... $($failures.Count - 80) more failure(s)"
    }
    exit 1
}

Write-Host "cgenrule docs ok: $($ruleIds.Count) unique rules, $($docs.Count) markdown files."
exit 0

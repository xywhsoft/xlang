param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$docsDir = Join-Path $Root "docs"
$readmePath = Join-Path $docsDir "README.md"
$matrixName = ([char]0x6587) + ([char]0x6863) + ([char]0x72B6) + ([char]0x6001) + ([char]0x77E9) + ([char]0x9635) + ".md"
$matrixPath = Join-Path $docsDir $matrixName
$settlementName = ([char]0x6587) + ([char]0x6863) + ([char]0x6574) + ([char]0x7406) + ([char]0x6536) + ([char]0x53E3) + ".md"
$settlementPath = Join-Path $docsDir $settlementName
$workspaceSnapshotName = "RC0" + ([char]0x5DE5) + ([char]0x4F5C) + ([char]0x533A) + ([char]0x6E05) + ([char]0x5355) + ".md"
$toolsDir = Join-Path $Root "tools"
$minDocsMarkdownFiles = 31
$minExtraMarkdownEntryDocs = 12
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

if ( ! (Test-Path -LiteralPath $docsDir -PathType Container) ) {
    Write-Host "docs directory missing: $docsDir"
    exit 1
}

if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
    Write-Host "docs README missing: $readmePath"
    exit 1
}

if ( ! (Test-Path -LiteralPath $matrixPath -PathType Leaf) ) {
    Write-Host "docs status matrix missing: $matrixPath"
    exit 1
}

if ( ! (Test-Path -LiteralPath $settlementPath -PathType Leaf) ) {
    Write-Host "docs settlement document missing: $settlementPath"
    exit 1
}

$readme = Read-Utf8Text $readmePath
$matrix = Read-Utf8Text $matrixPath
$settlement = Read-Utf8Text $settlementPath
$docs = @(Get-ChildItem -LiteralPath $docsDir -Filter "*.md" -File | Sort-Object Name)
$extraMarkdownDocs = @(
    "README.md",
    "cgenrule\README.md",
    "src\README.md",
    "tools\README.md",
    "release\include\README.md",
    "lib\README.md",
    "release\README.md",
    "release\examples\README.md",
    "release\examples\apps\README.md",
    "release\examples\feature_suite\README.md",
    "release\examples\tutorial\README.md",
    "release\projects\xs_static\README.md",
    "release\extlibs\README.md",
    "test\README.md"
)

if ( $docs.Count -lt $minDocsMarkdownFiles ) {
    Add-Failure "docs markdown file baseline regressed: $($docs.Count) < $minDocsMarkdownFiles"
}

if ( $extraMarkdownDocs.Count -lt $minExtraMarkdownEntryDocs ) {
    Add-Failure "extra markdown entry doc baseline regressed: $($extraMarkdownDocs.Count) < $minExtraMarkdownEntryDocs"
}

$knownReadmeDocs = New-Object System.Collections.Generic.HashSet[string]
$knownReadmeDocs.Add(("docs\README.md").ToLowerInvariant()) | Out-Null
foreach ( $relative in $extraMarkdownDocs ) {
    $knownReadmeDocs.Add($relative.ToLowerInvariant()) | Out-Null
}
$ignoredReadmePrefixes = @(
    "release\extlibs\",
    "build\tmp\",
    "build\test\",
    "release\tcc\",
    "lib\xrt\",
    "dev\xapi_legacy\"
)
$stalePatterns = @(
    '\bTODO\b',
    '\bTBD\b',
    '\bFIXME\b',
    'placeholder',
    '\u5F85\u586B',
    '\u672A\u6574\u7406',
    '\u5F85\u786E\u8BA4'
)

$forbiddenPolicyPatterns = @(
    '\.\.\\demo5\\release',
    'demo5\\release\\tcc\\lib\\libtcc1-64\.a',
    'release[\\/]tmp',
    'xlUninit',
    'xlUnInit',
    '`system`.*\u7528\u4E8E\u8C03\u7528\s*tcc',
    'XRT process.*`system`.*\u8C03\u7528',
    '`driver`.*\u547D\u4EE4\u884C\u62FC\u63A5.*`system`'
)
$staleCurrentCountPatterns = @(
    '271\s*\u4E2A\u6D4B\u8BD5',
    '251\s*\u4E2A',
    '27\s*\u4E2A\u8303\u4F8B',
    '`docs`\s*\uFF1A19\s*\u4E2A',
    '`packages`\s*\uFF1A9\s*\u4E2A',
    '\u5F53\u524D[^\r\n]{0,80}445\s*\u4E2A',
    '\u5F53\u524D[^\r\n]{0,80}829\s*\u4E2A',
    '\u6D4B\u8BD5\u7528\u4F8B\u57FA\u7EBF\s*\|[^\r\n]{0,40}\u5F53\u524D\u4E3A\s*445',
    'UTF-8\s*\u6587\u672C\u57FA\u7EBF\s*\|[^\r\n]{0,40}\u5F53\u524D\u4E3A\s*829',
    'Negative diagnostics artifacts ok:\s*246 files,\s*247 expected-failure commands',
    '\u4E3B\u56DE\u5F52\u5F53\u524D\u786E\u8BA4\s*246\s*\u4E2A\u8D1F\u4F8B\u8BCA\u65AD artifact',
    '\u4E3B\u56DE\u5F52\u5F53\u524D\u786E\u8BA4[^\r\n]{0,80}539\s*\u4E2A build/tmp',
    '\u4E3B\u56DE\u5F52\u5F53\u524D\u786E\u8BA4[^\r\n]{0,120}\u8D1F\u4F8B\u8BCA\u65AD artifact \u4E3A 246'
)

foreach ( $doc in $docs ) {
    $text = Read-Utf8Text $doc.FullName

    if ( $text.Contains([char]0) ) {
        Add-Failure "NUL/control character found in docs/$($doc.Name)"
    }

    if ( $doc.Name -ne "README.md" -and ! $readme.Contains($doc.Name) ) {
        Add-Failure "docs README does not reference docs/$($doc.Name)"
    }

    if ( ! $matrix.Contains($doc.Name) ) {
        Add-Failure "docs status matrix does not reference docs/$($doc.Name)"
    }

    foreach ( $pattern in $stalePatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "stale marker remains in docs/$($doc.Name): $pattern"
        }
    }

    foreach ( $pattern in $forbiddenPolicyPatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "forbidden stale policy wording in docs/$($doc.Name): $pattern"
        }
    }

    foreach ( $pattern in $staleCurrentCountPatterns ) {
        if ( $doc.Name -ne $workspaceSnapshotName -and $text -match $pattern ) {
            Add-Failure "stale current-count wording in docs/$($doc.Name): $pattern"
        }
    }

    if ( $doc.Name -eq $workspaceSnapshotName ) {
        if ( $text -notmatch 'release/examples' -or $text -notmatch 'release/extlibs' -or $text -notmatch '\u5F53\u524D RC0 \u57FA\u7EBF' ) {
            Add-Failure "RC0 workspace inventory must use current release/examples and release/extlibs asset roots"
        }
    }
}

foreach ( $relative in $extraMarkdownDocs ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "extra markdown document missing: $relative"
        continue
    }

    $text = Read-Utf8Text $path
    $display = $relative.Replace("\", "/")

    if ( $text.Contains([char]0) ) {
        Add-Failure "NUL/control character found in $display"
    }

    if ( ! $readme.Contains($display) ) {
        Add-Failure "docs README does not reference $display"
    }

    if ( ! $matrix.Contains($display) ) {
        Add-Failure "docs status matrix does not reference $display"
    }

    foreach ( $pattern in $stalePatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "stale marker remains in ${display}: $pattern"
        }
    }

    foreach ( $pattern in $forbiddenPolicyPatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "forbidden stale policy wording in ${display}: $pattern"
        }
    }

    foreach ( $pattern in $staleCurrentCountPatterns ) {
        if ( $text -match $pattern ) {
            Add-Failure "stale current-count wording in ${display}: $pattern"
        }
    }
}

$readmeDocs = @(Get-ChildItem -LiteralPath $Root -Filter "README.md" -File -Recurse | Sort-Object FullName)
$rootFullPath = (Resolve-Path -LiteralPath $Root).Path
if ( ! $rootFullPath.EndsWith("\") ) {
    $rootFullPath = $rootFullPath + "\"
}
foreach ( $doc in $readmeDocs ) {
    $docFullPath = (Resolve-Path -LiteralPath $doc.FullName).Path
    if ( ! $docFullPath.StartsWith($rootFullPath, [System.StringComparison]::OrdinalIgnoreCase) ) {
        Add-Failure "README document is outside root: $docFullPath"
        continue
    }

    $relative = $docFullPath.Substring($rootFullPath.Length)
    $normalized = $relative.Replace("/", "\")
    $lower = $normalized.ToLowerInvariant()
    $ignored = $false

    foreach ( $prefix in $ignoredReadmePrefixes ) {
        if ( $lower.StartsWith($prefix) ) {
            $ignored = $true
            break
        }
    }

    if ( $ignored ) {
        continue
    }

    if ( ! $knownReadmeDocs.Contains($lower) ) {
        Add-Failure "README document is not registered in docs index guard: $normalized"
    }
}

if ( ! $readme.Contains("../cgenrule/README.md") ) {
    Add-Failure "docs README does not link cgenrule README"
}

if ( Test-Path -LiteralPath $toolsDir -PathType Container ) {
    $checkTools = @(Get-ChildItem -LiteralPath $toolsDir -Filter "check_*.ps1" -File | Sort-Object Name)
    foreach ( $tool in $checkTools ) {
        if ( ! $settlement.Contains($tool.Name) ) {
            Add-Failure "docs settlement does not mention tools/$($tool.Name)"
        }
    }
}

if ( ! $settlement.Contains("rc0_guard.bat") -and ! $settlement.Contains("rc0_guard.ps1") ) {
    Add-Failure "docs settlement does not mention RC0 guard entry"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "docs index ok: $($docs.Count) docs markdown files, $($extraMarkdownDocs.Count) extra markdown entry docs."

param(
    [string]$Root = "",
    [int]$MinMarkdownFiles = 94
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Normalize-RelativePath($base, $path)
{
    $fullBase = [System.IO.Path]::GetFullPath($base)
    if ( ! $fullBase.EndsWith([System.IO.Path]::DirectorySeparatorChar) ) {
        $fullBase = $fullBase + [System.IO.Path]::DirectorySeparatorChar
    }

    $fullPath = [System.IO.Path]::GetFullPath($path)
    if ( ! $fullPath.StartsWith($fullBase, [System.StringComparison]::OrdinalIgnoreCase) ) {
        return $fullPath
    }

    return $fullPath.Substring($fullBase.Length).Replace("\", "/")
}

function Is-IgnoredMarkdownPath($rootFull, $path)
{
    $relative = Normalize-RelativePath $rootFull $path
    $lower = $relative.Replace("/", "\").ToLowerInvariant()

    return $lower.StartsWith("release\tmp\") -or
        $lower.StartsWith("release\tcc\") -or
        $lower.StartsWith("lib\xrt\")
}

function Get-MarkdownTargetPath($rawTarget)
{
    $target = $rawTarget.Trim()
    if ( $target.Length -eq 0 ) {
        return $null
    }

    if ( $target.StartsWith("<") ) {
        $end = $target.IndexOf(">")
        if ( $end -le 0 ) {
            return ""
        }
        $target = $target.Substring(1, $end - 1)
    } else {
        $match = [regex]::Match($target, '^[^\s]+')
        if ( ! $match.Success ) {
            return $null
        }
        $target = $match.Value
    }

    $target = $target.Trim()
    if ( $target.Length -eq 0 -or $target.StartsWith("#") ) {
        return $null
    }

    if ( $target -match '^[A-Za-z][A-Za-z0-9+.-]*:' -and $target -notmatch '^[A-Za-z]:[\\/]' ) {
        return $null
    }

    $fragment = $target.IndexOf("#")
    if ( $fragment -ge 0 ) {
        $target = $target.Substring(0, $fragment)
    }

    $query = $target.IndexOf("?")
    if ( $query -ge 0 ) {
        $target = $target.Substring(0, $query)
    }

    $target = $target.Trim()
    if ( $target.Length -eq 0 ) {
        return $null
    }

    try {
        $target = [System.Uri]::UnescapeDataString($target)
    } catch {
        return ""
    }

    return $target.Replace("/", "\")
}

function Test-MarkdownLinkTarget($rootFull, $doc, $lineNumber, $rawTarget)
{
    $targetPath = Get-MarkdownTargetPath $rawTarget
    if ( $null -eq $targetPath ) {
        return
    }

    $docRelative = Normalize-RelativePath $rootFull $doc.FullName
    if ( $targetPath.Length -eq 0 ) {
        Add-Failure "invalid markdown link target: ${docRelative}:$lineNumber -> $rawTarget"
        return
    }

    if ( [System.IO.Path]::IsPathRooted($targetPath) ) {
        $resolved = [System.IO.Path]::GetFullPath($targetPath)
    } else {
        $resolved = [System.IO.Path]::GetFullPath((Join-Path $doc.DirectoryName $targetPath))
    }

    $rootPrefix = $rootFull
    if ( ! $rootPrefix.EndsWith("\") ) {
        $rootPrefix = $rootPrefix + "\"
    }

    if ( ! $resolved.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase) ) {
        Add-Failure "markdown link leaves demo6 root: ${docRelative}:$lineNumber -> $rawTarget"
        return
    }

    if ( ! (Test-Path -LiteralPath $resolved) ) {
        Add-Failure "broken markdown link: ${docRelative}:$lineNumber -> $rawTarget"
    }
}

$rootFull = [System.IO.Path]::GetFullPath($Root)
$markdownFiles = @(
    Get-ChildItem -LiteralPath $rootFull -Filter "*.md" -File -Recurse |
        Where-Object { ! (Is-IgnoredMarkdownPath $rootFull $_.FullName) } |
        Sort-Object FullName
)

if ( $markdownFiles.Count -lt $MinMarkdownFiles ) {
    Add-Failure "markdown link scan baseline regressed: $($markdownFiles.Count) < $MinMarkdownFiles"
}

foreach ( $doc in $markdownFiles ) {
    $text = Read-Utf8Text $doc.FullName
    $lines = $text -split "`r?`n"
    $inFence = $false

    for ( $i = 0; $i -lt $lines.Count; $i++ ) {
        $line = $lines[$i]
        $lineNumber = $i + 1

        if ( $line -match '^\s*(```|~~~)' ) {
            $inFence = ! $inFence
            continue
        }

        if ( $inFence ) {
            continue
        }

        foreach ( $match in [regex]::Matches($line, '!?\[[^\]\r\n]*\]\((?<target>[^)\r\n]+)\)') ) {
            Test-MarkdownLinkTarget $rootFull $doc $lineNumber $match.Groups["target"].Value
        }

        foreach ( $match in [regex]::Matches($line, '^\s{0,3}\[[^\]\r\n]+\]:\s+(?<target>\S.*)$') ) {
            Test-MarkdownLinkTarget $rootFull $doc $lineNumber $match.Groups["target"].Value
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in ($failures | Select-Object -First 80) ) {
        Write-Host $failure
    }
    if ( $failures.Count -gt 80 ) {
        Write-Host "... $($failures.Count - 80) more failure(s)"
    }
    exit 1
}

Write-Host "markdown links ok: $($markdownFiles.Count) markdown files."
exit 0

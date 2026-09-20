param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$scanExtensions = @(
    ".c",
    ".h",
    ".l",
    ".y",
    ".xl",
    ".md",
    ".ps1",
    ".bat",
    ".json"
)
$minScannedTextFiles = 1547
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$looseUtf8 = [System.Text.Encoding]::UTF8
$reservedOutIdentifierPattern = '(?<![A-Za-z0-9_])(?:const\s+)?(?:auto|var|[A-Za-z_\p{L}][A-Za-z0-9_\p{L}]*(?:\s*<[^>\r\n]+>)?)\s+out(?=\s*(?:=|,|$))'

function Join-CodePoints($codes)
{
    $chars = New-Object System.Collections.Generic.List[char]
    foreach ( $code in $codes ) {
        $chars.Add([char]$code) | Out-Null
    }
    return -join $chars
}

$badTokens = @(
    [string][char]0xfffd,
    (Join-CodePoints @(0x951f, 0x65a4, 0x62f7))
)
$sourceMojibakeTokens = @(
    (Join-CodePoints @(0x6d93, 0x5d88, 0x5158)),
    (Join-CodePoints @(0x7eeb, 0x8bf2, 0x7037)),
    (Join-CodePoints @(0x9359, 0x509b, 0x669f)),
    (Join-CodePoints @(0x93c3, 0x72b3, 0x7876)),
    (Join-CodePoints @(0x6769, 0x6b13, 0x5677)),
    (Join-CodePoints @(0x748b, 0x51aa, 0x6564)),
    (Join-CodePoints @(0x701b, 0x6941, 0xe18c)),
    (Join-CodePoints @(0x9350, 0x546d, 0x74e8))
)

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}


# Mask literals and line comments before checking source identifiers.
function Get-XlangCodeLine($line)
{
    $code = New-Object System.Text.StringBuilder
    $quote = [char]0
    $escaped = $false

    for ( $i = 0; $i -lt $line.Length; ++$i ) {
        $current = $line[$i]

        if ( $quote -ne [char]0 ) {
            [void]$code.Append(' ')
            if ( $escaped ) {
                $escaped = $false
            } elseif ( $current -eq '\' ) {
                $escaped = $true
            } elseif ( $current -eq $quote ) {
                $quote = [char]0
            }
            continue
        }

        if ( ($current -eq '"') -or ($current -eq "'") ) {
            $quote = $current
            [void]$code.Append(' ')
            continue
        }

        if ( ($current -eq '/') -and (($i + 1) -lt $line.Length) -and ($line[$i + 1] -eq '/') ) {
            break
        }

        [void]$code.Append($current)
    }

    return $code.ToString()
}


$quotedOutProbe = Get-XlangCodeLine -line 'println("WARN: drain timed out, forcing stop")'
$codeOutProbe = Get-XlangCodeLine -line 'int out = 0'
if ( ($quotedOutProbe -match $reservedOutIdentifierPattern) -or
     ! ($codeOutProbe -match $reservedOutIdentifierPattern) ) {
    Add-Failure "reserved out identifier lexical guard self-test failed"
}


function Should-SkipFile($file)
{
    $relative = $file.FullName.Substring($Root.Length).TrimStart("\", "/")
    $normalized = $relative.Replace("\", "/")

    if ( $normalized.StartsWith("release/examples/", [System.StringComparison]::OrdinalIgnoreCase) -or
         $normalized.StartsWith("release/extlibs/", [System.StringComparison]::OrdinalIgnoreCase) -or
         $normalized.StartsWith("release/include/", [System.StringComparison]::OrdinalIgnoreCase) -or
         $normalized -eq "release/README.md" ) {
        return $false
    }

    if ( $normalized -eq "release" -or
         $normalized.StartsWith("release/", [System.StringComparison]::OrdinalIgnoreCase) ) {
        return $true
    }

    if ( $normalized -eq "build" -or
         $normalized.StartsWith("build/", [System.StringComparison]::OrdinalIgnoreCase) ) {
        return $true
    }

    return $false
}

if ( ! (Test-Path -LiteralPath $Root -PathType Container) ) {
    Write-Host "root directory missing: $Root"
    exit 1
}

$files = @(
    Get-ChildItem -LiteralPath $Root -Recurse -File | Where-Object {
        ($scanExtensions -contains $_.Extension) -and
        ! (Should-SkipFile $_)
    } | Sort-Object FullName
)

foreach ( $file in $files ) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $text = $null
    $relative = $file.FullName.Substring($Root.Length).TrimStart("\", "/").Replace("\", "/")
    $isHandwrittenSource = $relative.StartsWith("src/", [System.StringComparison]::OrdinalIgnoreCase)
    $isXlangSource = $file.Extension -eq ".xl"

    try {
        $text = $strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "invalid UTF-8: $($file.FullName)"
        $text = $looseUtf8.GetString($bytes)
    }

    $lines = $text -split "`r?`n"
    for ( $i = 0; $i -lt $lines.Count; ++$i ) {
        if ( $isXlangSource -and ($lines[$i] -match $reservedOutIdentifierPattern) ) {
            $codeLine = Get-XlangCodeLine -line ($lines[$i])
            if ( $codeLine -match $reservedOutIdentifierPattern ) {
                Add-Failure ("reserved parameter-mode keyword used as identifier in {0}:{1}: {2}" -f $file.FullName, ($i + 1), $lines[$i].Trim())
            }
        }
        foreach ( $token in $badTokens ) {
            if ( $lines[$i].Contains($token) ) {
                Add-Failure ("mojibake token in {0}:{1}: {2}" -f $file.FullName, ($i + 1), $lines[$i].Trim())
                break
            }
        }
        if ( $isHandwrittenSource ) {
            foreach ( $token in $sourceMojibakeTokens ) {
                if ( $lines[$i].Contains($token) ) {
                    Add-Failure ("misdecoded UTF-8 text in {0}:{1}: {2}" -f $file.FullName, ($i + 1), $lines[$i].Trim())
                    break
                }
            }
        }
    }
}

if ( $files.Count -lt $minScannedTextFiles ) {
    Add-Failure "source encoding scan baseline regressed: $($files.Count) < $minScannedTextFiles"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Source encoding ok: $($files.Count) text files."
exit 0

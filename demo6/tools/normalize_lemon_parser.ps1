param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = "Stop"
$resolved = (Resolve-Path -LiteralPath $Path).Path
$encoding = [System.Text.UTF8Encoding]::new($false)
$text = [System.IO.File]::ReadAllText($resolved, $encoding)
# Lemon 3.45 and earlier emit the unconditional destructor directly. Do not
# replace their stack engine with a newer template just to obtain this guard.
if (!$text.Contains('YY_MIN_DSTRCTR') -and
    [regex]::Matches($text, '(?m)^  yy_destructor\(pParser, yytos->major, &yytos->minor\);\r?$').Count -eq 1) {
    exit 0
}
$newline = if ( $text.Contains("`r`n") ) { "`r`n" } else { "`n" }
$pattern = '(?m)^    if\( yytos->major>=YY_MIN_DSTRCTR \)\{\r?\n' +
    '      yy_destructor\(pParser, yytos->major, &yytos->minor\);\r?\n' +
    '    \}'
$replacement = @(
    '#if YY_MIN_DSTRCTR > 0',
    '    if( yytos->major>=YY_MIN_DSTRCTR ){',
    '      yy_destructor(pParser, yytos->major, &yytos->minor);',
    '    }',
    '#else',
    '    yy_destructor(pParser, yytos->major, &yytos->minor);',
    '#endif'
) -join $newline

if ( $text.Contains($replacement) ) {
    exit 0
}
$matches = [regex]::Matches($text, $pattern)
if ( $matches.Count -ne 1 ) {
    throw "Generated Lemon parser does not contain the expected destructor guard"
}

[System.IO.File]::WriteAllText(
    $resolved,
    [regex]::Replace($text, $pattern, $replacement, 1),
    $encoding
)

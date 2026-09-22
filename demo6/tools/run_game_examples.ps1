param(
    [string]$Root = "",
    [switch]$FullCheck
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$gameDir = Join-Path $Root "release\examples\games"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

# NOTE: keep this file ASCII-only. Windows PowerShell 5.1 reads BOM-less
# scripts as ANSI/GBK; UTF-8 comment bytes can swallow the next newline
# and silently merge statements into comment lines.
$games = @(Get-ChildItem -LiteralPath $gameDir -File -Filter "*.xl" | Sort-Object Name)
if ( $games.Count -eq 0 ) {
    Write-Host "No game examples found in $gameDir"
    exit 1
}

# snake.xl exercises the full game stack (Input/Font/Shape/pixel APIs/
# setMaxFrames) and is fully semantic-checked with native compile.
$fullCheckSet = @("snake.xl")
if ( $FullCheck ) {
    # With -FullCheck every game runs a full `xl check`; each import of xge
    # triggers a native compile, so this is slow.
    $fullCheckSet = @($games | ForEach-Object { $_.Name })
}

$parseFail = 0
$checkFail = 0
$checked = 0

foreach ( $game in $games ) {
    if ( $fullCheckSet -contains $game.Name ) {
        Write-Host "Checking game (full): $($game.Name)"
        & $xl check $game.FullName
        if ( $LASTEXITCODE -ne 0 ) {
            $checkFail += 1
        }
        $checked += 1
    } else {
        # Other games get a parse-only syntax gate; semantic coverage of the
        # game stack is carried by snake.xl above.
        & $xl parse $game.FullName *> $null
        if ( $LASTEXITCODE -ne 0 ) {
            Write-Host "FAIL (parse): $($game.Name)"
            & $xl parse $game.FullName
            $parseFail += 1
        }
    }
}

Write-Host ""
Write-Host "Game examples: $($games.Count) files, full-checked: $checked, parse failures: $parseFail, check failures: $checkFail"

if ( $parseFail -gt 0 -or $checkFail -gt 0 ) {
    exit 1
}
Write-Host "Game examples check ok."
exit 0

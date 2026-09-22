param(
    [string]$Root = "",
    [switch]$FullCheck
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$exampleDir = Join-Path $Root "release\examples\xui"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

# Wires: release/examples/xui/*.xl
# NOTE: keep this file ASCII-only. Windows PowerShell 5.1 reads BOM-less
# scripts as ANSI/GBK; UTF-8 comment bytes can swallow the next newline
# and silently merge statements into comment lines.
$examples = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Sort-Object Name)
if ( $examples.Count -eq 0 ) {
    Write-Host "No XUI examples found in $exampleDir"
    exit 1
}

# all_controls.xl covers creation paths of every control and helper object,
# so it must run a full `xl check` (native C compile included).
$fullCheckSet = @("all_controls.xl")
if ( $FullCheck ) {
    # With -FullCheck every example runs a full `xl check`; each import of
    # xui triggers a native compile, so this is slow.
    $fullCheckSet = @($examples | ForEach-Object { $_.Name })
}

$parseFail = 0
$checkFail = 0
$checked = 0

foreach ( $example in $examples ) {
    if ( $fullCheckSet -contains $example.Name ) {
        Write-Host "Checking XUI example (full): $($example.Name)"
        & $xl check $example.FullName
        if ( $LASTEXITCODE -ne 0 ) {
            $checkFail += 1
        }
        $checked += 1
    } else {
        # Other examples get a parse-only syntax gate; semantic coverage is
        # carried by all_controls.xl above.
        & $xl parse $example.FullName *> $null
        if ( $LASTEXITCODE -ne 0 ) {
            Write-Host "FAIL (parse): $($example.Name)"
            & $xl parse $example.FullName
            $parseFail += 1
        }
    }
}

Write-Host ""
Write-Host "XUI examples: $($examples.Count) files, full-checked: $checked, parse failures: $parseFail, check failures: $checkFail"

if ( $parseFail -gt 0 -or $checkFail -gt 0 ) {
    exit 1
}
Write-Host "XUI examples check ok."
exit 0

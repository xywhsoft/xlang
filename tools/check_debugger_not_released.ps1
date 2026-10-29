param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl.exe missing: $xl"
    exit 1
}

$help = & $xl --help 2>&1 | Out-String
if ( $LASTEXITCODE -ne 0 ) {
    Write-Host "xl --help failed"
    exit 1
}
if ( $help -match '(?m)^\s*xl\s+(dbg|dap)\b' ) {
    Write-Host "unreleased debugger command is still advertised"
    exit 1
}

foreach ( $command in @("dbg", "dap") ) {
    $oldErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $xl $command 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $oldErrorAction
    if ( $exitCode -eq 0 ) {
        Write-Host "unreleased debugger command unexpectedly succeeded: $command"
        exit 1
    }
    if ( ! $output.Contains("unknown command: $command") ) {
        Write-Host "unreleased debugger command did not use the ordinary unknown-command path: $command"
        exit 1
    }
}

$driver = [IO.File]::ReadAllText((Join-Path $Root "src\driver\xl6_xlc_shell.c"), [Text.Encoding]::UTF8)
$build = [IO.File]::ReadAllText((Join-Path $Root "build.bat"), [Text.Encoding]::UTF8)
if ( $driver.Contains("xl6DapMain") -or $build.Contains("src\debug\xl6_dap.c") ) {
    Write-Host "synthetic DAP adapter remains wired into the release CLI"
    exit 1
}

Write-Host "Debugger release boundary ok: dbg/dap are not first-release commands."
exit 0

param(
    [Parameter(Mandatory = $true)]
    [string]$Xl,

    [Parameter(Mandatory = $true)]
    [string]$Script,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $Xl -PathType Leaf)) {
    throw "xl.exe is missing: $Xl"
}
if (!(Test-Path -LiteralPath $Script -PathType Leaf)) {
    throw "XLang example is missing: $Script"
}
if (!(Test-Path -LiteralPath $WorkingDirectory -PathType Container)) {
    throw "XLang example working directory is missing: $WorkingDirectory"
}

# 范例可能使用相对于 release 的资源路径，必须同时设置两种当前目录。
Set-Location -LiteralPath $WorkingDirectory
[System.Environment]::CurrentDirectory = $WorkingDirectory
& $Xl run $Script
exit $LASTEXITCODE

[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [switch]$Tests,
    [string]$ToolchainDirectory=''
)
$ErrorActionPreference='Stop'
$python = (Get-Command python -ErrorAction Stop).Source
$arguments = @('-X','utf8',(Join-Path $PSScriptRoot 'build_current.py'),'--configuration',$Configuration)
if ($Tests) { $arguments += '--tests' }
if ($ToolchainDirectory) { $arguments += @('--toolchain-directory',$ToolchainDirectory) }
& $python @arguments
exit $LASTEXITCODE

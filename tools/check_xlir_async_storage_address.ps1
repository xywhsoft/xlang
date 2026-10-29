param([string]$Root = '', [string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
# Retain this historical gate entry point, not its removed Channel Into ABI.
# The source gate checks receiveAfterAwait's owning result inside an async
# machine, rejects receive storage-address operands and executes real AOT/JIT.
& "$PSScriptRoot/check_xlir_channel_resource_abi.ps1" -Root $Root -PublicDirectory $PublicDirectory -Directory $Directory
if (!$?) { throw 'Async owning Channel receive gate failed' }

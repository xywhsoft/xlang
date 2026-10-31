[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [string]$CServerRoot,
    [Parameter(Mandatory=$true)][string]$CompilerManifest,
    [string]$CServerManifest,
    [switch]$WithCLsp,
    [string]$Output,
    [string]$SdkSourceRoot,
    [string]$CompilerSourceManifest,
    [string]$CompilerSourceBuildReport
)
$ErrorActionPreference='Stop'
$arguments=@((Join-Path $PSScriptRoot 'package_portable.py'),'--compiler-root',$CompilerRoot,'--compiler-manifest',$CompilerManifest)
if ($WithCLsp) {
    if (-not $CServerRoot -or -not $CServerManifest) { throw 'WithCLsp requires an explicit C server and manifest.' }
    $arguments+=@('--with-c-lsp','--c-server-root',$CServerRoot,'--c-server-manifest',$CServerManifest)
}
if ($Output) { $arguments+=@('--output',$Output) }
if ($SdkSourceRoot) { $arguments+=@('--sdk-source-root',$SdkSourceRoot) }
if ($CompilerSourceManifest) { $arguments+=@('--compiler-source-manifest',$CompilerSourceManifest) }
if ($CompilerSourceBuildReport) { $arguments+=@('--compiler-source-build-report',$CompilerSourceBuildReport) }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Portable candidate assembly failed; inspect the preserved stage if one was created.' }

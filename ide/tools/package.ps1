[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [Parameter(Mandatory=$true)][string]$CServerRoot,
    [Parameter(Mandatory=$true)][string]$CompilerManifest,
    [Parameter(Mandatory=$true)][string]$CServerManifest,
    [string]$Output,
    [string]$SdkSourceRoot,
    [string]$CompilerSourceManifest,
    [string]$CompilerSourceBuildReport
)
$ErrorActionPreference='Stop'
$arguments=@((Join-Path $PSScriptRoot 'package_portable.py'),'--compiler-root',$CompilerRoot,'--c-server-root',$CServerRoot,'--compiler-manifest',$CompilerManifest,'--c-server-manifest',$CServerManifest)
if ($Output) { $arguments+=@('--output',$Output) }
if ($SdkSourceRoot) { $arguments+=@('--sdk-source-root',$SdkSourceRoot) }
if ($CompilerSourceManifest) { $arguments+=@('--compiler-source-manifest',$CompilerSourceManifest) }
if ($CompilerSourceBuildReport) { $arguments+=@('--compiler-source-build-report',$CompilerSourceBuildReport) }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Portable candidate assembly failed; inspect the preserved stage if one was created.' }

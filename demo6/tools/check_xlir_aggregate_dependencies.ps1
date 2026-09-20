param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'check_xlir_mixed_dependencies.ps1') -PublicDirectory $PublicDirectory -ContractDirectory $ContractDirectory -Directory $Directory -Aggregate

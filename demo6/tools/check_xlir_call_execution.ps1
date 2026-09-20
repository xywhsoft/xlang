param([string]$Root = '', [string]$ContractDriver = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($ContractDriver -eq '') {
        $ContractDriver = 'build/test/xlir_call_execution_contract.exe'
        & tools/build_xlir_validation_driver.ps1 -Driver test/cases/xlir_call_execution_contract.c -Output $ContractDriver
        if ($LASTEXITCODE -ne 0) { throw 'Call execution contract build failed' }
    }
    & $ContractDriver
    if ($LASTEXITCODE -ne 0) { throw 'Call execution contract failed' }
} finally { Pop-Location }
Write-Output 'Call execution metadata passed; dispatch and blocking-admission lowering are not yet implemented.'

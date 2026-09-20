param([string]$Root = '', [string]$ContractDriver = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($ContractDriver -eq '') {
        & tools/check_xlir_module_deactivation.ps1
        if ($LASTEXITCODE -ne 0) { throw 'Module API source contract build failed' }
        $ContractDriver = 'build/test/xlir_module_deactivation.exe'
    }
    & $ContractDriver --retirement
    if ($LASTEXITCODE -ne 0) { throw 'Module deletion requested despite checked cleanup failure; retirement is not admitted' }
} finally { Pop-Location }
Write-Output 'Host/runtime retirement contract passed; TCC deletion is recorded, dependency unload is real. Generated-language code-owner binding/dispatch and arbitrary escaped callbacks are not yet admitted.'

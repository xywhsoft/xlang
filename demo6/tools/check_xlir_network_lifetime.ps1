param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
& python (Join-Path $PSScriptRoot 'check_stdlib_network_services.py') --root $projectRoot `
    --source-build (Join-Path $PublicDirectory 'build.json') --out $Directory --jobs 1
if($LASTEXITCODE-ne 0){throw 'Explicit managed network service ownership and retirement gate failed'}

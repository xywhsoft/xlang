param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$build=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
$runtime=@($build.Inputs|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xl.dll'})
if($runtime.Count-ne 1){throw 'One exact public-build runtime dependency is required'}
& python (Join-Path $PSScriptRoot 'check_network_retirement_source.py') --root $projectRoot `
    --public-directory $PublicDirectory --runtime $runtime[0].Path --out $Directory --contracts-only
if($LASTEXITCODE-ne 0){throw 'Current native Future / managed network contract gate failed'}

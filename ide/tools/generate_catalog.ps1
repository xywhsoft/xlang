[CmdletBinding()]
param([string]$PythonPath,[string]$CompilerPath,[string]$ProfilePath,[string]$DocumentationRoot,[string]$OutputDirectory,[string]$GccPath,[switch]$ExtensionsOnly,[switch]$PrepareOnly,[switch]$Recover)
$ErrorActionPreference='Stop'
$ideRoot=Split-Path -Parent $PSScriptRoot
if (-not $PythonPath) { $PythonPath='C:\Program Files\Python312\python.exe' }
if (-not $ProfilePath) { $ProfilePath=Join-Path $ideRoot 'data\portable-build-inputs.json' }
if ($CompilerPath -and -not $ExtensionsOnly) {
    $profile=Get-Content -LiteralPath $ProfilePath -Raw -Encoding UTF8 | ConvertFrom-Json
    $pinned=Join-Path (Join-Path $ideRoot $profile.compiler_root) 'xl.exe'
    if ([IO.Path]::GetFullPath($CompilerPath) -ne [IO.Path]::GetFullPath($pinned)) { throw 'CompilerPath must match the pinned compiler in ProfilePath; source and compiler evidence must agree.' }
}
$dataRoot=Join-Path $ideRoot 'data'
if ($Recover) {
    & $PythonPath -X utf8 (Join-Path $PSScriptRoot 'catalog_publish.py') --data $dataRoot --recover
    exit $LASTEXITCODE
}
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $ideRoot ('build\cg-'+[Guid]::NewGuid().ToString('N').Substring(0,8)) }
if ($ExtensionsOnly) {
    if ($CompilerPath -or $DocumentationRoot) {throw 'ExtensionsOnly reads the current workspace frontend; CompilerPath and DocumentationRoot do not apply.'}
    $arguments=@('-X','utf8','-B',(Join-Path $PSScriptRoot 'refresh_extensions.py'),'--root',(Split-Path -Parent $ideRoot),'--output',$OutputDirectory)
} else {
    $arguments=@('-X','utf8','-B',(Join-Path $PSScriptRoot 'generate_catalog_snapshot.py'),'--profile',$ProfilePath,'--output',$OutputDirectory)
    if ($DocumentationRoot) { $arguments+=@('--documentation-root',$DocumentationRoot) }
}
if ($GccPath) { $arguments+=@('--gcc',$GccPath) }
& $PythonPath @arguments
if ($LASTEXITCODE -ne 0) { throw 'Catalog/source pair generation failed; installed data is preserved.' }
if ($PrepareOnly) { Write-Output ('Validated pair prepared in '+$OutputDirectory); exit 0 }
& $PythonPath -X utf8 (Join-Path $PSScriptRoot 'catalog_publish.py') --pair $OutputDirectory --data $dataRoot
if ($LASTEXITCODE -ne 0) { throw 'Catalog publication failed; retain its journal and run -Recover after resolving the reported condition.' }
Write-Output 'Installed verified command catalog and matching source snapshots.'

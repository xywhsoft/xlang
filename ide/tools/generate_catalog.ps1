[CmdletBinding()]
param([string]$PythonPath='python',[string]$Root,[string]$ReleaseReportPath,[string]$OutputDirectory,[string]$GccPath,[switch]$ExtensionsOnly,[switch]$PrepareOnly,[switch]$Recover,[string]$ReleaseDirectory,[string]$CompilerSdkPath)
$ErrorActionPreference='Stop'
$ideRoot=Split-Path -Parent $PSScriptRoot
if (-not $Root) { $Root=Split-Path -Parent $ideRoot }
$dataRoot=Join-Path $ideRoot 'data'
if ($Recover) {
    & $PythonPath -X utf8 (Join-Path $PSScriptRoot 'catalog_publish.py') --data $dataRoot --recover
    exit $LASTEXITCODE
}
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $ideRoot ('build\cg-'+[Guid]::NewGuid().ToString('N').Substring(0,8)) }
if ($ExtensionsOnly) {
    if ($ReleaseReportPath -or $CompilerSdkPath) {throw 'ExtensionsOnly uses the installed standard-library binding.'}
    $arguments=@('-B','-X','utf8',(Join-Path $PSScriptRoot 'refresh_extensions.py'),'--root',$Root,'--output',$OutputDirectory)
} else {
    $arguments=@('-B','-X','utf8',(Join-Path $PSScriptRoot 'generate_catalog_snapshot.py'),'--root',$Root,'--output',$OutputDirectory)
    if ($ReleaseReportPath) { $arguments+=@('--release-report',$ReleaseReportPath) }
    if ($CompilerSdkPath) { $arguments+=@('--compiler-sdk',$CompilerSdkPath) }
}
if ($ReleaseDirectory) { $arguments+=@('--release-directory',$ReleaseDirectory) }
if ($GccPath) { $arguments+=@('--gcc',$GccPath) }
& $PythonPath @arguments
if ($LASTEXITCODE -ne 0) { throw 'Catalog/source pair generation failed; installed data is preserved.' }
$coverage=@('--root',$Root,'--catalog',(Join-Path $OutputDirectory 'commands.json'))
if ($ReleaseDirectory) { $coverage+=@('--release-directory',$ReleaseDirectory) }
& $PythonPath -X utf8 (Join-Path $PSScriptRoot 'check_catalog_coverage.py') @coverage
if ($LASTEXITCODE -ne 0) { throw 'Catalog public API coverage or current release binding failed.' }
if ($PrepareOnly) { Write-Output ('Validated pair prepared in '+$OutputDirectory); exit 0 }
& $PythonPath -X utf8 (Join-Path $PSScriptRoot 'catalog_publish.py') --pair $OutputDirectory --data $dataRoot
if ($LASTEXITCODE -ne 0) { throw 'Catalog publication failed; retain its journal and run -Recover after resolving the reported condition.' }
Write-Output 'Installed verified command catalog and matching source snapshots.'

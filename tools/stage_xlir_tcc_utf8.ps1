param([Parameter(Mandatory=$true)][string]$Compiler,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$root=Split-Path -Parent $PSScriptRoot
$compilerPath=(Resolve-Path -LiteralPath $Compiler).Path
$sourceRoot=Split-Path -Parent $compilerPath
$output=[IO.Path]::GetFullPath($Directory)
if (Test-Path -LiteralPath $output) { throw 'Preserve the existing staged toolchain' }
if ($output.StartsWith($sourceRoot.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
    $output.Equals($sourceRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'The staged toolchain must be outside the source installation' }
$reparse=@(Get-ChildItem -LiteralPath $sourceRoot -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint })
if ($reparse.Count) { throw 'Do not copy a toolchain through reparse points' }
$manifest=Join-Path $PSScriptRoot 'tcc_utf8.manifest'
$fixture=Join-Path $root 'test/cases/xlir_native_response_contract.c'
$files=@(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File)
$records=@($files | ForEach-Object { [ordered]@{Source=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$control=@($PSCommandPath,$script:XlirNativeProcessImplementation,$manifest,$fixture) |
    ForEach-Object { [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash} }
[IO.Directory]::CreateDirectory($output) | Out-Null
foreach($record in $records){
    $relative=Get-XlirContainedRelativePath $sourceRoot $record.Source
    $target=Join-Path $output $relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    Copy-Item -LiteralPath $record.Source -Destination $target
    if((Get-FileHash -LiteralPath $target).Hash-cne$record.Sha256){throw 'Toolchain copy changed bytes'}
    $record.Copied=$target
}
$stagedCompiler=Join-Path $output ([IO.Path]::GetFileName($compilerPath))
$sidecar=$stagedCompiler+'.manifest'
if(Test-Path -LiteralPath $sidecar){
    if((Get-FileHash -LiteralPath $sidecar).Hash-cne(Get-FileHash -LiteralPath $manifest).Hash){throw 'Preserve the source manifest; select an already UTF-8 capable toolchain or a fresh manifest-free source'}
}else{Copy-Item -LiteralPath $manifest -Destination $sidecar}
# Prove the loader actually applies the sidecar, including the output filename.
# An embedded manifest can take precedence; never assume the sidecar succeeded.
$unicode=-join([char]0x6D4B,[char]0x8BD5)
$probe=Join-Path $output ('probe spaces '+$unicode)
[IO.Directory]::CreateDirectory($probe) | Out-Null
$binary=Join-Path $probe 'probe.exe';$log=Join-Path $probe 'build.log'
$build=Invoke-XlirTccToLog $stagedCompiler @('-std=c11','-Wall','-Werror','-DXL6_NATIVE_DEFINE="utf8"',$fixture,'-o',$binary) $log
if($build-ne0){throw 'The selected TCC did not pass process-local UTF-8 output-path compilation; no global locale change was made'}
$run=Invoke-XlirNativeToLog $binary @() (Join-Path $probe 'run.log')
if($run-ne0-or[IO.File]::ReadAllText((Join-Path $probe 'run.log')).Trim()-cne'4:75746638'){throw 'Staged TCC probe did not preserve C bytes'}
foreach($record in $records){
    if((Get-FileHash -LiteralPath $record.Source).Hash-cne$record.Sha256-or
       (Get-FileHash -LiteralPath $record.Copied).Hash-cne$record.Sha256){throw 'Source or copied toolchain changed during staging'}
}
foreach($record in $control){if((Get-FileHash -LiteralPath $record.Path).Hash-cne$record.Sha256){throw 'Staging control input changed'}}
$result=[ordered]@{Complete=$true;Compiler=$stagedCompiler;SourceCompiler=$compilerPath;Files=$records;Inputs=@($control);
    Manifest=$sidecar;ManifestSha256=(Get-FileHash -LiteralPath $sidecar).Hash;Build=$build;Run=$run;
    CompilerCodeUnmodified=$true;InstallationUnmodified=$true;ProcessLocalUtf8Proved=$true;SystemLocaleChanged=$false}
[IO.File]::WriteAllText((Join-Path $output 'setup.json'),($result | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
Write-Output ([pscustomobject]$result)

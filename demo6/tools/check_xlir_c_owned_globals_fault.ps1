param([Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$Compiler,
    [Parameter(Mandatory=$true)][string]$PublicDriver)
$ErrorActionPreference='Stop'
$outputPath=if([IO.Path]::IsPathRooted($Directory)) {
    [IO.Path]::GetFullPath($Directory)
} else {
    [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Directory))
}
if(Test-Path -LiteralPath $outputPath){throw 'Use a fresh global fault validation directory'}
New-Item -ItemType Directory -Path $outputPath | Out-Null
$driverDirectory=Split-Path (Resolve-Path -LiteralPath $PublicDriver).Path
$manifest=Get-Content -LiteralPath "$driverDirectory/build.json" -Raw | ConvertFrom-Json
$objects=@($manifest.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o'} | ForEach-Object Path)
if(!$manifest.SourceXrt -or !$objects.Count){throw 'Global fault validation requires current source runtime objects'}
Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
$generated=Join-Path $outputPath 'globals-fault.c'
& $Compiler test/cases/xlir_c_owned_globals_fault.xl $generated
if($LASTEXITCODE){throw 'Global fault program compilation failed'}
$harnessPath=(Resolve-Path test/cases/xlir_c_owned_globals_fault_host.c).Path.Replace('\','/')
$wrapper=Join-Path $outputPath 'host.c'
[IO.File]::WriteAllText($wrapper,('#define XL6_C_OWNED_GLOBALS_PROGRAM "'+$generated.Replace('\','/')+'"'+"`n"+'#include "'+$harnessPath+'"'+"`n"),[Text.UTF8Encoding]::new($false))
$flags=@('-std=c11','-Wall','-Wextra','-Werror','-O0','-ffunction-sections','-fdata-sections',
    '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation',"-I$driverDirectory/include")
$arguments=@($flags)+@($wrapper)+$objects+@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-Wl,--gc-sections','-o',"$outputPath/probe.exe")
$response=Join-Path $outputPath 'probe.rsp'
[IO.File]::WriteAllText($response,(@($arguments | ForEach-Object {'"'+$_.Replace('\','/').Replace('"','\"')+'"'}) -join "`n"),[Text.UTF8Encoding]::new($false))
& gcc ('@'+$response)
if($LASTEXITCODE){throw 'Global fault host build failed'}
$failed=0;$succeeded=0
foreach($step in 0..31){
    & "$outputPath/probe.exe" $step
    if($LASTEXITCODE -eq 0){$failed++}
    elseif($LASTEXITCODE -eq 10){$succeeded++}
    else{throw "Global fault probe $step failed: $LASTEXITCODE"}
}
if(!$failed -or !$succeeded){throw 'Global fault sweep must exercise failures and successful completion'}
[pscustomobject]@{Steps=32;InjectedFailures=$failed;Successes=$succeeded} | ConvertTo-Json | Set-Content -LiteralPath "$outputPath/result.json" -Encoding utf8
Write-Output "C owner global initialization faults: $failed failures and $succeeded completions; all C allocations released"

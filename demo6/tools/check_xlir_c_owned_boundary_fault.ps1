param([Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$Compiler,
    [Parameter(Mandatory=$true)][string]$PublicDriver)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$work=[IO.Path]::GetFullPath((Join-Path $root $Directory))
if(Test-Path -LiteralPath $work){throw 'Use a fresh callable boundary fault directory'}
[IO.Directory]::CreateDirectory($work) | Out-Null
$compilerPath=(Resolve-Path -LiteralPath $Compiler).Path
$publicPath=Split-Path (Resolve-Path -LiteralPath $PublicDriver).Path
$public=Get-Content -LiteralPath "$publicPath/build.json" -Raw | ConvertFrom-Json
if(!$public.SourceXrt -or !$public.Complete){throw 'A complete source runtime build is required'}
$objects=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o'} | ForEach-Object Path)
$checks=[Collections.Generic.List[object]]::new()
$utf8=[Text.UTF8Encoding]::new($false)
Push-Location $root
try {
    Copy-Item -LiteralPath release/xl.dll -Destination "$work/xl.dll"
    & $compilerPath test/cases/xlir_c_owned_boundary_fault.xl "$work/program.c"
    if($LASTEXITCODE){throw 'Callable boundary fault source compilation failed'}
    $hostSource=(Resolve-Path test/cases/xlir_c_owned_boundary_fault_host.c).Path.Replace('\','/')
    [IO.File]::WriteAllText("$work/host.c",('#define XL6_C_OWNED_BOUNDARY_PROGRAM "'+"$work/program.c".Replace('\','/')+'"'+"`n"+'#include "'+$hostSource+'"'+"`n"),$utf8)
    foreach($optimization in @('O0','O2')) {
        $flags=@('-std=c11',"-$optimization",'-ffunction-sections','-fdata-sections',
            "-I$publicPath/include",'-Irelease/include',"$work/host.c")+$objects+
            @('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32',
                '-Wl,--gc-sections','-Wl,--no-keep-memory','-o',"$work/$optimization.exe")
        [IO.File]::WriteAllText("$work/$optimization.rsp",(@($flags | ForEach-Object {'"'+$_.Replace('\','/').Replace('"','\"')+'"'}) -join "`n"),$utf8)
        & gcc ("@$work/$optimization.rsp")
        if($LASTEXITCODE){throw "Callable boundary $optimization host build failed"}
        $log=@(& "$work/$optimization.exe" 2>&1)
        $exitCode=$LASTEXITCODE
        $log | Set-Content -LiteralPath "$work/$optimization.log" -Encoding utf8
        $log | Write-Output
        $checks.Add([ordered]@{optimization=$optimization;exitCode=$exitCode;log="$work/$optimization.log"})
        if($exitCode){throw "Callable boundary $optimization fault sweep failed"}
    }
} finally {
    [ordered]@{publicBuild="$publicPath/build.json";compiler=$compilerPath;checks=@($checks.ToArray())} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$work/results.json" -Encoding utf8
    Pop-Location
}

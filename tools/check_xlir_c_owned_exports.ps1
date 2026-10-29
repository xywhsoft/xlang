param(
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$publicPath=(Resolve-Path -LiteralPath $PublicDirectory).Path
$public=Get-Content -LiteralPath "$publicPath/build.json" -Raw | ConvertFrom-Json
if (!$public.SourceXrt -or !$public.Complete) { throw 'A complete source-runtime build is required' }
$work=if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) } else { Join-Path $root $Directory }
if (Test-Path -LiteralPath $work) { throw 'Use a fresh C owner export validation directory' }
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$objects=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o'} | ForEach-Object Path)
$checks=[Collections.Generic.List[object]]::new()
$files=@($PSCommandPath,"$root/test/cases/xlir_c_owned_exports.xl","$root/test/cases/xlir_c_owned_import_exports.xl",
    "$root/test/cases/xlir_c_owned_export_contract.c","$root/test/cases/xlir_c_owned_export_host.c","$root/test/cases/xlir_c_owned_native.c")+
    @(Get-ChildItem -LiteralPath "$root/test/fixtures/c_owned_exports" -File -Recurse | ForEach-Object FullName)
$inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($files | ForEach-Object {
    [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}
})
function Unchanged {
    foreach ($input in $inputs) {
        if ((Get-FileHash -LiteralPath $input.Path).Hash -cne $input.Sha256) { throw "Changed input: $($input.Path)" }
    }
}
function Check([string]$name,[string]$program,[string[]]$arguments) {
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root 900
    $checks.Add([ordered]@{name=$name;result=$result})
    if ($result.status -ne 'pass') { throw "$name failed: $($result.stdout) $($result.stderr)" }
    Write-Host "[C owner exports] ${name}: PASS $($result.stdout.Trim())"
}
function Response([string]$name,[string[]]$arguments) {
    $path=Join-Path $work "$name.rsp"
    [IO.File]::WriteAllText($path,(($arguments | ForEach-Object {'"'+$_.Replace('\','/').Replace('"','\"')+'"'}) -join "`n"),$utf8)
    Check $name $gcc @('@'+$path)
}
$flags=@('-std=c11','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',
    '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Irelease',"-I$publicPath/include",'-Irelease/include')
$links=@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-Wl,--gc-sections','-Wl,--no-keep-memory')
$oldPackagePath=$env:XLANG_PACKAGE_PATH
$outcome='failed'
Unchanged
Push-Location $root
try {
    Copy-Item -LiteralPath release/xl.dll -Destination "$work/xl.dll"
    Response 'compiler-build' ($flags+@('-O0','test/cases/xlir_c_owned_export_contract.c')+$objects+$links+@('-o',"$work/compiler.exe"))
    Check 'compiler-recipes' "$work/compiler.exe" @('test/cases/xlir_c_owned_exports.xl',"$work/exports")
    if ([IO.File]::ReadAllText("$work/exports.h") -match 'XL6_COwned|xl6_c_owned') { throw 'Private owner leaked into the public C header' }
    $ir=Get-Content -LiteralPath "$work/exports.json" -Raw | ConvertFrom-Json
    $generated=[IO.File]::ReadAllText("$work/exports.c")
    foreach ($export in $ir.exports) {
        $pattern='(?ms)^static [^;{]+ xlir_fn_'+$export.symbol+'\([^;{]*\)\n\{.*?^\}'
        $body=[regex]::Match($generated,$pattern)
        if (!$body.Success -or $body.Value -match '\bxl6C(?:String|Bytes)(?:Copy|Box)V1\s*\(') {
            throw "Private function acquired C payload materialization: $($export.cName)"
        }
    }
    [IO.File]::WriteAllText("$work/client.c",'#include "exports.h"'+"`n"+'xlir_string client(void) { return owned_text(); }'+"`n"+'XLTestOwnedProduct pair_client(void) { return owned_pair(); }'+"`n",$utf8)
    Check 'public-header' $gcc ($flags+@('-fsyntax-only',"$work/client.c"))
    $hostSource=(Resolve-Path test/cases/xlir_c_owned_export_host.c).Path.Replace('\','/')
    [IO.File]::WriteAllText("$work/host.c",('#define XL6_C_OWNED_EXPORT_PROGRAM "'+"$work/exports.c".Replace('\','/')+'"'+"`n"+'#include "'+$hostSource+'"'+"`n"),$utf8)
    foreach ($optimization in @('O0','O2')) {
        Response "host-$optimization-build" ($flags+@("-$optimization","$work/host.c")+$objects+$links+@('-o',"$work/host-$optimization.exe"))
        Check "host-$optimization-run" "$work/host-$optimization.exe" @()
    }
    $env:XLANG_PACKAGE_PATH="$root/test/fixtures;$root/release/extlibs"
    foreach ($name in @('xlir_c_owned_exports','xlir_c_owned_import_exports')) {
        $sidecars=if ($name -eq 'xlir_c_owned_exports') { @('test/cases/xlir_c_owned_native.c') } else { @() }
        Check "$name-jit" $public.Driver (@("test/cases/$name.xl",'run',"$work/$name-jit")+$sidecars)
        Check "$name-exe-build" $public.Driver (@("test/cases/$name.xl",'exe',"$work/$name")+$sidecars)
        Check "$name-exe-run" "$work/$name.exe" @()
    }
    Unchanged
    $outcome='passed'
} finally {
    $env:XLANG_PACKAGE_PATH=$oldPackagePath
    Pop-Location
    [ordered]@{outcome=$outcome;publicBuild="$publicPath/build.json";checks=@($checks.ToArray());inputs=$inputs} |
        ConvertTo-Json -Depth 7 | Set-Content -LiteralPath "$work/results.json" -Encoding utf8
}

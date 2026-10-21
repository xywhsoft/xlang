param(
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$work = [IO.Path]::GetFullPath($Directory)
if (Test-Path -LiteralPath $work) { throw 'Choose a fresh native-contract evidence directory' }
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
$checks = [Collections.Generic.List[object]]::new()
$files = @(Get-ChildItem -LiteralPath (Join-Path $root 'test/fixtures/native_source_contract') -File)
$files += @(Get-ChildItem -LiteralPath (Join-Path $root 'test/fixtures/sdk_feature_probe') -File)
$files += @(Get-Item -LiteralPath $PSCommandPath,$gcc,
    (Join-Path $root 'src/driver/xl6_xlc_shell.c'),
    (Join-Path $root 'test/cases/xlir_artifact_runtime_contract.c'),
    (Join-Path $root 'test/cases/extlib_sdk_features.xl'),
    (Join-Path $root 'test/cases/extlib_sdk_features_c.c'),
    (Join-Path $root 'test/cases/cgen_basic.xl'),
    (Join-Path $root 'test/cases/xlir_artifact_linkage_literal.xl'),
    (Join-Path $root 'tools/stage_xlir_artifact_runtime_sources.ps1'),
    (Join-Path $root 'test/suites/12_phase1_extlibs.bat'),
    (Join-Path $root 'tools/extlib_smoke_process.ps1'),
    (Join-Path $root 'tools/xlir_native_process.ps1'))
$inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
    @($files | Sort-Object FullName -Unique | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
function Unchanged {
    foreach ($row in $inputs) {
        if ((Get-FileHash -LiteralPath $row.Path).Hash -cne $row.Sha256) { throw "Changed input: $($row.Path)" }
    }
}
Unchanged
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$outcome = 'failed'
$oldEnv = @{}
foreach ($key in @('XL6_TCC_DLL','XL6_TCC_ROOT','XL6_TCC_LIB','XLANG_PACKAGE_PATH')) {
    $oldEnv[$key] = [Environment]::GetEnvironmentVariable($key)
}
function Check([string]$name, [string]$program, [string[]]$arguments, [string]$expected = '') {
    $r = Invoke-ExtlibSmokeProcess $program $arguments $root 120
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    if ($r.status -ne 'pass') { throw "$name failed: $($r.stderr) $($r.stdout)" }
    if ($expected -and $r.stdout.Replace("`r`n","`n").TrimEnd("`n") -cne $expected) {
        throw "$name returned unexpected output: $($r.stdout)"
    }
    Write-Host "[native-contract] ${name}: PASS"
    return $r
}
function Reject([string]$name, [string[]]$arguments, [string]$diagnostic) {
    $r = Invoke-ExtlibSmokeProcess $cli $arguments $root 120
    $checks.Add([ordered]@{name=$name;program=$cli;arguments=$arguments;expected='rejected';result=$r})
    if ($r.status -ne 'fail' -or $r.stderr -notmatch $diagnostic) {
        throw "$name must fail with its original diagnostic: $($r.stderr)"
    }
    Write-Host "[native-contract] ${name}: PASS (rejected)"
}
function BuildCase([string]$name, [string]$source, [string[]]$native, [bool]$full, [string]$expected) {
    $binary = Join-Path $work "$name.exe"
    $common = @($source) + $native + @('--inc-path',$includes)
    $null = Check "$name-jit" $cli (@('run') + $common) $expected
    $r = Check "$name-build" $cli (@('build') + $common + @('-o',$binary,'--save-temps'))
    $saved = @($r.stderr -split '\r?\n' | Where-Object { $_ -match '^temporary files kept: (.+)$' } | ForEach-Object { $_.Substring(22).Trim() })
    if ($saved.Count -ne 1) { throw "$name did not expose one explicitly requested generated source directory" }
    $implementation = Join-Path $saved[0] 'xl6_xrt_impl.c'
    $text = Get-Content -LiteralPath $implementation -Raw -Encoding UTF8
    if ($text.Contains('#define XRT_MODULE_ALL') -ne $full -or $text.Contains('#define XRUNTIME_MODULE_ALL') -ne $full) {
        throw "$name selected the wrong native runtime contract"
    }
    Copy-Item -LiteralPath $implementation -Destination (Join-Path $work "$name.native.c")
    $null = Check "$name-aot" $binary @() $expected
}
Push-Location $root
try {
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    $flags = @('-std=c11','-O0','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXL_ENABLE_EXE_COMPILER=1',
        '-I.','-Irelease','-Ilib/tcc')
    $libs = @('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $cli = Join-Path $work 'xl.exe'
    $contract = Join-Path $work 'contract.exe'
    $null = Check 'cli-build' $gcc ($flags + @('src/driver/xl6_xlc_shell.c') + $objects + $libs + @('-o',$cli))
    $null = Check 'contract-build' $gcc ($flags + @('test/cases/xlir_artifact_runtime_contract.c') + $objects + $libs + @('-o',$contract))
    Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $work 'xl.dll')
    $identity = [ordered]@{Driver=$cli;DriverSha256=(Get-FileHash -LiteralPath $cli).Hash}
    [IO.File]::WriteAllText((Join-Path $work 'build.json'),($identity | ConvertTo-Json),$utf8)
    $null = Check 'stage-runtime' $shell @('-NoProfile','-File','tools/stage_xlir_artifact_runtime_sources.ps1','-Directory',$work)
    $includes = [IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $env:XL6_TCC_DLL = Join-Path $root 'release/xl.dll'
    $env:XL6_TCC_ROOT = Join-Path $root 'res/tcc/win64/x86_64'
    $env:XL6_TCC_LIB = Join-Path $root 'res/tcc/win64/x86_64/lib'
    $env:XLANG_PACKAGE_PATH = Join-Path $root 'test/fixtures'
    $null = Check 'selection-contract' $contract @('test/cases/cgen_basic.xl','test/cases/xlir_artifact_linkage_literal.xl')
    $fixtures = 'test/fixtures/native_source_contract'
    $native = 'test/fixtures/sdk_feature_probe/sdk_features_native.c'
    BuildCase 'direct' "$fixtures/direct.xl" @($native) $true 'native source contract ok'
    BuildCase 'multiple' "$fixtures/multiple.xl" @($native,"$fixtures/extra.c") $true 'native source contract ok'
    BuildCase 'package-and-direct' "$fixtures/package_and_direct.xl" @("$fixtures/extra.c") $true 'native source contract ok'
    BuildCase 'project' "$fixtures/native_project.xlp" @() $true 'native source contract ok'
    BuildCase 'package-only' 'test/cases/extlib_sdk_features.xl' @() $false 'SDK selected features ok'
    BuildCase 'pure-xl' 'test/cases/xlir_artifact_linkage_literal.xl' @() $false ''
    BuildCase 'initializer-only' 'test/cases/cgen_basic.xl' @() $false ''

    # C 提供 main，仍由编译器注入当前生成头与 XRT 环境。
    $cmain = Join-Path $work 'c-main.exe'
    $null = Check 'c-main-build' $cli @('build',"$fixtures/exported.xl",$native,"$fixtures/c_main.c",'--inc-path',$includes,'-o',$cmain)
    $null = Check 'c-main-aot' $cmain @() 'native source contract ok'

    # DLL 消费者独立编译，只使用实际生成的公共头和 DLL。
    $library = Join-Path $work 'native-module.dll'
    $header = Join-Path $work 'module_api.h'
    $null = Check 'shared-build' $cli @('build',"$fixtures/exported.xl",$native,'--inc-path',$includes,'-shared','-o',$library,'--emit-header',$header)
    $null = Check 'shared-header-cpp' $gcc @('-x','c++','-std=c++11','-Wall','-Wextra','-Werror','-fsyntax-only','-DXL6_USE_SHARED',"-I$includes",$header)
    $null = Check 'shared-header-runtime-first-cpp' $gcc @('-x','c++','-std=c++11','-Wall','-Wextra','-Werror','-fsyntax-only','-DXL6_USE_SHARED',"-I$includes",'-include',(Join-Path $root 'src/api/xl_foreign_error.h'),$header)
    $client = Join-Path $work 'shared-client.exe'
    $null = Check 'shared-client-build' $gcc @('-std=c11','-Wall','-Wextra','-Werror','-DXL_TEST_DLL_CLIENT','-DXL6_USE_SHARED',"-I$work","-I$includes","$fixtures/c_main.c",$library,'-o',$client)
    $null = Check 'shared-client-run' $client @() 'native source contract ok'
    $runtimeFirst = Join-Path $work 'shared-runtime-first.exe'
    $null = Check 'shared-runtime-first-build' $gcc @('-std=c11','-Wall','-Wextra','-Werror','-DXL_TEST_DLL_CLIENT','-DXL6_USE_SHARED',"-I$work","-I$includes",'-include',(Join-Path $root 'src/api/xl_foreign_error.h'),"$fixtures/c_main.c",$library,'-o',$runtimeFirst)
    $null = Check 'shared-runtime-first-run' $runtimeFirst @() 'native source contract ok'

    foreach ($mode in @('run','exe')) {
        $null = Check "public-api-$mode" $public.Driver @("$fixtures/direct.xl",$mode,(Join-Path $work "public-$mode"),$native)
    }
    $null = Check 'public-api-aot' (Join-Path $work 'public-exe.exe') @() 'native source contract ok'
    Reject 'unknown-symbol-negative' @('build',"$fixtures/direct.xl","$fixtures/missing_symbol.c",'--inc-path',$includes,'-o',(Join-Path $work 'missing.exe')) 'deliberately_missing_native_symbol'
    Reject 'missing-main-negative' @('build',"$fixtures/exported.xl",$native,'--inc-path',$includes,'-o',(Join-Path $work 'missing-main.exe')) "unresolved reference to '(main|WinMain)'"
    Reject 'duplicate-main-negative' @('build',"$fixtures/direct.xl",$native,"$fixtures/duplicate_main.c",'--inc-path',$includes,'-o',(Join-Path $work 'duplicate-main.exe')) "'main' defined twice"
    Reject 'script-main-negative' @('build','test/cases/xlir_artifact_linkage_literal.xl',"$fixtures/duplicate_main.c",'--inc-path',$includes,'-o',(Join-Path $work 'script-main.exe')) "'main' defined twice"
    Unchanged
    $outcome = 'passed'
} finally {
    foreach ($key in $oldEnv.Keys) { [Environment]::SetEnvironmentVariable($key,$oldEnv[$key]) }
    $record = [ordered]@{date=[DateTimeOffset]::Now.ToString('o');outcome=$outcome;
        scope='Current-source isolated compiler/API and staged SDK; explicit C, project, package+direct, precise package trim, pure XL trim, C main, DLL consumer and unknown-symbol rejection. No release/VFS replacement.';
        inputs=$inputs;publicBuild=(Join-Path $PublicDirectory 'build.json');checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record | ConvertTo-Json -Depth 7),$utf8)
    Pop-Location
}

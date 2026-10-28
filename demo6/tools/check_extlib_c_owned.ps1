param(
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$publicPath = (Resolve-Path -LiteralPath $PublicDirectory).Path
$public = Get-Content -LiteralPath (Join-Path $publicPath 'build.json') -Raw | ConvertFrom-Json
if (!$public.SourceXrt) { throw 'A current-source public driver is required' }
$work = if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) } else { [IO.Path]::GetFullPath((Join-Path $root $Directory)) }
if (Test-Path -LiteralPath $work) { throw 'Use a fresh acceptance directory' }
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$packages = @('terraform_state','redis','bson','gettext','siphash128','blake2','blake3',
    'zstd','sharedmem','csv','xml.sax','zip','xhttp','xhttp.protocol','xhttp.body','xhttp.client',
    'xhttp.multipart','xhttp.form_data','xhttp.body_async','xhttp.body_codec','xhttp.body_decode',
    'xhttp.body_file','xhttp.body_stream')
$caseNames = @('xlir_extlib_c_owned_inputs','xlir_extlib_c_owned_text',
    'xlir_extlib_c_owned_http','xlir_extlib_owned_http_text','xlir_extlib_redis_current_tcp')
$files = @($PSCommandPath,$gcc,(Join-Path $root 'tools/extlib_smoke_process.ps1'))
$files += @(Get-ChildItem -LiteralPath (Join-Path $root 'test/cases') -File | Where-Object {
    $_.Name -like 'xlir_extlib_c_owned*' -or $_.Name -like 'xlir_extlib_owned_http*' -or $_.Name -eq 'xlir_extlib_redis_current_tcp.xl' -or
    $_.Name -in @('extlib_http_owned_native.c','extlib_http_feature_order.c')
} | ForEach-Object FullName)
foreach ($package in $packages) {
    $files += @(Get-ChildItem -LiteralPath (Join-Path $root "release/extlibs/$package") -Recurse -File |
        Where-Object { $_.Extension -in @('.c','.h','.xl','.json') } | ForEach-Object FullName)
}
$inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) + @(
    $files | Sort-Object -Unique | ForEach-Object {
        [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}
    })
function Unchanged {
    foreach ($input in $inputs) {
        if ((Get-FileHash -LiteralPath $input.Path).Hash -cne $input.Sha256) { throw "Changed input: $($input.Path)" }
    }
    if ((Get-FileHash -LiteralPath $public.Driver).Hash -cne $public.DriverSha256) { throw 'Changed public driver' }
}
function Check([string]$name, [string]$program, [string[]]$arguments) {
    $r = Invoke-ExtlibSmokeProcess $program $arguments $root 900
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    if ($r.status -ne 'pass') { throw "$name failed: $($r.stderr) $($r.stdout)" }
    Write-Host "[extlib-c-owned] ${name}: PASS"
}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Ilib/xruntime',
    '-Irelease/include','-Irelease/extlibs/terraform_state/include')
$links = @('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$runtimeObject = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o' })
if ($runtimeObject.Count -ne 1) { throw 'Missing source XRT implementation object' }
$oldPackagePath = $env:XLANG_PACKAGE_PATH
$outcome = 'failed'
Unchanged
[IO.Directory]::CreateDirectory($work) | Out-Null
Push-Location $root
try {
    $env:XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'
    foreach ($name in $caseNames) {
        $source = "test/cases/$name.xl"
        $sidecars = @('test/cases/xlir_extlib_c_owned_inputs_c.c')
        if ($name -eq 'xlir_extlib_c_owned_http') { $sidecars += 'test/cases/xlir_extlib_c_owned_http_c.c' }
        Check "$name-jit" $public.Driver (@($source,'run',"$work/$name-jit") + $sidecars)
        Check "$name-exe-build" $public.Driver (@($source,'exe',"$work/$name") + $sidecars)
        Check "$name-exe-run" "$work/$name.exe" @()
    }
    $module = Join-Path $work 'xlir_extlib_c_owned_inputs.temps/module.c'
    if ([IO.File]::ReadAllText($module) -match '(?m)^\s*v\d+ = xl6C(?:Bytes|String)(?:Copy|Box)V1\(') {
        throw 'Static extension reads acquired an implicit C owner materialization'
    }
    Write-Host '[extlib-c-owned] generated static inputs contain no implicit C payload materialization: PASS'
    Check 'csv-dictionary-and-boundaries' $public.Driver @('release/extlibs/csv/test/boundaries.xl','run',"$work/csv-boundaries")
    foreach ($reverse in @($false,$true)) {
        $defines = if ($reverse) { @('-DXL_TEST_HTTP_REVERSE') } else { @() }
        Check "http-feature-order-$reverse" $gcc ($flags + $defines + @('-fsyntax-only','test/cases/extlib_http_feature_order.c'))
    }
    $nativeCases = @(
        @('bson-smoke','bson','test/test_bson_smoke.c'),
        @('bson-boundaries','bson','test/test_boundaries.c'),
        @('redis','redis','test/test_redis.c'),
        @('redis-alloc','redis','test/test_redis_alloc.c'),
        @('gettext','gettext','test/test_gettext.c'),
        @('terraform','terraform_state','test/test_terraform_state.c'),
        @('blake2','blake2','test/test_blake2_native.c'),
        @('blake3','blake3','test/test_blake3_native.c'),
        @('http-owned','xhttp.protocol','@test/cases/extlib_http_owned_native.c')
    )
    foreach ($row in $nativeCases) {
        $package = $row[1]
        $manifest = Get-Content "release/extlibs/$package/xlang.package.json" -Raw | ConvertFrom-Json
        $sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/$package/$_" })
        $test = if ($row[2].StartsWith('@')) { $row[2].Substring(1) } else { "release/extlibs/$package/$($row[2])" }
        $binary = "$work/$($row[0]).exe"
        Check "$($row[0])-native-build" $gcc ($flags + @($test) + $sources + @($runtimeObject[0].Path) + $links + @('-o',$binary))
        $arguments = @()
        if ($row[0] -in @('bson-boundaries','gettext')) {
            $data = "$work/$($row[0])-data"
            [IO.Directory]::CreateDirectory($data) | Out-Null
            $arguments = @($data)
        }
        Check "$($row[0])-native-run" $binary $arguments
    }
    Check 'siphash-core-build' $gcc ($flags + @('release/extlibs/siphash128/test/test_siphash128_core.c',
        'release/extlibs/siphash128/third_party/siphash.c','-o',"$work/siphash-core.exe"))
    Check 'siphash-core-run' "$work/siphash-core.exe" @()
    Unchanged
    $outcome = 'passed'
} finally {
    $env:XLANG_PACKAGE_PATH = $oldPackagePath
    Pop-Location
    $record = [ordered]@{date=[DateTimeOffset]::Now.ToString('o');outcome=$outcome;
        scope='Five current-source JIT/EXE programs, no implicit C payload materialization, current TCP Redis, native legacy boundaries and allocation failures. HTTP Request component is tested independently; full legacy TCP/TLS facade is not covered.';
        publicBuild=(Join-Path $publicPath 'build.json');inputs=$inputs;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record | ConvertTo-Json -Depth 8),$utf8)
}

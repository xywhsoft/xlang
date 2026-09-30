param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')

$root = (Get-Location).Path
$build = Get-Content (Join-Path $Directory 'build.json') -Raw | ConvertFrom-Json
$staged = Get-Content (Join-Path $Directory 'artifact_sources.json') -Raw | ConvertFrom-Json
if ((Get-FileHash $build.Driver).Hash -ne $build.DriverSha256 -or
    @(@($build.Inputs) + @($build.Headers) + @($staged.Files) | Where-Object {
        (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256
    }).Count) { throw 'Stale compiler or artifact inputs' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose fresh HTTP async evidence directory' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$output = (Resolve-Path $OutputDirectory).Path
$packages = (Resolve-Path release/extlibs).Path
$httpInputs = @(Get-ChildItem release/extlibs -Directory -Filter 'xhttp*' |
    ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Recurse -File } |
    ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$cases = @(
    @{Name='body_async';Source='release/extlibs/xhttp.body_async/test/xhttp_body_async_test.xl';Expected='xhttp.body_async package ok'},
    @{Name='body_codec';Source='release/extlibs/xhttp.body_codec/test/xhttp_body_codec_test.xl';Expected='xhttp.body_codec package ok'},
    @{Name='body_stream';Source='release/extlibs/xhttp.body_stream/test/xhttp_body_stream_test.xl';Expected='xhttp.body_stream package ok'},
    @{Name='void_suspension';Source='test/cases/xlir_async_void_suspension.xl';Sidecar='test/cases/xlir_async_void_suspension_native.c';Expected='async void suspension: PASS'},
    @{Name='constant_loop';Source='test/cases/xlir_constant_loop_completion.xl';Expected='constant loop completion: PASS'}
)
$results = @()
foreach ($case in $cases) {
    $prefix = Join-Path $output $case.Name
    $sourceHash = (Get-FileHash -LiteralPath $case.Source).Hash
    $sidecarHash = if ($case.Sidecar) { (Get-FileHash -LiteralPath $case.Sidecar).Hash } else { $null }
    $extra = if ($case.Sidecar) { @($case.Sidecar) } else { @() }
    $environment = @{XLANG_PACKAGE_PATH=$packages}
    $jit = Invoke-ExtlibSmokeProcess $build.Driver (@($case.Source,'run',$prefix) + $extra) $root 45 $environment
    $built = Invoke-ExtlibSmokeProcess $build.Driver (@($case.Source,'exe',$prefix) + $extra) $root 45 $environment
    $executable = "$prefix.exe"
    $aot = if ($built.status -eq 'pass' -and (Test-Path -LiteralPath $executable)) {
        Invoke-ExtlibSmokeProcess $executable @() $root 45
    } else { $null }
    $imports = if ($aot) { @(& objdump -p $executable | Select-String 'DLL Name:' | ForEach-Object Line) } else { @() }
    $passed = $jit.status -eq 'pass' -and $jit.stdout.Contains($case.Expected) -and
        $built.status -eq 'pass' -and $aot -and $aot.status -eq 'pass' -and
        $aot.stdout.Contains($case.Expected) -and !($imports -match 'DLL Name:\s+xlc?\.dll')
    $results += [pscustomobject]@{Name=$case.Name;Source=$case.Source;SourceSha256=$sourceHash;
        Sidecar=$case.Sidecar;SidecarSha256=$sidecarHash;Jit=$jit;Build=$built;Aot=$aot;
        Imports=$imports;ArtifactSha256=if ($aot) { (Get-FileHash $executable).Hash } else { $null };
        Passed=[bool]$passed}
    Write-Output "$($case.Name) jit=$($jit.status) build=$($built.status) aot=$($aot.status) passed=$passed"
}
$unchanged = @($results | Where-Object {
    (Get-FileHash -LiteralPath $_.Source).Hash -ne $_.SourceSha256 -or
    ($_.Sidecar -and (Get-FileHash -LiteralPath $_.Sidecar).Hash -ne $_.SidecarSha256)
}).Count -eq 0 -and @(@($build.Inputs) + @($build.Headers) + @($staged.Files) + $httpInputs | Where-Object {
    (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256
}).Count -eq 0 -and (Get-FileHash $build.Driver).Hash -eq $build.DriverSha256 -and
    @(Get-ChildItem release/extlibs -Directory -Filter 'xhttp*' |
        ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Recurse -File }).Count -eq $httpInputs.Count
$report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');DriverSha256=$build.DriverSha256;
    RuntimeSha256=$build.RuntimeSha256;SourcesUnchanged=$unchanged;HttpInputs=$httpInputs;Results=$results;
    Scope='Current-source JIT and standalone Windows AOT for three HTTP async package cases and shared control-flow regressions; not the formal release or full protocol certification.'}
[IO.File]::WriteAllText((Join-Path $output 'results.json'), ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
if (!$unchanged -or @($results | Where-Object { !$_.Passed }).Count) { throw 'HTTP async source gate failed' }

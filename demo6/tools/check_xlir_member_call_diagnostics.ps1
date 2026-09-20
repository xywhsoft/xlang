param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve member call diagnostics evidence'}
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $files=Get-Item -LiteralPath $PSCommandPath,'test/cases/xlir_member_call_diagnostic_contract.c',
        'test/cases/xlir_member_call_resolution.xl',(Get-Command gcc).Source
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($r in $inputs){if((Get-FileHash -LiteralPath $r.Path).Hash-ne$r.Sha256){throw "Stale diagnostic input: $($r.Path)"}}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $objects=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
    $flags=@('-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-I.','-Irelease','-Ilib/tcc')
    $libs=@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    & gcc @flags test/cases/xlir_member_call_diagnostic_contract.c @objects @libs -o "$outputPath/contract.exe" *> "$outputPath/build.log"
    $build=$LASTEXITCODE;$run=$null;$cases=@();$summary=$null
    if($build-eq0){
        & "$outputPath/contract.exe" 1> "$outputPath/contract.log" 2> "$outputPath/contract.err.log"
        $run=$LASTEXITCODE
        $rows=@(Get-Content -LiteralPath "$outputPath/contract.log"|Where-Object {$_-match '^\{'}|ConvertFrom-Json)
        $cases=@($rows|Where-Object {$_.PSObject.Properties.Name-contains'Case'})
        $summary=@($rows|Where-Object {$_.PSObject.Properties.Name-contains'SemanticRejections'})|Select-Object -Last 1
    }
    $driver=$public.Driver
    if((Get-FileHash -LiteralPath $driver).Hash-ne$public.DriverSha256){throw 'Public driver drift'}
    & $driver test/cases/xlir_member_call_resolution.xl emit "$outputPath/positive" *> "$outputPath/positive-emit.log"
    $emit=$LASTEXITCODE;$strict=$null
    if($emit-eq0){
        $includes=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
        & gcc @flags "-I$includes" -c "$outputPath/positive.c" -o "$outputPath/positive.o" *> "$outputPath/positive-strict.log"
        $strict=$LASTEXITCODE
    }
    & $driver test/cases/xlir_member_call_resolution.xl run "$outputPath/positive" *> "$outputPath/positive-run.log"
    $positiveRun=$LASTEXITCODE
    $unchanged=!@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count
    $complete=$unchanged-and$build-eq0-and$run-eq0-and$summary.Complete-and$cases.Count-eq11-and
        !@($cases|Where-Object {!$_.Complete}).Count-and$emit-eq0-and$strict-eq0-and$positiveRun-eq0
    $report=[pscustomobject]@{Complete=$complete;Inputs=$inputs;InputsUnchanged=$unchanged;Build=$build;Run=$run;Results=$cases;Summary=$summary;
        Positive=[pscustomobject]@{Emit=$emit;StrictC=$strict;Run=$positiveRun};
        Scope='Eleven malformed class/native calls must stop in semantic analysis with source-located diagnostics before any IR nodes. Valid overload/default/named/variadic/inherited/generic/function-field/dynamic/async calls retain strict C and public runtime execution.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Write-Output "Member diagnostics $($summary.SemanticRejections)/11, positive emit=$emit C=$strict run=$positiveRun"
    if(!$complete){throw 'Member call diagnostic failures retained'}
} finally {Pop-Location}

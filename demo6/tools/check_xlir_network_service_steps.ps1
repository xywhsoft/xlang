param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$NetworkReport,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve typed-step evidence'}
    $publicPath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'build.json'))
    $networkPath=[IO.Path]::GetFullPath($NetworkReport)
    $public=Get-Content $publicPath -Raw|ConvertFrom-Json;$network=Get-Content $networkPath -Raw|ConvertFrom-Json
    if(!$public.SourceXrt-or!$network.Complete-or!$network.InputsUnchanged-or$network.Results.Count-ne6){throw 'Six current-source network programs required'}
    $hostPath=[IO.Path]::GetFullPath('test/cases/xlir_network_service_step_host.c');$compiler=(Get-Command gcc).Source
    $files=Get-Item -LiteralPath $PSCommandPath,$hostPath,$publicPath,$networkPath,$compiler
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($network.Inputs)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($r in $inputs){if((Get-FileHash -LiteralPath $r.Path).Hash-ne$r.Sha256){throw "Stale typed-step input $($r.Path)"}}
    New-Item -ItemType Directory -Path $out|Out-Null
    $objects=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
    $includes=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $flags=@('-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-I.','-Irelease','-Ilib/tcc',"-I$includes")
    $rows=@()
    foreach($case in $network.Results){
        $caseDir=Join-Path $out $case.Fixture
        New-Item -ItemType Directory -Path $caseDir,"$caseDir/build/test","$caseDir/release/examples/fixtures"|Out-Null
        Copy-Item -LiteralPath release/xl.dll -Destination "$caseDir/xl.dll"
        Copy-Item -LiteralPath 'release/examples/fixtures/tls_cert.pem','release/examples/fixtures/tls_key.pem' -Destination "$caseDir/release/examples/fixtures"
        $define=New-XlirCStringDefine 'XLIR_NETWORK_GENERATED_C' $case.Generated.Replace('\','/');$binary="$caseDir/steps.exe"
        & $compiler @flags $define $hostPath @objects release/xl.dll -lws2_32 -liphlpapi -lshell32 -lole32 -ladvapi32 -o $binary *> "$caseDir/build.log"
        $build=$LASTEXITCODE
        foreach($mode in @('idle','pinned-engine','resolver-worker')){
            $exit=$null;$observed=$null
            if(!$build){
$process=Start-Process -FilePath $binary -ArgumentList $mode -WindowStyle Hidden -PassThru -WorkingDirectory $caseDir -RedirectStandardOutput "$caseDir/$mode.out.log" -RedirectStandardError "$caseDir/$mode.err.log"
                $null = $process.Handle
                if(!$process.WaitForExit(45000)){$process.Kill();$process.WaitForExit();$exit=-999}else{$process.Refresh();$exit=$process.ExitCode};$process.Dispose()
                $line=@(Get-Content "$caseDir/$mode.out.log"|Where-Object {$_-like'{"Mode":*'})|Select-Object -Last 1
                if($line){$observed=$line|ConvertFrom-Json}
            }
            $rows+=[pscustomobject]@{Fixture=$case.Fixture;Mode=$mode;Build=$build;Exit=$exit;Observed=$observed;Complete=$build-eq0-and$exit-eq0-and$observed.Complete}
            Write-Output "$($case.Fixture) $mode build=$build run=$exit"
        }
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{Complete=$unchanged-and$rows.Count-eq18-and@($rows|Where-Object {!$_.Complete}).Count-eq0;Inputs=$inputs;InputsUnchanged=$unchanged;Results=$rows;
        Scope='18 typed lifecycle cases from 6 unmodified current-source generated programs, including phase-zero retained owners, retry, reinit exclusion and exact memory balance. Live network graphs deliberately refuse; not whole-goal acceptance.'}
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if(!$report.Complete){throw 'Typed service-step failures collected'}
} finally {Pop-Location}

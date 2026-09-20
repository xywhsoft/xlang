param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
$saved=@{}
try {
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve V4 host evidence'}
    $buildPath=Join-Path $PublicDirectory 'build.json'
    $build=Get-Content $buildPath -Raw|ConvertFrom-Json
    if(!$build.SourceXrt-or$build.RuntimeOnly){throw 'Complete current-source compiler/runtime required'}
    $compiler=(Get-Command gcc).Source
    $files=Get-Item -LiteralPath $PSCommandPath,$buildPath,$compiler,'test/cases/xlir_module_retirement_v4_host.c',
        'test/cases/xlir_module_retirement_v4_artifact.c','test/cases/xlir_module_retirement_v4_hooks.h'
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale V4 input $($row.Path)"}}
    New-Item -ItemType Directory -Path $out|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$out/xl.dll"
    $objects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-notin @('xlir_public_source_driver.o','xl_api.o')}|ForEach-Object Path)
    $flags=@('-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-I.','-Irelease','-Ilib/tcc')
    & $compiler @flags test/cases/xlir_module_retirement_v4_host.c @objects release/xl.dll -lws2_32 -liphlpapi -lshell32 -lole32 -ladvapi32 -o "$out/host.exe" *> "$out/build.log"
    $buildExit=$LASTEXITCODE;$run=$null
    $profile=@{
        XL6_TCC_DLL='release/xl.dll';XL6_TCC_ROOT='res/tcc/win64/x86_64';XL6_TCC_LIB='res/tcc/win64/x86_64/lib';
        XL6_TCC_INCLUDE='lib/tcc/include';XL6_TCC_WIN_INCLUDE='res/tcc/win64/x86_64/include';
        XL6_TCC_WINAPI_INCLUDE='res/tcc/win64/x86_64/include/winapi';XL6_TCC_LIBTCC1='res/tcc/win64/x86_64/lib/libtcc1.a'}
    foreach($key in $profile.Keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,[IO.Path]::GetFullPath($profile[$key]),'Process')}
    if(!$buildExit){
        $headers=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$process=Start-Process -FilePath "$out/host.exe" -ArgumentList ('"'+$headers+'"') -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$out/run.out.log" -RedirectStandardError "$out/run.err.log"
        $null = $process.Handle
        if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
        $process.Dispose()
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq 0
    $report=[pscustomobject]@{Complete=$buildExit-eq0-and$run-eq0-and$unchanged;Inputs=$inputs;InputsUnchanged=$unchanged;Build=$buildExit;Run=$run;
        Scope='Real TCC compiled ABI fixtures against current-source host, including a real module/code-lease cycle. Not generated language service graph acceptance.'}
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    Write-Output "V4 host build=$buildExit run=$run complete=$($report.Complete)"
    if(!$report.Complete){throw 'V4 host failure preserved'}
} finally {foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')};Pop-Location}

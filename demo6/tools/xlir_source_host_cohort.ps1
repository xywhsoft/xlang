$script:XlirSourceHostCohortImplementation=$PSCommandPath
function Get-XlirSourceHostCohort([string]$Root,[string]$SourceDriver,[string]$Directory) {
    $contractPath=Join-Path (Split-Path -Parent $SourceDriver) 'build.json'
    $contracts=Get-Content -LiteralPath $contractPath -Raw|ConvertFrom-Json
    if(!$contracts.Complete-or!$contracts.InputsUnchanged-or!$contracts.PublicDirectory){throw 'Require the complete contract/source cohort'}
    $public=$contracts.PublicDirectory;$manifest=Join-Path $public 'build.json'
    $build=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json
    if(!$build.Complete-or!$build.InputsUnchanged-or!$build.SourceXrt-or$build.RuntimeOnly-or$build.EnableExeCompiler-ne1){throw 'Require the full compiler source role'}
    $proof=Join-Path $Directory 'source-role.json';$verifier=Join-Path $PSScriptRoot 'xlir_public_build_inputs.py'
    $code=Invoke-XlirNativeToLog 'python' @($verifier,'--root',$Root,'--public',$public,'--out',$proof) (Join-Path $Directory 'source-role.log')
    if($code){throw 'Source-host cohort verification failed'}
    if((Get-FileHash -LiteralPath (Join-Path $Root 'release/xl.dll')).Hash-ne$build.RuntimeSha256){throw 'Source-host runtime mismatch'}
    # These three TUs are actually included by the API host; provide every
    # other real object, excluding the one source-driver main.
    $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    foreach($name in $excluded){if(@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq$name}).Count-ne1){throw "Missing or ambiguous source role: $name"}}
    $objects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-notin$excluded}|ForEach-Object Path)
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($contracts.Inputs)+@($contracts.Drivers)
    $inputs+=@($script:XlirSourceHostCohortImplementation,$contractPath,$manifest,$proof,$verifier,
        (Join-Path $Directory 'source-role.log')|ForEach-Object {[pscustomobject]@{Path=[IO.Path]::GetFullPath($_);Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    [pscustomobject]@{Objects=$objects;Inputs=$inputs;PublicDirectory=$public;IncludeDirectory=(Join-Path $public 'include')}
}

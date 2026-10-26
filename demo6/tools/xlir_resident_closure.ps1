$script:XlirResidentClosureImplementation=$PSCommandPath
function Get-XlirResidentClosure([string]$Root,[string]$Directory,[string[]]$Symbols=@(),[string[]]$Sources=@(),[string]$NativeProvider='',
    [string[]]$Fixtures=@(),[string]$PublicDirectory='') {
    $selectedRoot=(Resolve-Path -LiteralPath $Root).Path
    $output=[IO.Path]::GetFullPath($Directory)
    if((Test-Path -LiteralPath $output) -or (!$Symbols.Count -and !$Sources.Count -and !$Fixtures.Count)){throw 'Choose fresh resident closure evidence and explicit roots'}
    [IO.Directory]::CreateDirectory($output)|Out-Null
    $observer=Join-Path (Split-Path -Parent $PSScriptRoot) 'test/support/xlir_resident_closure.c'
    $registry=Join-Path $selectedRoot 'src/compile/xl6_compile_unit.c'
    $binary=Join-Path $output 'closure.exe'
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-DXL_STATIC_EMBED',
        '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
        "-I$selectedRoot","-I$selectedRoot/release","-I$selectedRoot/release/include")
    $code=Invoke-XlirGccToLog ($flags+@($observer,$registry,'-Wl,--gc-sections','-o',$binary)) (Join-Path $output 'build.log')
    if($code){throw 'Canonical resident closure observer did not compile'}
    $fixtureInputs=@();$fixturePlans=@();$fixturePaths=@()
    if($Fixtures.Count){
        if(!$PublicDirectory){throw 'Fixture plans require an explicit complete source cohort'}
        $publicPath=(Resolve-Path -LiteralPath $PublicDirectory).Path
        $publicReport=Join-Path $publicPath 'build.json'
        $public=Get-Content -LiteralPath $publicReport -Raw|ConvertFrom-Json
        $sourceProof=Join-Path $output 'fixture-source-proof.json'
        $verifier=Join-Path $PSScriptRoot 'xlir_public_build_inputs.py'
        $verification=Invoke-XlirNativeToLog 'python' @($verifier,'--root',$selectedRoot,'--public',$publicPath,'--out',$sourceProof) (Join-Path $output 'fixture-source-proof.log')
        if($verification){throw 'Fixture plan source cohort verification failed'}
        $proof=Get-Content -LiteralPath $sourceProof -Raw|ConvertFrom-Json
        if(!$proof.Complete -or $proof.RuntimeOnly -or $proof.EnableExeCompiler -ne 1){throw 'Fixture plan needs the full compiler source role'}
        $mains=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Source)-eq'xlir_public_source_driver.c'})
        if($mains.Count-ne1){throw 'Ambiguous fixture plan source entry point'}
        $planSource=Join-Path (Split-Path -Parent $PSScriptRoot) 'test/support/xlir_fixture_runtime_plan.c'
        $planBinary=Join-Path $output 'fixture-plan.exe'
        $planFlags=@('-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
            "-I$selectedRoot","-I$selectedRoot/release","-I$selectedRoot/release/include")
        $objects=@($public.Objects|Where-Object {$_.Path-ne$mains[0].Path}|ForEach-Object Path)
        $runtime=Join-Path $selectedRoot 'release/xl.dll'
        Copy-Item -LiteralPath $runtime -Destination (Join-Path $output 'xl.dll')
        $planBuild=Invoke-XlirGccToLog ($planFlags+@($planSource)+$objects+@($runtime,'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',$planBinary)) (Join-Path $output 'fixture-plan-build.log')
        if($planBuild){throw 'Verified fixture plan observer did not link'}
        $planLog=Join-Path $output 'fixture-plans.jsonl'
        $planExit=Invoke-XlirNativeToLog $planBinary $Fixtures $planLog
        if($planExit){throw 'Fixture IR plan refused; no partial resident closure accepted'}
        $fixturePlans=@(Get-Content -LiteralPath $planLog|ForEach-Object {$_|ConvertFrom-Json})
        if($fixturePlans.Count-ne$Fixtures.Count-or@($fixturePlans|Where-Object {!$_.Deterministic}).Count){throw 'Incomplete or unstable fixture IR plans'}
        $Sources=@($Sources)+@($fixturePlans|ForEach-Object {$_.Sources})
        $Sources=@($Sources|Sort-Object -Unique)
        $fixturePaths=@($publicReport,$sourceProof,$verifier,$planSource,$planBinary,$runtime,$planLog,
            (Join-Path $output 'fixture-plan-build.log'),(Join-Path $output 'fixture-source-proof.log'))+
            @($Fixtures|ForEach-Object {Join-Path $selectedRoot $_})
        $fixtureInputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)
    }
    $arguments=@();foreach($symbol in $Symbols){$arguments+=@('--symbol',$symbol)}
    foreach($source in $Sources){$arguments+=@('--source',$source)}
    & $binary @arguments > (Join-Path $output 'closure.json')
    if($LASTEXITCODE){throw 'Unregistered resident root or invalid closure'}
    $closure=Get-Content -LiteralPath (Join-Path $output 'closure.json') -Raw|ConvertFrom-Json
    if(!$closure.Sources.Count -or @($closure.Sources|Sort-Object -Unique).Count-ne$closure.Sources.Count){throw 'Empty or duplicate resident closure'}
    $provided=@();$providerPath=$null
    if($NativeProvider){
        # The full native source cohort intentionally embeds the language
        # error TLS in this physical TU. Keep the required logical component,
        # but provide it exactly once; this is not a missing-provider waiver.
        if($NativeProvider-ne'tools/xrt_single_header_impl.c'){throw 'Unreviewed composite native provider'}
        $providerPath=Join-Path $selectedRoot $NativeProvider
        if([IO.File]::ReadAllText($providerPath)-notmatch'(?m)^#include "src/api/xl_foreign_error.c"\r?$'){
            throw 'Composite native provider no longer embeds the declared error component'
        }
        $provided=@('src/api/xl_foreign_error.c')
    }
    $paths=@($observer,$registry,$binary,$script:XlirResidentClosureImplementation,(Get-Command gcc).Source,
        (Join-Path $selectedRoot 'src/compile/xl6_artifact_runtime.h'),
        (Join-Path $selectedRoot 'src/compile/xl6_artifact_runtime_symbols.inc'),
        (Join-Path $output 'build.log'),(Join-Path $output 'closure.json'))
    $paths+=@($fixturePaths)
    $paths+=@($closure.Sources|ForEach-Object {Join-Path $selectedRoot $_})
    if($providerPath){$paths+=@($providerPath)}
    $inputs=@($paths|Sort-Object -Unique|ForEach-Object {[pscustomobject]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    $inputs+=@($fixtureInputs)
    [pscustomobject]@{Sources=@($closure.Sources|Where-Object {$_-notin$provided});RequiredSources=@($closure.Sources);
        CompositeProvider=$NativeProvider;EmbeddedSources=$provided;NativeModules=@($closure.NativeModules);Symbols=$Symbols;
        DirectIncludes=$Sources;FixturePlans=$fixturePlans;Inputs=$inputs;Scope='Actual verified fixture IR plus explicit host roots and artifact registry transitive closure, not a generated-C spelling filter or alternate provider'}
}

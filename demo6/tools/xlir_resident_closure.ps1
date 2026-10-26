function Get-XlirResidentClosure([string]$Root,[string]$Directory,[string[]]$Symbols=@(),[string[]]$Sources=@(),[string]$NativeProvider='') {
    $selectedRoot=(Resolve-Path -LiteralPath $Root).Path
    $output=[IO.Path]::GetFullPath($Directory)
    if((Test-Path -LiteralPath $output) -or (!$Symbols.Count -and !$Sources.Count)){throw 'Choose fresh resident closure evidence and explicit roots'}
    [IO.Directory]::CreateDirectory($output)|Out-Null
    $observer=Join-Path (Split-Path -Parent $PSScriptRoot) 'test/support/xlir_resident_closure.c'
    $registry=Join-Path $selectedRoot 'src/compile/xl6_compile_unit.c'
    $binary=Join-Path $output 'closure.exe'
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-DXL_STATIC_EMBED',
        '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
        "-I$selectedRoot","-I$selectedRoot/release","-I$selectedRoot/release/include")
    $code=Invoke-XlirGccToLog ($flags+@($observer,$registry,'-Wl,--gc-sections','-o',$binary)) (Join-Path $output 'build.log')
    if($code){throw 'Canonical resident closure observer did not compile'}
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
    $paths=@($observer,$registry,$binary,$PSCommandPath,(Get-Command gcc).Source,
        (Join-Path $selectedRoot 'src/compile/xl6_artifact_runtime.h'),
        (Join-Path $selectedRoot 'src/compile/xl6_artifact_runtime_symbols.inc'),
        (Join-Path $output 'build.log'),(Join-Path $output 'closure.json'))
    $paths+=@($closure.Sources|ForEach-Object {Join-Path $selectedRoot $_})
    if($providerPath){$paths+=@($providerPath)}
    $inputs=@($paths|Sort-Object -Unique|ForEach-Object {[pscustomobject]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    [pscustomobject]@{Sources=@($closure.Sources|Where-Object {$_-notin$provided});RequiredSources=@($closure.Sources);
        CompositeProvider=$NativeProvider;EmbeddedSources=$provided;NativeModules=@($closure.NativeModules);Symbols=$Symbols;
        DirectIncludes=$Sources;Inputs=$inputs;Scope='Actual artifact registry transitive closure, not a filename filter or alternate provider'}
}

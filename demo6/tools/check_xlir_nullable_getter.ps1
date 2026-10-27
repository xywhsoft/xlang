param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
. "$root/tools/xlir_native_process.ps1"
. (Join-Path $PSScriptRoot 'xlir_resident_objects.ps1')
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Choose a fresh nullable getter evidence directory'}
    $build=Get-Content "$public/build.json" -Raw|ConvertFrom-Json
    if($build.RuntimeOnly){throw 'Current-source compiler required'}
    foreach($row in $build.Inputs){if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw 'Source build changed'}}
    & ./tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $public
    if(!$?){throw 'Stage failed'}
    $cases=@('xlir_nullable_getter','xlir_nullable_protocol_getter','xlir_nullable_generic_getter','xlir_nullable_inherited_getter','xlir_nullable_getter_alias','xlir_nullable_getter_cleanup','stdlib_process_live_wait')
    $regressions=@('protocol_property_dispatch','protocol_generic_property_dispatch','cgen_class_self_property_dispatch','xlir_internal_property_error_propagation','nullable_safe_navigation')
    $extra=@($PSCommandPath,'tools/check_xlir_nullable_getter.sh','test/cases/xlir_nullable_getter_contract.c',
        'test/cases/xlir_process_oom_host.c','test/cases/xlir_time_text_closure_host.c','test/cases/xlir_process_child.c',
        'test/cases/sem_nullable_getter_private.xl','release/xl.exe','release/xlc.dll','release/xl.dll',
        'tools/extlib_smoke_process.ps1','tools/stage_xlir_artifact_runtime_sources.ps1',"$public/build.json",(Get-Command gcc).Source)
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+$extra
    $paths+=@($cases+$regressions|ForEach-Object {"test/cases/$_.xl"})
    $inputs=[ordered]@{}
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
    New-Item -ItemType Directory -Path $work|Out-Null
    $rows=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
    $child="$work/child.exe"
    function Check($name,$program,[string[]]$arguments,[string]$reject='') {
        $result=if($program -eq 'gcc'){Invoke-XlirGccToLog $arguments "$work/$name.compiler.log" -PassThru -TimeoutSeconds 120}
            else{Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{XL6_PROCESS_CHILD=$child}}
        $rows.Add([ordered]@{Name=$name;Program=$program;Arguments=$arguments;Result=$result})
        [IO.File]::WriteAllText("$work/$name.log",$result.stdout+$result.stderr,$utf8)
        if($reject){if($result.status-ne'fail'-or$result.exitCode-ne1-or$result.stderr-notmatch$reject){throw "$name rejection mismatch"}}
        elseif($result.status-ne'pass'){throw "$name failed: $($result.stderr) $($result.stdout)"}
        return $result.stdout
    }
    $complete=$false
    try {
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
            '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
            '-Wno-pointer-sign','-Wno-format-truncation',"-I$public/include",'-I.','-Irelease','-Irelease/include')
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $resident=Get-XlirFixtureResidentObjects -Root $root -PublicDirectory $public -Directory "$work/resident-closure" -Fixtures @($cases|ForEach-Object {"test/cases/$_.xl"})
        $nativeObjects=@($resident.Objects)
        foreach($record in $resident.Inputs){$inputs[$record.Path]=$record.Sha256}
        $null=Check 'child-build' gcc @('-std=c11','-Wall','-Wextra','-Werror','-municode','test/cases/xlir_process_child.c','-o',$child)
        $inputs[$child]=(Get-FileHash $child).Hash
        $null=Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_nullable_getter_contract.c')+$compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($case in $cases) {
            $source="test/cases/$case.xl";$prefix="$work/$case"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            $null=Check "$case-repeat" $build.Driver @($source,'emit',"$prefix.repeat")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.repeat.c").Hash){throw 'Getter C nondeterministic'}
            $witness=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
            if($witness-notmatch 'nullable getter closed contract: [1-9][0-9]* sites, [1-9][0-9]* rejected mutations'){throw 'Closed gate witness missing'}
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.closed.c").Hash){throw 'Closed contract C mismatch'}
            $entry=@();if($case-notin@('xlir_nullable_getter_alias','stdlib_process_live_wait')){$entry=@('-DXL6_TIME_USER_ENTRY')}
            $null=Check "$case-strict-build" gcc ($flags+$entry+@('-include',"$prefix.c",'test/cases/xlir_time_text_closure_host.c')+$nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot" "$prefix.aot.exe" @()
            $null=Check "$case-formal-jit" 'release/xl.exe' @('run',$source)
            $null=Check "$case-formal-aot-build" 'release/xl.exe' @('build',$source,'-o',"$prefix.formal.exe")
            $null=Check "$case-formal-aot" "$prefix.formal.exe" @()
            if($case-eq'xlir_nullable_getter_alias') {
                $null=Check 'alias-oom-build' gcc ($flags+@('-include',"$prefix.c",'test/cases/xlir_process_oom_host.c')+$nativeObjects+$libs+@('-o',"$prefix.oom.exe"))
                $terminal=$false
                for($point=0;$point-lt512;++$point){if((Check "alias-oom-$point" "$prefix.oom.exe" @("$point"))-match 'hit=0 balanced'){$terminal=$true;break}}
                if(!$terminal){throw 'Getter alias OOM prefix incomplete'}
            }
        }
        foreach($case in $regressions){$null=Check "$case-regression" $build.Driver @("test/cases/$case.xl",'run',"$work/$case")}
        $null=Check 'private-rejection' $build.Driver @('test/cases/sem_nullable_getter_private.xl','emit',"$work/private") 'safe member was not found|private|accessible'
        if(Test-Path "$work/private.c"){throw 'Private getter produced C'}
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;Cases=$cases;
            Inputs=$inputs;ChangedInputs=$changed;Results=$rows;Scope='Generic nullable getter CFG gate; scalar/string/bytes/class/collection/var and already-nullable returns, exact NUL, one evaluation, null skips getter, exception and alias lifetime, generic/protocol/inherited dispatch. Closed IR mutations, strict C/source and formal JIT/AOT; complete logical OOM prefix for alias fixture. Not total 53-module acceptance.'}|ConvertTo-Json -Depth 10),$utf8)
        if($changed.Count){throw 'Frozen nullable getter inputs changed'}
    }
    Write-Output "Nullable getter Windows checks: $($rows.Count)"
} finally {Pop-Location}

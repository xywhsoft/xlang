param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve native adapter evidence'}
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $parsedCases=Get-Content -LiteralPath 'test/cases/xlir_native_adapter_cases.json' -Raw|ConvertFrom-Json
    $cases=@(foreach($case in $parsedCases){$case})
    if(!$cases.Count-or@($cases|Sort-Object -Unique).Count-ne$cases.Count){throw 'Adapter cases must be explicit and unique'}
    $files=Get-Item -LiteralPath (@($PSCommandPath,'test/cases/xlir_native_adapter_cases.json','src/driver/xl6_xlc_shell.c',
        'test/cases/xlir_artifact_runtime_contract.c','test/cases/xlir_dynamic_operand_transaction_host.c',
        'tools/check_xlir_point_sequences.py','tools/xlir_public_build_inputs.py',(Get-Command gcc).Source,(Get-Command nm).Source)+
        @($cases|ForEach-Object {"test/cases/$_.xl"}))
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($r in $inputs){if((Get-FileHash -LiteralPath $r.Path).Hash-ne$r.Sha256){throw "Stale native adapter input: $($r.Path)"}}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $objects=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
    $flags=@('-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc')
    $libs=@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $cliBuild=Invoke-XlirGccToLog ($flags+@('src/driver/xl6_xlc_shell.c')+$objects+$libs+@('-o',"$outputPath/xl.exe")) "$outputPath/cli-build.log"
    $contractBuild=Invoke-XlirGccToLog ($flags+@('test/cases/xlir_artifact_runtime_contract.c')+$objects+$libs+@('-o',"$outputPath/contract.exe")) "$outputPath/contract-build.log"
    $boundaryErrors=@()
    $dependencySource=Get-Content -LiteralPath 'src/cgen/xl6_ir_cgen_native_dependencies.c' -Raw
    if($dependencySource-match 'XL6_Ast|strstr\(|ScanGenerated|ScanNode'){$boundaryErrors+='Native selector uses a source/text inference path'}
    $fields=[regex]::Matches((Get-Content -LiteralPath 'src/cgen/xl6_ir_cgen_needs.inc' -Raw),'XL6_IR_CGEN_NEED\([^,]+,\s*(\w+),\s*(\w+)\)')
    if($fields.Count-lt243-or@($fields|ForEach-Object {$_.Groups[1].Value}|Sort-Object -Unique).Count-ne$fields.Count){$boundaryErrors+='Missing or duplicate C implementation need descriptors'}
    $groups=@([regex]::Matches((Get-Content -LiteralPath 'src/cgen/xl6_ir_cgen_native_groups.inc' -Raw),'XL6_IR_CGEN_NATIVE_GROUP\((\w+),')|ForEach-Object {$_.Groups[1].Value})
    foreach($field in $fields){if($field.Groups[2].Value-notin$groups){$boundaryErrors+="Undeclared native group $($field.Groups[2].Value)"}}
    $api=Get-Content -LiteralPath 'src/api/xl_api.c' -Raw
    if($api.Contains('xl6XrtTrimBuildImplementationSourceWithRuntimeModules(')-or
        $api.Contains('xl6XrtTrimBuildImplementationSourceWithGeneratedSource(')-or
        $api.Contains('xl6XrtTrimBuildImplementationSourceFromAst(')-or
        $api.Contains('XLC_BACKEND_LEGACY')-or
        !$api.Contains('xl6XrtTrimBuildImplementationSourceFromArtifact(&unit->importDeps,')){
        $boundaryErrors+='Canonical XLIR assembly is not isolated from AST or generated-C dependency inference'
    }
    $includes=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $residentObjects=@{}
    foreach($object in $public.Objects){$residentObjects[[IO.Path]::GetFileNameWithoutExtension($object.Path)]=$object.Path}
    $residentUndefined=@{};$results=@()
    if($cliBuild-eq0-and$contractBuild-eq0){foreach($case in $cases){
        $prefix="$outputPath/$case"
        & "$outputPath/xl.exe" cgen "test/cases/$case.xl" --inc-path $includes --emit "$prefix.c" 1> "$prefix.cgen.log" 2> "$prefix.cgen.err.log"
        $cgen=$LASTEXITCODE
        & "$outputPath/contract.exe" --emit-native "test/cases/$case.xl" "$prefix.native.c" 1> "$prefix.plan.log" 2> "$prefix.plan.err.log"
        $planExit=$LASTEXITCODE;$nativeBuild=$null;$generatedBuild=$null;$defined=@();$required=@();$missing=@();$sources=@()
        if($cgen-eq0-and$planExit-eq0){
            $plan=Get-Content -LiteralPath "$prefix.plan.log"|Where-Object {$_-match '^\{"Fixture"'}|ConvertFrom-Json
            $sources=@($plan.Sources)
            $nativeBuild=Invoke-XlirGccToLog ($flags+@("-I$includes",'-Ilib/xruntime','-c',"$prefix.native.c",'-o',"$prefix.native.o")) "$prefix.native-build.log"
            $generatedBuild=Invoke-XlirGccToLog ($flags+@("-I$includes",'-c',"$prefix.c",'-o',"$prefix.generated.o")) "$prefix.generated-build.log"
            if($nativeBuild-eq0-and$generatedBuild-eq0){
                $defined=@(& nm -g --defined-only "$prefix.native.o"|ForEach-Object {if($_-match '\b[TRD] (xrt\w+)$'){$Matches[1]}})
                $required=@(& nm -u "$prefix.generated.o" "$prefix.native.o"|ForEach-Object {if($_-match '\bU (xrt\w+)$'){$Matches[1]}})
                foreach($source in $sources){
                    $name=[IO.Path]::GetFileNameWithoutExtension($source)
                    if(!$residentUndefined.ContainsKey($name)){
                        if(!$residentObjects.ContainsKey($name)){
                            # Some public API providers are unity-included in the
                            # host. Compile their standalone artifact source.
                            $provider="$outputPath/$name.resident.o"
                            $providerExit=Invoke-XlirGccToLog ($flags+@('-c',$source,'-o',$provider)) "$outputPath/$name.resident.log"
                            if($providerExit-ne0){throw "Cannot compile actual resident provider $source"}
                            $residentObjects[$name]=$provider
                        }
                        $residentUndefined[$name]=@(& nm -u $residentObjects[$name]|ForEach-Object {if($_-match '\bU (xrt\w+)$'){$Matches[1]}})
                    }
                    $required+=@($residentUndefined[$name])
                }
                $required=@($required|Sort-Object -Unique);$missing=@($required|Where-Object {$_-notin$defined})
            }
        }
        $results+=[pscustomobject]@{Fixture=$case;Cgen=$cgen;Plan=$planExit;NativeBuild=$nativeBuild;GeneratedBuild=$generatedBuild;
            Sources=$sources;Defined=$defined;Required=$required;Missing=$missing;
            Complete=($cgen-eq0-and$planExit-eq0-and$nativeBuild-eq0-and$generatedBuild-eq0-and!$missing.Count)}
        Write-Output "Adapter $case cgen=$cgen plan=$planExit native=$nativeBuild C=$generatedBuild missing=$($missing.Count)"
    }}
    $boxingBuild=$null;$boxingRun=$null
    $boxingC="$outputPath/xlir_dynamic_operand_transaction.c"
    if(Test-Path -LiteralPath $boxingC){
        $generatedDefine=New-XlirCStringDefine 'XLIR_BOXING_GENERATED_C' $boxingC.Replace('\','/') -RawArgument
        $boxingBuild=Invoke-XlirGccToLog ($flags+@("-I$includes",$generatedDefine,'test/cases/xlir_dynamic_operand_transaction_host.c')+$objects+$libs+@('-o',"$outputPath/boxing-runtime.exe")) "$outputPath/boxing-runtime-build.log"
        if($boxingBuild-eq0){
            & "$outputPath/boxing-runtime.exe" 1> "$outputPath/boxing-runtime.log" 2> "$outputPath/boxing-runtime.err.log"
            $boxingRun=$LASTEXITCODE
        }
    }
    & python tools/check_xlir_point_sequences.py --root $rootPath --public $PublicDirectory --out "$outputPath/point-sequences" *> "$outputPath/point-sequences.log"
    $pointExit=$LASTEXITCODE
    $pointReportPath="$outputPath/point-sequences/results.json"
    $pointComplete=$false
    $pointReportSha=$null
    if(Test-Path -LiteralPath $pointReportPath){
        $pointComplete=$pointExit-eq0-and(Get-Content -LiteralPath $pointReportPath -Raw|ConvertFrom-Json).Complete
        $pointReportSha=(Get-FileHash -LiteralPath $pointReportPath).Hash
    }
    $unchanged=!@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count
    $complete=$unchanged-and$cliBuild-eq0-and$contractBuild-eq0-and!$boundaryErrors.Count-and
        $results.Count-eq$cases.Count-and!@($results|Where-Object {!$_.Complete}).Count-and$boxingBuild-eq0-and$boxingRun-eq0-and$pointComplete
    $report=[pscustomobject]@{Complete=$complete;Inputs=$inputs;InputsUnchanged=$unchanged;CliBuild=$cliBuild;ContractBuild=$contractBuild;
        DescriptorFields=$fields.Count;DescriptorGroups=$groups.Count;BoundaryErrors=$boundaryErrors;Results=$results;
        BoxingRuntime=[pscustomobject]@{Build=$boxingBuild;Run=$boxingRun;Complete=($boxingBuild-eq0-and$boxingRun-eq0)};
        PointSequences=[pscustomobject]@{Exit=$pointExit;Complete=$pointComplete;Report=$pointReportPath;ReportSha256=$pointReportSha};
        Scope='Generated C and selected native implementation strictly compiled; actual undefined native symbols of generated and resident objects checked against selected implementation. Raw pointer collections additionally check actual JIT/AOT, addresses, IR refusal and finite allocation-failure prefixes; broader lifetime and platform acceptance remain separate.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if(!$complete){throw 'Native adapter closure failures retained'}
} finally {Pop-Location}

param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$runtimePath = Join-Path $Root "release\include\xl_runtime.h"
$stdlibDataPath = Join-Path $Root "src\stdlib\xl6_stdlib_data.c"
$stdlibPath = Join-Path $Root "src\stdlib\xl6_stdlib.c"
$modulePath = Join-Path $Root "src\cgen\xl6_cgen_module.c"
$asyncOwnerPath = Join-Path $Root "src\cgen\xl6_cgen_async_func_emit.c"
$valueOwnerPath = Join-Path $Root "src\cgen\xl6_cgen_value_object.c"
$persistencePath = Join-Path $Root "src\cgen\xl6_cgen_class_value_protocol.c"
$trimPath = Join-Path $Root "src\compile\xl6_xrt_trim.c"
$matrixGeneratorPath = Join-Path $Root "tools\gen_xrt_xlang_matrix.py"
$matrixDocName = "XRT$([char]0x4E0E)xlang$([char]0x80FD)$([char]0x529B)$([char]0x77E9)$([char]0x9635).md"
$matrixDocPath = Join-Path $Root (Join-Path "docs" $matrixDocName)
$cgenPaths = @(
    (Join-Path $Root "src\cgen\xl6_cgen_value_object_containers.c"),
    (Join-Path $Root "src\cgen\xl6_cgen_record_field.c"),
	(Join-Path $Root "src\cgen\xl6_cgen_special_collection.c"),
	(Join-Path $Root "src\cgen\xl6_cgen_special_collection_runtime.c")
)
$runtime = [System.IO.File]::ReadAllText($runtimePath, [System.Text.Encoding]::UTF8)
$stdlibData = [System.IO.File]::ReadAllText($stdlibDataPath, [System.Text.Encoding]::UTF8)
$stdlib = [System.IO.File]::ReadAllText($stdlibPath, [System.Text.Encoding]::UTF8)
$module = [System.IO.File]::ReadAllText($modulePath, [System.Text.Encoding]::UTF8)
$ownerCgen = [System.IO.File]::ReadAllText($asyncOwnerPath, [System.Text.Encoding]::UTF8) + "`n" +
    [System.IO.File]::ReadAllText($valueOwnerPath, [System.Text.Encoding]::UTF8)
$persistence = [System.IO.File]::ReadAllText($persistencePath, [System.Text.Encoding]::UTF8)
$trim = [System.IO.File]::ReadAllText($trimPath, [System.Text.Encoding]::UTF8)
$matrixGenerator = [System.IO.File]::ReadAllText($matrixGeneratorPath, [System.Text.Encoding]::UTF8)
$matrixDoc = [System.IO.File]::ReadAllText($matrixDocPath, [System.Text.Encoding]::UTF8)
$cgen = ($cgenPaths | ForEach-Object {
    [System.IO.File]::ReadAllText($_, [System.Text.Encoding]::UTF8)
}) -join "`n"
$containerCgen = [System.IO.File]::ReadAllText($cgenPaths[0], [System.Text.Encoding]::UTF8)
$failures = New-Object System.Collections.Generic.List[string]

function Require-Text
{
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if ( !$Text.Contains($Pattern) ) {
        $script:failures.Add($Message) | Out-Null
    }
}



function Get-Section
{
    param(
        [string]$Text,
        [string]$Start,
        [string]$End
    )

    $startIndex = $Text.IndexOf($Start, [System.StringComparison]::Ordinal)
    if ( $startIndex -lt 0 ) { return "" }

    $endIndex = $Text.IndexOf($End, $startIndex, [System.StringComparison]::Ordinal)
    if ( $endIndex -lt 0 ) { return "" }

    return $Text.Substring($startIndex, ($endIndex - $startIndex))
}



$directCalls = @(
    "xrtTypedArrayCapacity(",
    "xrtTypedArrayData(",
    "xrtTypedArrayConstData(",
    "xrtTypedArrayReserve(",
    "xrtTypedArrayResize(",
    "xrtTypedArrayTrim(",
    "xrtTypedListTrim(",
    "xrtTypedSetCapacity(",
    "xrtTypedSetReserve(",
    "xrtTypedSetTrim(",
    "xrtTypedSetMerge(",
    "xrtTypedSetUnion(",
    "xrtTypedSetIntersection(",
    "xrtTypedSetDifference(",
    "xrtTypedSetSymmetricDifference(",
    "xrtTypedSetIsSubset(",
    "xrtTypedSetIsSuperset(",
    "xrtTypedSetIsDisjoint(",
    "xrtTypedSetEquals(",
    "xrtTypedDictCapacity(",
    "xrtTypedDictReserve(",
    "xrtTypedDictTrim(",
    "xrtTypedDictMerge(",
    "xrtTypedDictEquals("
)

foreach ( $call in $directCalls ) {
    Require-Text $runtime $call "missing direct xruntime bridge: $call"
}

foreach ( $call in @(
    "xrtDynamicFieldsGetRef(",
    "xrtDynamicFieldsSetRef(",
    "xrtDynamicFieldsClone(",
    "xrtDynamicFieldsToValue(",
    "xrtDynamicFieldsFromValue("
) ) {
    Require-Text $runtime $call "missing direct xruntime dynamic-field bridge: $call"
}

foreach ( $signature in @(
    "static inline int xl6_xrt_dynamic_fields_reserve(",
    "static inline int xl6_xrt_dynamic_fields_set(",
    "static inline int xl6_xrt_dynamic_fields_merge("
) ) {
    Require-Text $runtime $signature "dynamic-field bridge does not follow the xlang bool ABI: $signature"
}

foreach ( $call in @(
    "xrtObjectCreate(",
    "xrtValueRuntimeObjectTake(",
    "xrtValueGetRuntimeObject(",
    "xrtWeakSet(",
    "xrtWeakCopy(",
    "xrtWeakMove(",
    "xrtWeakExpired(",
    "xrtWeakLock("
) ) {
    Require-Text $runtime $call "class/weak kernel does not directly use xruntime: $call"
}
if ( $runtime.Contains("xl6_weak_control") ) {
    $failures.Add("class/weak kernel still contains the private weak control block") | Out-Null
}
Require-Text $ownerCgen "xl6ValueRecordOwnerRef(self)" "managed self does not use the owned runtime-object lookup"
if ( $ownerCgen.Contains("xl6ValueRecordOwner(self)") ) {
    $failures.Add("managed self still uses the removed borrowed owner lookup") | Out-Null
}

Require-Text $stdlibData '"class DynamicFields\n"' "DynamicFields language abstraction is missing"
Require-Text $stdlibData '"    func clone() as DynamicFields\n"' "DynamicFields clone abstraction is missing"
Require-Text $stdlibData '"    var __fields = __xl6DynamicFieldsDefault()\n"' "root object is not backed by DynamicFields"
if ( $stdlibData.Contains('"    var __fields = dict{}\n"') ) {
    $failures.Add("root object still uses the private dictionary field store") | Out-Null
}
Require-Text $stdlib "if ( needsCoreClass )" "core object classes are not conditionally injected"
Require-Text $module "XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD 1" "dynamic-field cgen trim marker is missing"
Require-Text $module "XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT 1" "class cgen runtime-object trim marker is missing"
Require-Text $module "XRUNTIME_MODULE_RUNTIME_VALUE_WEAK 1" "weak cgen runtime-value trim marker is missing"
Require-Text $module "XRT_MODULE_ATOMIC 1" "class xrttype cache atomic trim marker is missing"
Require-Text $module "ctx.nullableTypeCount > 0" "nullable generated helpers do not close their runtime dependencies"
Require-Text $persistence "xl6_xrt_dynamic_fields_to_value_handle" "object persistence bypasses the dynamic-field bridge"
Require-Text $persistence "xl6_xrt_dynamic_fields_assign_value_handle" "object restore bypasses the dynamic-field bridge"
Require-Text $trim "keepDynamicFields" "standalone trim config does not retain dynamic fields"
Require-Text $trim '"XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD"' "standalone trim output omits dynamic fields"
Require-Text $trim "keepRuntimeWeak" "standalone trim config does not retain xruntime weak values"
Require-Text $trim '"XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT"' "standalone trim output omits runtime objects"
Require-Text $trim '"XRUNTIME_MODULE_RUNTIME_VALUE_WEAK"' "standalone trim output omits runtime weak values"
Require-Text $matrixGenerator "XRUNTIME_CAPABILITY_CONTRACT" "xruntime semantic capability contract is missing"
Require-Text $matrixGenerator '"schema": 2' "xruntime capability report schema is stale"
Require-Text $matrixGenerator '"directIntegratedApiCount"' "direct integration and semantic abstraction are still conflated"
Require-Text $matrixDoc "extlibs/xruntime/include/xrt/runtime_field.h" "generated matrix omits the dynamic-field capability"
if ( $matrixDoc.Contains("xlang-private-backend-pending-unification") ) {
    $failures.Add("generated matrix still marks the class/weak backend as private") | Out-Null
}

$setSection = Get-Section `
    $runtime `
    "static inline bool xl6TypedSetMerge(" `
    "static inline xtdict xl6TypedDictCreate("
if ( $setSection -eq "" ) {
    $failures.Add("cannot locate typed set bridge section") | Out-Null
} elseif ( $setSection -match '(?m)\bfor\s*\(' -or $setSection -match '(?m)\bwhile\s*\(' ) {
    $failures.Add("typed set algebra must not reimplement xruntime traversal") | Out-Null
}

$dictMergeSection = Get-Section `
    $runtime `
    "static inline bool xl6TypedDictMerge(" `
    "static inline const xl6_type_desc* xl6TypedDictItemType("
if ( $dictMergeSection -eq "" ) {
    $failures.Add("cannot locate typed dictionary merge bridge") | Out-Null
} elseif ( $dictMergeSection -match '(?m)\bfor\s*\(' -or $dictMergeSection -match '(?m)\bwhile\s*\(' ) {
    $failures.Add("typed dictionary merge must not reimplement xruntime traversal") | Out-Null
}

Require-Text `
    $runtime `
    "xl6_xrt_container_reserve(xvalue* values, long long count)" `
    "dynamic reserve must preserve a signed count until validation"
Require-Text `
    $runtime `
    "long long index1," `
    "dynamic swap must preserve signed indices until validation"
Require-Text `
    $runtime `
    "if ( count < 0 ) { return 0; }" `
    "dynamic reserve must reject negative counts"
Require-Text `
    $runtime `
    "if ( index1 < 0 || index2 < 0 ) { return 0; }" `
    "dynamic swap must reject negative indices"
$dynamicReserveSection = Get-Section `
    $runtime `
    "static inline int xl6_xrt_container_reserve(" `
    "static inline int xl6_xrt_container_swap("
Require-Text `
    $dynamicReserveSection `
    "return xl6TypedArrayReserve(" `
    "boxed typed arrays must execute reserve instead of reporting a false success"
Require-Text `
    $dynamicReserveSection `
    "(xtarray)xl6ValueHandleData(values), count" `
    "boxed typed array reserve must preserve its checked signed count"
Require-Text `
    $cgen `
    "xl6_xrt_container_reserve(env->self, xl6_xrt_value_int(frame->Argv[0]))" `
    "dynamic bound reserve must forward its signed integer unchanged"
Require-Text `
    $cgen `
    "xl6_xrt_container_swap(env->self, xl6_xrt_value_int(frame->Argv[0]), xl6_xrt_value_int(frame->Argv[1]))" `
    "dynamic bound swap must forward signed indices unchanged"

$reserveCgenSection = Get-Section `
    $containerCgen `
    'if ( xl6CTextEquals(name, "reserve")' `
    'if ( xl6CTextEquals(name, "swap")'
$swapCgenSection = Get-Section `
    $containerCgen `
    'if ( xl6CTextEquals(name, "swap")' `
    'xl6CgenError(ctx, call, "invalid container method arguments")'
if ( $reserveCgenSection -eq "" -or $swapCgenSection -eq "" ) {
    $failures.Add("cannot locate reserve/swap lowering sections") | Out-Null
} elseif ( $reserveCgenSection.Contains("unsigned") -or
           $swapCgenSection.Contains("unsigned") ) {
    $failures.Add("container lowering still contains a pre-validation unsigned cast") | Out-Null
}

foreach ( $call in @(
    "xrtTypedStackClone(",
    "xrtTypedStackEquals(",
    "xrtTypedTreeMerge(",
    "xrtTypedTreeClone(",
    "xrtTypedTreeEquals(",
    "xrtTypedSPSCQueuePushBatch(",
    "xrtTypedSPSCQueuePopBatch(",
    "xrtTypedMPSCQueuePushBatch(",
    "xrtTypedMPSCQueuePopBatch(",
    "xrtTypedMPMCQueuePushBatch(",
    "xrtTypedMPMCQueuePopBatch("
) ) {
    Require-Text $cgen $call "missing direct typed queue batch bridge: $call"
}
Require-Text `
    $cgen `
    "xl6_special_queue_push_batch_raw(" `
    "typed queue batch push dispatcher is missing"
Require-Text `
    $cgen `
    "xl6_special_queue_pop_batch_raw(" `
    "typed queue batch pop dispatcher is missing"

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures | Select-Object -First 30 ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "xruntime language bridge ok: containers, dynamic fields, class/weak ownership, ABI, and trim closure preserved."
exit 0

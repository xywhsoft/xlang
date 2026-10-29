param([string]$Root = '', [string]$Xl = '', [string]$Out = '', [switch]$StaticOnly)

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path

$matrixDocName = 'XRT' + ([char]0x4E0E) + 'xlang' + ([char]0x80FD) +
    ([char]0x529B) + ([char]0x77E9) + ([char]0x9635) + '.md'
$paths = [ordered]@{
    Runtime = 'release/include/xl_runtime.h'
    StdlibData = 'src/stdlib/xl6_stdlib_data.c'
    Stdlib = 'src/stdlib/xl6_stdlib.c'
    NativeGroups = 'src/cgen/xl6_ir_cgen_native_groups.inc'
    Needs = 'src/cgen/xl6_ir_cgen_needs.inc'
    NeedsAnalysis = 'src/cgen/xl6_ir_cgen_needs_analysis.c'
    XValueEmit = 'src/cgen/xl6_ir_cgen_emit_xvalue_containers.c'
    DynamicFields = 'src/runtime/xl6_dynamic_fields.c'
    DynamicCollection = 'src/runtime/xl6_dynamic_collection.c'
    ArrayMethods = 'src/runtime/xl6_array_methods.c'
    Tree = 'src/runtime/xl6_tree.c'
    Project = 'src/runtime/xl6_persistence_project_open.c'
    Persistence = 'src/cgen/xl6_ir_cgen_dispatch_persistence.c'
    ResourceEmit = 'src/cgen/xl6_ir_cgen_emit_resource_support.c'
    CollectionContract = 'src/cgen/xl6_ir_cgen_collection_emit.c'
    CollectionInstruction = 'src/cgen/xl6_ir_cgen_runtime_collection_instruction.c'
    Trim = 'src/compile/xl6_xrt_trim.c'
    ArtifactSymbols = 'src/compile/xl6_artifact_native_symbols.inc'
    MatrixGenerator = 'tools/gen_xrt_xlang_matrix.py'
    MatrixDoc = 'docs/' + $matrixDocName
}
$text = @{}
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$Message) {
    $script:failures.Add($Message)
}

function Require-Text([string]$Text, [string]$Pattern, [string]$Message) {
    if (!$Text.Contains($Pattern)) { Add-Failure $Message }
}

function Get-Section([string]$Text, [string]$Start, [string]$End) {
    $startIndex = $Text.IndexOf($Start, [StringComparison]::Ordinal)
    if ($startIndex -lt 0) { return '' }
    $endIndex = $Text.IndexOf($End, $startIndex, [StringComparison]::Ordinal)
    if ($endIndex -lt 0) { return '' }
    return $Text.Substring($startIndex, $endIndex - $startIndex)
}

foreach ($entry in $paths.GetEnumerator()) {
    $path = Join-Path $Root $entry.Value
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-Failure "required xruntime language bridge file is missing: $($entry.Value)"
        continue
    }
    $text[$entry.Key] = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
}

if ($failures.Count -eq 0) {
    foreach ($call in @(
        'xrtTypedArrayCapacity(',
        'xrtTypedArrayData(',
        'xrtTypedArrayConstData(',
        'xrtTypedArrayReserve(',
        'xrtTypedArrayResize(',
        'xrtTypedArrayTrim(',
        'xrtTypedListTrim(',
        'xrtTypedSetCapacity(',
        'xrtTypedSetReserve(',
        'xrtTypedSetTrim(',
        'xrtTypedSetMerge(',
        'xrtTypedSetUnion(',
        'xrtTypedSetIntersection(',
        'xrtTypedSetDifference(',
        'xrtTypedSetSymmetricDifference(',
        'xrtTypedSetIsSubset(',
        'xrtTypedSetIsSuperset(',
        'xrtTypedSetIsDisjoint(',
        'xrtTypedSetEquals(',
        'xrtTypedDictCapacity(',
        'xrtTypedDictReserve(',
        'xrtTypedDictTrim(',
        'xrtTypedDictMerge(',
        'xrtTypedDictEquals('
    )) {
        Require-Text $text.Runtime $call "missing direct xruntime container bridge: $call"
    }

    foreach ($call in @(
        'xrtDynamicFieldsGetRef(',
        'xrtDynamicFieldsSetRef(',
        'xrtDynamicFieldsClone(',
        'xrtDynamicFieldsToValue(',
        'xrtDynamicFieldsFromValue('
    )) {
        Require-Text $text.Runtime $call "missing direct xruntime dynamic-field bridge: $call"
    }
    foreach ($signature in @(
        'static inline int xl6_xrt_dynamic_fields_reserve(',
        'static inline int xl6_xrt_dynamic_fields_set(',
        'static inline int xl6_xrt_dynamic_fields_merge('
    )) {
        Require-Text $text.Runtime $signature "dynamic-field bridge does not use the xlang bool ABI: $signature"
    }

    foreach ($call in @(
        'xrtObjectCreate(',
        'xrtValueRuntimeObjectTake(',
        'xrtValueGetRuntimeObject(',
        'xrtWeakSet(',
        'xrtWeakCopy(',
        'xrtWeakMove(',
        'xrtWeakExpired(',
        'xrtWeakLock('
    )) {
        Require-Text $text.Runtime $call "class/weak bridge does not directly use xruntime: $call"
    }
    if ($text.Runtime.Contains('xl6_weak_control')) {
        Add-Failure 'class/weak bridge still contains the retired private weak control block'
    }

    Require-Text $text.StdlibData '"class DynamicFields\n"' 'DynamicFields language abstraction is missing'
    Require-Text $text.StdlibData '"    func clone() as DynamicFields\n"' 'DynamicFields clone abstraction is missing'
    Require-Text $text.StdlibData '"    DynamicFields __fields = __xl6DynamicFieldsDefault()\n"' 'root object is not backed by nominal DynamicFields storage'
    if ($text.StdlibData.Contains('"    var __fields = dict{}\n"')) {
        Add-Failure 'root object still uses the retired private dictionary field store'
    }
    Require-Text $text.Stdlib 'if ( needsCoreClass )' 'core object classes are not conditionally injected'

    foreach ($contract in @(
        'XL6_IR_CGEN_NATIVE_GROUP(DYNAMIC_FIELDS, "XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD")',
        'XL6_IR_CGEN_NATIVE_GROUP(WEAK, "XRUNTIME_MODULE_RUNTIME_VALUE_WEAK")',
        'XL6_IR_CGEN_NATIVE_GROUP(SPSC_QUEUE, "XRUNTIME_MODULE_TYPED_QUEUE_SPSC")',
        'XL6_IR_CGEN_NATIVE_GROUP(MPSC_QUEUE, "XRUNTIME_MODULE_TYPED_QUEUE_MPSC")',
        'XL6_IR_CGEN_NATIVE_GROUP(MPMC_QUEUE, "XRUNTIME_MODULE_TYPED_QUEUE_MPMC")',
        'XL6_IR_CGEN_NATIVE_GROUP(TREE, "XRUNTIME_MODULE_TYPED_TREE")'
    )) {
        Require-Text $text.NativeGroups $contract "XLIR native dependency group is missing: $contract"
    }
    foreach ($contract in @(
        'XL6_IR_CGEN_NEED(int, needsXValueDynamicFieldRuntime, DYNAMIC_FIELDS)',
        'XL6_IR_CGEN_NEED(int, needsPersistenceDynamicFields, DYNAMIC_FIELDS)',
        'XL6_IR_CGEN_NEED(int, needsStackResourceRuntime, NONE)',
        'XL6_IR_CGEN_NEED(int, needsAvlTreeResourceRuntime, TREE)'
    )) {
        Require-Text $text.Needs $contract "XLIR runtime need is not mapped to an implementation group: $contract"
    }

    Require-Text $text.NeedsAnalysis 'needs->needsXValueDynamicFieldRuntime = 1;' 'lowered XLIR loses dynamic-field dependency'
    foreach ($call in @('xrtDynamicFieldsHas(', 'xrtDynamicFieldsSetRef(', 'xrtDynamicFieldsCreate(')) {
        Require-Text $text.DynamicFields $call "resident DynamicFields does not delegate to xruntime: $call"
    }
    foreach ($call in @('xl6ArrayMethodReserveV1(', 'xl6ArrayMethodSwapV1(')) {
        Require-Text $text.DynamicCollection $call "dynamic collection loses checked typed method: $call"
    }
    foreach ($call in @('xrtTypedTreeMerge(', 'xrtTypedTreeClone(', 'compatiblePolicies(')) {
        Require-Text $text.Tree $call "resident tree does not delegate to xruntime: $call"
    }
    # Signed-range, native family pruning, batches and persistence behavior are
    # exercised below against JIT and standalone AOT, rather than inferred from
    # retired inline emitter text. Resident calls may legitimately move files.

    $setSection = Get-Section $text.Runtime 'static inline bool xl6TypedSetMerge(' 'static inline xtdict xl6TypedDictCreate('
    if (!$setSection) {
        Add-Failure 'cannot locate typed set bridge section'
    } elseif ($setSection -match '(?m)\bfor\s*\(' -or $setSection -match '(?m)\bwhile\s*\(') {
        Add-Failure 'typed set algebra must delegate traversal to xruntime'
    }
    $dictSection = Get-Section $text.Runtime 'static inline bool xl6TypedDictMerge(' 'static inline const xl6_type_desc* xl6TypedDictItemType('
    if (!$dictSection) {
        Add-Failure 'cannot locate typed dictionary bridge section'
    } elseif ($dictSection -match '(?m)\bfor\s*\(' -or $dictSection -match '(?m)\bwhile\s*\(') {
        Add-Failure 'typed dictionary merge must delegate traversal to xruntime'
    }

    foreach ($contract in @(
        'xl6XrtTrimBuildImplementationSourceFromArtifact(',
        'deps->xrtModules.items',
        'deps->xrtModules.count',
        'xl6XrtTrimEmitModules(&out, modules, moduleCount)',
        '"XRT_MODULE_ALL"',
        '"XRUNTIME_MODULE_ALL"',
        '#define XRUNTIME_IMPLEMENTATION'
    )) {
        Require-Text $text.Trim $contract "artifact-driven XRT trim contract is missing: $contract"
    }
    foreach ($contract in @(
        'XL6_ARTIFACT_NATIVE_SYMBOL("xrtDynamicFieldsCreate", "XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD")',
        'XL6_ARTIFACT_NATIVE_SYMBOL("xrtValueRuntimeObject", "XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT")',
        'XL6_ARTIFACT_NATIVE_SYMBOL("xrtValueWeak", "XRUNTIME_MODULE_RUNTIME_VALUE_WEAK")'
    )) {
        Require-Text $text.ArtifactSymbols $contract "artifact native-symbol module mapping is missing: $contract"
    }
    Require-Text $text.MatrixGenerator 'XRUNTIME_CAPABILITY_CONTRACT' 'xruntime capability contract is missing'
    Require-Text $text.MatrixGenerator '"schema": 2' 'xruntime capability report schema is stale'
    Require-Text $text.MatrixGenerator '"directIntegratedApiCount"' 'direct integration and semantic abstraction are conflated'
    Require-Text $text.MatrixDoc 'extlibs/xruntime/include/xrt/runtime_field.h' 'capability matrix omits dynamic fields'
    if ($text.MatrixDoc.Contains('xlang-private-backend-pending-unification')) {
        Add-Failure 'capability matrix still marks class/weak support as a private backend'
    }

    $emitterText = $text.NativeGroups + $text.Needs + $text.NeedsAnalysis +
        $text.XValueEmit + $text.Persistence + $text.ResourceEmit +
        $text.CollectionContract + $text.CollectionInstruction
    if ($emitterText -match '\bXL6_(?:Ast|Hir)\w*|->inferredType\b|\bxl6Sem(?!CreateV1\b|CreateDefaultV1\b|WaitV1\b|TryWaitV1\b|WaitTimeoutV1\b|PostV1\b|PostMultipleV1\b)[A-Z]\w*\s*\(') {
        Add-Failure 'xruntime language bridge reads a retired frontend semantic model'
    }
}

if ($failures.Count -ne 0) {
    Write-Host 'xruntime language bridge check failed.'
    $failures | Select-Object -First 80 | ForEach-Object { Write-Host "  $_" }
    if ($failures.Count -gt 80) {
        Write-Host "  ... $($failures.Count - 80) more failure(s)"
    }
    exit 1
}

if (!$StaticOnly) {
    if (!$Xl) { $Xl = Join-Path $Root 'release/xl.exe' }
    if (!$Out) { throw 'Provide -Out outside the source tree for JIT/AOT bridge acceptance, or explicitly select -StaticOnly.' }
    & python -B (Join-Path $Root 'tools/check_xruntime_bridge_behavior.py') --root $Root --xl $Xl --out $Out
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Write-Host ('xruntime language bridge ok: resident delegation; ' + $(if ($StaticOnly) { 'static contracts only' } else { 'JIT/AOT behavior and dependency closure' }))
exit 0


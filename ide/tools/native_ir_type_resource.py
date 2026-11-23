"""Reject incomplete native IR after native construction allocation failures.

Only used after the caller verifies the complete original producer cohort.
The synchronous failure scope is independent of experimental cache profiles.
"""
import hashlib,re
from check_native_bridge_catalog import function

FUNCTIONS=(
 'xl6IrGrow',
 'xl6IrTypeFinalizeDisplayName',
 'xl6IrTypeDisplayNameMatches',
 'xl6IrTypeFinalizeRepresentation',
 'xl6IrTypeInitParameterAbis',
 'xl6IrTypePoolInternTuple',
 'xl6IrTypeInternArray',
 'xl6IrEnsureSymbolSlot',
 'xl6IrInternAstType',
 'xl6IrAppendExitCleanupActions',
 'xl6IrAppendExitStep',
 'xl6IrGeneratorClosePrefixMatches',
 'xl6IrAppendGeneratorClosePlan',
 'xl6IrRefreshExitPlans',
 'xl6IrBuildAstNode',
 'xl6IrInternDeclaredClassTypes',
 'xl6IrBuildSubstituteTypeDepth',
 'xl6IrBuildInternClassSubstitutedTypeDepth',
 'xl6IrBuildEnumLayouts',
 'xl6IrBuildUnionLayoutAppendField',
 'xl6IrBuildUnionLayouts',
 'xl6IrBuildClassLayoutAppendField',
 'xl6IrBuildClassStaticFieldAppend',
 'xl6IrBuildAppendClassDestroyStep',
 'xl6IrRefreshClassLayoutsDetailed',
 'xl6IrPersistenceInvocationOperands',
 'xl6IrPersistenceProjectWalkFields',
 'xl6IrPersistenceProjectWalkArena',
 'xl6IrPersistenceRestoreWalkFields',
 'xl6IrPersistenceRestoreWalkArena',
 'xl6IrBuildResultListType',
 'xl6IrBuildClassDispatchAppendSlot',
 'xl6IrBuildClassDispatchAppendImpl',
 'xl6IrBuildProtocolDispatchAppendSlot',
 'xl6IrBuildProtocolDispatchAppendImpl',
 'xl6IrBuildProtocolDispatchTables',
 'xl6IrBuildAppendNominalIdentity',
 'xl6IrBuildAppendNominalIdentitySet',
 'xl6IrNamedCallableAppliedType',
 'xl6IrInternNamedCallableSignature',
 'xl6IrBuildSyntaxCallSite',
 'xl6IrBuildFunctionValueCallSite',
 'xl6IrBuildCallSite',
 'xl6IrBuildIntrinsicPositionalOperands',
 'xl6IrBuildDynamicCall',
 'xl6IrBuildDynamicMemberWrite',
 'xl6IrBuildDynamicCalls',
 'xl6IrPrepareDynamicMethodResults',
 'xl6IrCanonicalIntegerValue',
 'xl6IrBuildDynamicMethodAppend',
 'xl6IrRebindDynamicMethodSpecialization',
 'xl6IrBuildIntrinsicCall',
 'xl6IrBuildValueInitializers',
 'xl6IrBuildNativeHandleBinds',
 'xl6IrBuildNativeHandleProjects',
 'xl6IrBindSpecializedSymbol',
 'xl6IrTypeInternCell',
 'xl6IrTypeInternValueIterator',
 'xl6IrTypeInternCFunctionPointer',
 'xl6IrTypeInternStorageRef',
 'xl6IrTypeInternPersistenceWire',
 'xl6IrVerifyLoweredLayout',
 'xl6IrBuildReachabilityFacts',
 'xl6IrBuildCallbackBoundaries',
 'xl6IrOwnedInputDominates',
 'xl6IrContinuationWriteAction',
 'xl6IrAsyncBindFrameSource',
 'xl6IrAsyncAppendMachineEntry',
 'xl6IrAsyncAppendMachineBlock',
 'xl6IrBuildAsyncFunctionMachine',
 'xl6IrGeneratorAppendFrameSlot',
 'xl6IrGeneratorAppendLiveSlot',
 'xl6IrGeneratorAppendMachineEntry',
 'xl6IrGeneratorAppendMachineBlock',
 'xl6IrBuildGeneratorFunctionMachine',
 'xl6IrVerifyGeneratorClosingPaths',
 'xl6IrVerifyGeneratorMachines',
 'xl6IrVerifyErrorUnwindRegions',
 'xl6IrVerifyStorageActivationFlowDetail',
 'xl6IrVerifyNodeTopology',
 'xl6IrVerifyModule',
)
RENAMED={
 "xl6IrBuildAstNode":"xlsIrBuildAstNodeOriginal",
 "xl6IrRefreshExitPlans":"xlsIrRefreshExitPlansOriginal",
 "xl6IrVerifyModule":"xlsIrVerifyModuleOriginal"}

REPLACE={'xrtMalloc':'xlsIrTypeMalloc','xrtCalloc':'xlsIrTypeCalloc',
 'xrtRealloc':'xlsIrTypeRealloc','xrtStrDup':'xlsIrTypeStrDup',
 'xrtStrDupN':'xlsIrTypeStrDupN','xl6IrGrow':'xlsIrTypeGrow'}

def derive(source):
    grow=function(source,'xl6IrGrow')
    assert source.count(grow)==1
    header='#include "src/ir/xl6_ir.h"'
    if source.count(header)!=1:raise ValueError('Native type-resource declaration seam changed')
    source=source.replace(header,header+'\n#include "analysis/frontend_ir_type_resource.inc"',1)
    rows=[]
    for name in FUNCTIONS:
        before=function(source,RENAMED.get(name,name));brace=before.index('{');body=before[brace:]
        calls=re.findall(r'\b('+'|'.join(REPLACE)+r')(?=\s*\()',body)
        if not calls:raise ValueError('Required type-allocation seam changed: '+name)
        after=before[:brace]+re.sub(r'\b('+'|'.join(REPLACE)+r')(?=\s*\()',lambda m:REPLACE[m[0]],body)
        assert source.count(before)==1
        source=source.replace(before,after,1)
        rows.append(dict(name=name,beforeSha256=hashlib.sha256(before.encode()).hexdigest(),
                         afterSha256=hashlib.sha256(after.encode()).hexdigest(),calls=len(calls)))
    if re.search(r'\b(?:xrtMalloc|xrtCalloc|xrtRealloc|xrtStrDup|xrtStrDupN)(?=\s*\()',source):raise ValueError('Unreviewed native allocation seam')
    return '#define XLS_NATIVE_IR_TYPE_RESOURCE_GUARD 1\n'+source,dict(
        enabled=True,functions=rows,scope='All native allocation sites in the audited TU;all profiles;conservative rejection after failure;optional overlay indexes/caches untouched')

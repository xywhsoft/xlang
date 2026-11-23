"""Expose the actual native raw prelude and complete ordered final pipeline.

The old builder is unchanged. This component creates no reuse eligibility,
and never constructs a complete new output to infer a production input.
"""
from pathlib import Path
import hashlib,re
from check_native_bridge_catalog import function
from native_ir_exit_profile import qualify

BODY_SHA='f0b8862bdaed2a6b49e0fa4959f5fb089747b2181ffc8bd28e13d712bc62df59'
PHASES=(
 'xl6IrBuildForeignClassBases','xl6IrInternAsyncInvocationTypes',
 'xl6IrBuildNamedCallableAbis','xl6IrInternProjectionDomains',
 'xl6IrTypePoolFinalizeCallableResults','xl6IrBuildOwnedReadResults',
 'xl6IrBuildUnionLayouts','xl6IrCloseOutputSizes','xl6IrCloseOutputRecords',
 'xl6IrCloseOutputViews','xl6IrCloseNativeSequences','xl6IrBuildEnumLayouts',
 'xl6IrRefreshClassLayoutsDetailed','xl6IrBuildClassStaticFields',
 'xl6IrCloseRegions','xl6IrCloseDataSnapshots','xl6IrClosePacks',
 'xl6IrCloseArrayGroups','xl6IrBuildImplicitNameAbis','xl6IrBuildClassDispatchTables',
 'xl6IrBuildProtocolDispatchTables','xl6IrRefreshNominalIdentitySets',
 'xl6IrNormalizeExpressionCallKinds','xl6IrBuildDynamicMethodRegistry',
 'xl6IrBuildIntrinsicCalls','xl6IrRefreshDynamicMethodAdmissions',
 'xl6IrBuildCallSites','xl6IrBuildValueInitializers','xl6IrBuildNativeHandleBinds',
 'xl6IrBuildNativeHandleProjects','xl6IrBuildOperatorErrorAbis','xl6IrBuildCastAbis',
 'xl6IrBuildMemberAbis','xl6IrBuildDynamicCalls','xl6IrBuildIndexWriteAbis',
 'xl6IrRefreshLoopAbis','xl6IrBuildImplicitConversionAbis','xl6IrRefreshExitPlans')

def derive(source:str,frontend:Path,output:Path):
    policy=qualify(frontend)
    include='\n#include "analysis/frontend_ir_pipeline.inc"\n'
    if not policy['enabled']:return source+include,dict(enabled=False,reason=policy['reason'])
    body=function(source,'xlsIrBuildModuleOriginal')
    if hashlib.sha256(body.encode()).hexdigest()!=BODY_SHA:
        raise ValueError('Native complete build pipeline changed;review all phase contracts')
    activate='    xlsIrDerivedActivate(module);'
    first=body.index(activate);failure=body.index('    if ( buildFailure != NULL )',first)
    stats=body.index('    module->stage = XL6_IR_STAGE_TYPED;',failure)
    prefix=body[:first].replace('xlsIrBuildModuleOriginal','xlsIrPipelinePrepareOriginal',1)
    for line in ('    const char* buildFailure = NULL;\n','    char classLayoutFailure[256];\n',
                 '    char classDispatchFailure[256];\n','    size_t i;\n',
                 "    classLayoutFailure[0] = '\\0';\n","    classDispatchFailure[0] = '\\0';\n"):
        if prefix.count(line)!=1:raise ValueError('Native prelude declaration changed')
        prefix=prefix.replace(line,'',1)
    prefix+='    return 1;\n}\n'
    chain=body[first+len(activate):failure]
    starts=list(re.finditer(r'(?m)^    (?:else )?if \( xlsFrontendCancelPoint\(XLS_CANCEL_IR_PHASE,1\) \|\|',chain))
    if len(starts)!=len(PHASES):raise ValueError('Native final-phase count changed')
    cases=[]
    for i,m in enumerate(starts):
        branch=chain[m.start():starts[i+1].start()if i+1<len(starts)else len(chain)]
        names=re.findall(r'\b(xl6Ir\w+)(?=\s*\()',branch)
        if names!=[PHASES[i]]:raise ValueError('Native phase order/read seam changed: '+str(names))
        branch=branch.replace('    else if (','    if (',1)
        cases.append('    case '+str(i)+': {\n'+branch+'    break;\n    }\n')
    phase='''static int xlsIrPipelinePhaseOriginal(XL6_IrModule* module,const XL6_AstNode* root,
 XL6_DiagList* diags,size_t phase)
{
    const char* buildFailure=NULL;
    char classLayoutFailure[256]={0},classDispatchFailure[256]={0};
    if(!phase)xlsIrDerivedActivate(module);
    switch(phase) {
'''+''.join(cases)+'''    default:return 0;
    }
    if(buildFailure!=NULL){xl6IrDiag(diags,NULL,buildFailure);xl6IrModuleFree(module);return 0;}
    return 1;
}
'''
    final='static int xlsIrPipelineCompleteOriginal(XL6_IrModule* module,XL6_DiagList* diags)\n{\n    size_t i;\n'+body[stats:]
    generated=prefix+phase+final+'\nstatic const char* const xlsIrPipelineNames[]={\n'+''.join(' "'+n+'",\n'for n in(*PHASES,'xl6IrVerifyModule'))+'};\n'
    target=output/'frontend/ir_native_pipeline_generated.inc';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(generated,encoding='utf8',newline='\n')
    return source+'\n#define XLS_NATIVE_IR_PIPELINE_SOURCE 1\n'+include,dict(
        enabled=True,originalBuilderSha256=BODY_SHA,generated=str(target),generatedSha256=hashlib.sha256(target.read_bytes()).hexdigest(),phases=list(PHASES),finalStep='Original statistics and complete native verification',dependencyContractSha256=policy['dependencyContractSha256'],scope='Fresh raw preparation and complete native pipeline;no cache eligibility or ordinary integration')

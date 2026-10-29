/* Offline registry/IR verification. No engine threads or ports are started. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals,targets,callbacks,services,policies,enums;
static const char* phase;
static void refused(XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(t){if(xl6IrRuntimeDirectBridgeTargetMatches(m,t))fprintf(stderr,"accepted mutation %zu: %s (%s)\n",refusals,t->cName,phase);
        assert(!xl6IrRuntimeDirectBridgeTargetMatches(m,t));}
    assert(!xl6IrVerifyModule(m,NULL));
    for(unsigned h=0;h<2;++h){XL6_Output o;XL6_DiagList d;size_t n=SIZE_MAX;
        xl6DiagInit(&d);assert(xl6OutputOpenMemory(&o));
        assert(!(h?xl6IrCgenEmitHeader(&o,m,"BAD_ENGINE",&d):xl6IrCgenEmitC(&o,m,&d)));
        char* s=xl6OutputTakeMemoryText(&o,&n);assert(n==0);xrtFree(s);xl6OutputDiscard(&o);xl6DiagFree(&d);}
    ++refusals;
}
static void contracts(const char* path,bool ownsSource)
{
    XL6_CompileUnit u;xl6CompileUnitInit(&u);
    assert(xl6CompileUnitLoadFile(&u,path,1)&&xl6IrLowerModule(&u.ir,&u.parse.diags));
    XL6_IrModule* m=&u.ir;assert(xl6IrVerifyModule(m,NULL));
    assert(m->abi.executionOwnershipAbi==(ownsSource?XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1:XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE));
    XL6_IrModuleAbi abi=m->abi;
    m->abi.executionOwnershipAbi=ownsSource?XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE:XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1;
    refused(m,NULL);m->abi=abi;
    assert(m->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_NONE&&m->abi.taskPoolAbi==XL6_IR_MODULE_TASK_POOL_NONE);
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];
        if(!t->cName||strncmp(t->cName,"xl6NetEngine",12))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(m,t));XL6_IrCallTarget saved=*t;++targets;
        phase="error";t->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;refused(m,t);*t=saved;
        phase="effect";t->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;refused(m,t);*t=saved;
        phase="boundary";t->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        refused(m,t);*t=saved;
        phase="service";t->nativeServicesRequired=saved.nativeServicesRequired?0:XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;refused(m,t);*t=saved;
        if(saved.nativeServicesRequired){++services;t->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;refused(m,t);*t=saved;}
        /* Handle policy identity is verified by the independent module policy
         * verifier, not by the direct-call signature matcher. */
        if(saved.nativeHandlePolicyCName){++policies;phase="policy";t->nativeHandlePolicyCName="xl6NetEngineOtherPolicy";refused(m,NULL);*t=saved;}
        if(strstr(t->cName,"PostV1")||strstr(t->cName,"AfterV1")){
            size_t p=strstr(t->cName,"PostV1")?1:2;
            XL6_IrCallParameterPhysicalAbi physical=t->parameterPhysicalAbis[p];
            t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m,t);t->parameterPhysicalAbis[p]=physical;
            XL6_IrType* c=(XL6_IrType*)xl6IrTypeGet(&m->types,t->parameterTypes[p]);XL6_IrType original=*c;
            c->paramCount=1;refused(m,t);*c=original;c->returnCount=1;refused(m,t);*c=original;
            c->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;refused(m,t);*c=original;++callbacks;
        }
        if(strstr(t->cName,"BackendV1")||strstr(t->cName,"StateV1")){
            XL6_IrTypeId id=strstr(t->cName,"WithBackendV1")?t->parameterTypes[1]:t->returnType;
            XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&m->types,id);XL6_IrType original=*type;
            type->nominalSymbol=0;refused(m,t);*type=original;
            type->kind=XL6_IR_TYPE_BUILTIN;refused(m,t);*type=original;++enums;
        }
    }
    assert(xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&u);
}
int main(int argc,char** argv)
{
    assert(argc==3);contracts(argv[1],true);contracts(argv[2],false);
    assert(targets>50&&callbacks>=2&&services>=2&&policies>=3&&enums>=4&&refusals>200);
    printf("Engine XLIR: %zu refusals, %zu direct targets; exact errors/effects/services/policies/callables/enums before C/header output\n",refusals,targets);
    return 0;
}

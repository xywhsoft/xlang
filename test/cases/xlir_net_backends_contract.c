/* Compiled metadata only, never port creation or network I/O. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/stdlib/xl6_stdlib_data.h"
#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals;
static void refused(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;
        xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_BACKEND",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(size==0);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
    ++refusals;
}
static void contracts(const char* path,bool text)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags)&&xl6IrVerifyModule(&unit.ir,NULL));
    XL6_ArtifactRuntimePlan plan;assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_BACKENDS));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_BACKEND_TEXT)==text);
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NETWORK_SERVICE));
    for(size_t n=0;n<plan.nativeModuleCount;++n)
        assert(!strstr(plan.nativeModules[n],"ENGINE")&&!strstr(plan.nativeModules[n],"DNS")&&
            !strstr(plan.nativeModules[n],"RESOLVER")&&!strstr(plan.nativeModules[n],"TLS"));
    XL6_IrTypeId plain=XL6_IR_TYPE_INVALID;
    for(size_t n=0;n<unit.ir.types.count;++n){const XL6_IrType* type=&unit.ir.types.items[n];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)plain=(XL6_IrTypeId)(n+1);}
    assert(plain);unsigned checked=0;XL6_IrCallTarget* enumTarget=NULL;
    XL6_IrTypeId enumId=XL6_IR_TYPE_INVALID;
    for(size_t n=0;n<unit.ir.callTargetCount;++n){
        XL6_IrCallTarget* t=&unit.ir.callTargets[n];
        if(!t->cName || (strncmp(t->cName,"xl6NetBackend",13)&&strcmp(t->cName,"xl6NetDefaultBackendV1")))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,t));XL6_IrCallTarget saved=*t;
        t->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;refused(&unit.ir,t);*t=saved;
        t->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;refused(&unit.ir,t);*t=saved;
        t->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;refused(&unit.ir,t);*t=saved;
        t->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;refused(&unit.ir,t);*t=saved;
        if(saved.parameterCount){XL6_IrTypeId original=t->parameterTypes[0];t->parameterTypes[0]=plain;
            refused(&unit.ir,t);t->parameterTypes[0]=original;enumTarget=t;enumId=original;
        }else {t->returnType=plain;refused(&unit.ir,t);*t=saved;enumTarget=t;enumId=saved.returnType;}
        ++checked;
    }
    assert(checked==(text?4u:2u)&&enumTarget&&enumId);
    XL6_IrType* type=&unit.ir.types.items[enumId-1];XL6_IrType savedType=*type;
    type->nominalSymbol=0;refused(&unit.ir,enumTarget);*type=savedType;
    type->typeValueId=0;refused(&unit.ir,enumTarget);*type=savedType;
    type->kind=XL6_IR_TYPE_BUILTIN;refused(&unit.ir,enumTarget);*type=savedType;
    type->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32;refused(&unit.ir,enumTarget);*type=savedType;
    XL6_IrEnumLayout* layout=(XL6_IrEnumLayout*)xl6IrEnumLayoutGet(&unit.ir,enumId);assert(layout);
    size_t count=layout->variantCount;--layout->variantCount;refused(&unit.ir,enumTarget);layout->variantCount=count;
    XL6_IrEnumVariant* member=&unit.ir.enumVariants[layout->firstVariant+3];
    char* name=member->name;member->name="other";refused(&unit.ir,enumTarget);member->name=name;
    XL6_IrNode* literal=&unit.ir.nodes[member->valueExpression-1];char* value=literal->text;
    literal->text="7";refused(&unit.ir,enumTarget);literal->text=value;
    XL6_IrNode* declaration=&unit.ir.nodes[layout->declaration-1];name=declaration->text;
    declaration->text="OtherBackend";refused(&unit.ir,enumTarget);declaration->text=name;
    assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    assert(xl6StdlibFuncCount==998);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void native_contracts(void)
{
    xrtSetErrorKind(XERR_ARGUMENT);const xerror* prior=xrtGetError();
    xmemdebugsnapshot queryBefore,queryAfter;xrtMemDebugSnapshot(&queryBefore);
    assert(xl6NetBackendCompiledV1(XNET_PORT_AUTO)&&xl6NetBackendCompiledV1(XNET_PORT_SELECT));
    assert(!xl6NetBackendCompiledV1(-1)&&!xl6NetBackendCompiledV1(INT64_MAX));
    assert(xl6NetBackendCapabilitiesV1(-1)==0&&xl6NetBackendCapabilitiesV1(INT64_MAX)==0);
    assert(xl6NetBackendCapabilitiesV1(XNET_PORT_SELECT)&XNET_PORT_CAP_READINESS);
    assert(xrtGetError()==prior);xrtMemDebugSnapshot(&queryAfter);
    assert(queryBefore.AllocCount==queryAfter.AllocCount&&queryBefore.FreeCount==queryAfter.FreeCount);
    xrtClearError();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xstrview text=xl6NetBackendNameV1(-1);assert(!text.Data&&xrtErrorKind(xrtGetError())==XERR_NOT_FOUND);balanced(&before);
    size_t prefixes=0;
    for(size_t point=0;point<16;++point){
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        text=xl6NetBackendNameV1(XNET_PORT_SELECT);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!text.Data&&xrtGetError());++prefixes;}
        else assert(text.Size==6&&!memcmp(text.Data,"select",6));
        xrtFree((void*)text.Data);balanced(&before);if(!hit)break;
    }
    assert(prefixes==1);
    puts("backend native: pure queries preserve error; one text OOM prefix, exact balanced storage");
}
int main(int argc,char** argv)
{
    assert(argc==3);contracts(argv[1],true);contracts(argv[2],false);native_contracts();
    printf("backend XLIR: %zu invalid contracts rejected before C/header output\n",refusals);
    return 0;
}

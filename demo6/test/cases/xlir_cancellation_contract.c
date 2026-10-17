#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void refuse(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header!=2;++header) {
        XL6_Output output; XL6_DiagList errors; size_t size=SIZE_MAX;
        xl6DiagInit(&errors); assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_CANCEL",&errors):
            xl6IrCgenEmitC(&output,module,&errors)));
        char* value=xl6OutputTakeMemoryText(&output,&size); assert(size==0);
        xrtFree(value); xl6OutputDiscard(&output); xl6DiagFree(&errors);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==4);
    assert(xl6ArtifactRuntimeSymbolOwner("xlCancellationRetirementSnapshotCheckedV1")==
        XL6_ARTIFACT_CANCELLATION_REGISTRATION);
    assert(xl6ArtifactRuntimeSymbolOwner("xlCancellationRetirementRetryCheckedV1")==
        XL6_ARTIFACT_CANCELLATION_REGISTRATION);
    assert(xl6ArtifactRuntimeSymbolOwner("xlCancellationRetirementRetryCheckedV1Almost")==
        XL6_ARTIFACT_RUNTIME_COUNT);
    static const struct { const char* name; XL6_ArtifactRuntimeId owner; } internal[]={
        {"xl6ThreadExitRegisterLocalV1",XL6_ARTIFACT_THREAD_EXIT},
        {"xl6ThreadExitCurrentV1",XL6_ARTIFACT_THREAD_EXIT},
        {"xl6ThreadLocalOwnershipAdapterV1",XL6_ARTIFACT_THREAD_LOCAL},
        {"xl6CancellationWatchPolicyV1Get",XL6_ARTIFACT_CANCELLATION_REGISTRATION},
        {"xl6CancellationRegistrationAdapterV1",XL6_ARTIFACT_CANCELLATION_REGISTRATION},
        {"xl6CancellationRegistrationQuiescenceV1Get",XL6_ARTIFACT_CANCELLATION_REGISTRATION}
    };
    for(size_t i=0;i<sizeof(internal)/sizeof(internal[0]);++i)
        assert(xl6ArtifactRuntimeSymbolOwner(internal[i].name)==internal[i].owner);
    size_t checked=0,closures=0,tags=0;
    for(int input=1;input!=argc;++input) {
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[input],1));
        assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* module=&unit.ir; assert(xl6IrVerifyModule(module,NULL));
        if(input==1) for(size_t i=0;i<module->callTargetCount;++i) {
            XL6_IrCallTarget* target=&module->callTargets[i];
            if(!target->cName || strncmp(target->cName,"xl6Cancellation",15))continue;
            XL6_IrCallTarget saved=*target;
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
            target->effectAbi=XL6_IR_CALL_EFFECT_PURE;refuse(module);*target=saved;
            target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?
                XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
            refuse(module);*target=saved;
            if(!strcmp(target->cName,"xl6CancellationRegistrationCreateV1")) {
                XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
                target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
                refuse(module);target->parameterPhysicalAbis[1]=physical;
                XL6_IrType* callback=&module->types.items[target->parameterTypes[1]-1];
                XL6_IrCallableErasureAbi erased=callback->callableErasureAbi;
                callback->callableErasureAbi=XL6_IR_CALLABLE_ERASURE_INVALID;
                refuse(module);callback->callableErasureAbi=erased;++closures;
            }
            ++checked;
        }
        if(input!=1) for(size_t i=0;i<module->instructionCount;++i) {
            const XL6_IrInstruction* instruction=&module->instructions[i];
            if(instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_I64)continue;
            assert(instruction->operandCount==1);
            XL6_IrValueId operand=module->operands[instruction->firstOperand];
            const XL6_IrInstruction* producer=NULL;
            for(size_t j=0;j<module->instructionCount;++j)
                if(module->instructions[j].result==operand){producer=&module->instructions[j];break;}
            assert(producer);
            const XL6_IrType* type=xl6IrTypeGet(&module->types,producer->type);
            assert(type && type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_I64 &&
                type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64);
            ++tags;
        }
        assert(xl6IrVerifyModule(module,NULL)); xl6CompileUnitFree(&unit);
    }
    assert(checked==16 && closures==1 && tags>=2);
    printf("cancellation: %zu exact targets, %zu checked closure, %zu canonical tags, %zu refusals\n",
        checked,closures,tags,refusals);
    return 0;
}

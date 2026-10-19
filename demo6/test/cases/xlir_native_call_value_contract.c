#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned plans,inputs,outputs,profiles,readonly,unions,awaits,nominal,mutations,emissions;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_NATIVE_CODEC_H",NULL):xl6IrCgenEmitC(&output,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(text&&size==0);
        xrtFree(text);xl6OutputDiscard(&output);++emissions;
    }
    ++mutations;
}
static void planRefused(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    XL6_IrNativeCallValuePlan plan,prior;memset(&plan,0xA5,sizeof(plan));memcpy(&prior,&plan,sizeof(plan));
    assert(!xl6IrNativeCallValuePlanBuild(module,target,&plan)&&!memcmp(&plan,&prior,sizeof(plan)));
    refused(module);
}
int main(int argc,char** argv)
{
    assert(argc>=3);
    for(int f=1;f<argc;++f) {
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        if(!xl6CompileUnitLoadFile(&unit,argv[f],1)||!xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
            for(size_t n=0;n<unit.parse.diags.count;++n)fprintf(stderr,"%s\n",unit.parse.diags.items[n].message);
            return 1;
        }
        XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned localPlans=0;
        bool readOnly=strstr(argv[f],"xlir_channel_codec_readonly")!=NULL;
        for(size_t i=0;i<module->callTargetCount;++i) {
            XL6_IrCallTarget* target=&module->callTargets[i];XL6_IrNativeCallValuePlan plan={0};
            if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
            assert(xl6IrNativeCallValuePlanBuild(module,target,&plan));
            if(!plan.value.schemaType||plan.value.codecAbi==XL6_IR_SEQUENCE_CODEC_DIRECT||(!plan.inputParameter&&!plan.output))continue;
            ++plans;++localPlans;
            assert(plan.value.slotAbi==XL6_IR_NATIVE_SLOT_XVALUE_CARRIER&&
                plan.value.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
            XL6_IrCallTarget saved=*target;
            if(plan.inputParameter) {
                ++inputs;size_t p=plan.inputParameter-1;XL6_IrTypeId type=target->parameterTypes[p];
                target->parameterTypes[p]=plan.value.schemaType;planRefused(module,target);target->parameterTypes[p]=type;
                XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
                target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);target->parameterPhysicalAbis[p]=abi;
            }
            if(plan.output) {
                ++outputs;target->returnType=plan.value.schemaType;planRefused(module,target);*target=saved;
                target->nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_NONE;refused(module);*target=saved;
                if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1) {
                    ++profiles;assert(target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_RETAINED_XVALUE);
                    target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE;refused(module);*target=saved;
                }
            }
            assert(xl6IrVerifyModule(module,NULL));
        }
        if(readOnly) {
            assert(localPlans>=4);
            for(size_t i=0;i<module->types.count;++i)
                assert(!xl6IrChannelConstructionRecipe(module,(XL6_IrTypeId)(i+1),NULL,NULL));
            readonly+=localPlans;
        }
        for(size_t i=0;i<module->callTargetCount;++i) {
            XL6_IrCallTarget* target=&module->callTargets[i];
            if(target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC||target->asyncResultAbi!=XL6_IR_ASYNC_RESULT_COPY_UNION)continue;
            ++unions;XL6_IrAsyncResultAbi saved=target->asyncResultAbi;
            assert(xl6IrUnionLayoutGet(module,target->workerReturnType));
            target->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_TAGGED;refused(module);target->asyncResultAbi=saved;
            for(size_t n=0;n<module->unionLayoutCount;++n) {
                XL6_IrTypeId other=module->unionLayouts[n].type;
                if(other==target->workerReturnType)continue;
                XL6_IrTypeId original=target->workerReturnType;target->workerReturnType=other;
                refused(module);target->workerReturnType=original;++nominal;break;
            }
        }
        for(size_t i=0;i<module->instructionCount;++i) {
            XL6_IrInstruction* instruction=&module->instructions[i];
            if(instruction->kind!=XL6_IR_INSTR_AWAIT||instruction->asyncResultAbi!=XL6_IR_ASYNC_RESULT_COPY_UNION)continue;
            ++awaits;instruction->asyncResultAbi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;refused(module);
            instruction->asyncResultAbi=XL6_IR_ASYNC_RESULT_COPY_UNION;
            for(size_t n=0;n<module->unionLayoutCount;++n) {
                XL6_IrTypeId other=module->unionLayouts[n].type;
                if(other==instruction->type)continue;
                XL6_IrTypeId original=instruction->type;instruction->type=other;
                refused(module);instruction->type=original;++nominal;break;
            }
        }
        assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    }
    assert(plans>10&&inputs>5&&outputs>5&&profiles>3&&readonly>=4&&unions>=1&&awaits>=1&&nominal>=2);
    assert(mutations==inputs*2+outputs*2+profiles+unions+awaits+nominal&&emissions==mutations*2);
    printf("native call value plans=%u inputs=%u outputs=%u stored-profiles=%u readonly=%u union-workers=%u union-awaits=%u mutations=%u zero-output=%u\n",
        plans,inputs,outputs,profiles,readonly,unions,awaits,mutations,emissions);
    return 0;
}

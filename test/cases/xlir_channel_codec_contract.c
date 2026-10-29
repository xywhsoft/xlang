#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned plans,refusals,emissions,genericRefusals;
static void moduleRefused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_DiagList errors;XL6_Output output;size_t size=123;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,ir,"CHANNEL_CODEC_H",&errors):xl6IrCgenEmitC(&output,ir,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(text&&size==0);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);++emissions;
    }
}
static void refused(XL6_IrModule* ir,XL6_IrTypeId owner)
{
    XL6_IrChannelValuePlan value,before;
    memset(&value,0xA5,sizeof(value));memcpy(&before,&value,sizeof(value));
    assert(!xl6IrChannelValuePlanBuild(ir,owner,&value));
    assert(!memcmp(&before,&value,sizeof(value)));
    moduleRefused(ir);
    ++refusals;
}
static void check(XL6_IrModule* ir)
{
    assert(xl6IrVerifyModule(ir,NULL));
    for(size_t i=0;i<ir->types.count;++i){
        XL6_IrTypeId owner=(XL6_IrTypeId)(i+1);XL6_IrChannelValuePlan plan={0};
        if(!xl6IrChannelValuePlanBuild(ir,owner,&plan)||plan.codecAbi==XL6_IR_SEQUENCE_CODEC_DIRECT)continue;
        XL6_IrType* type=&ir->types.items[i];
        XL6_IrType* schema=&ir->types.items[plan.schemaType-1];
        XL6_IrType* base=&ir->types.items[plan.baseElementType-1];
        assert(type->params[0]==plan.elementType&&plan.slotAbi==XL6_IR_NATIVE_SLOT_XVALUE_CARRIER&&
            plan.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
        assert(plan.nullable==(xl6IrTypeGet(&ir->types,plan.elementType)->kind==XL6_IR_TYPE_NULLABLE));
        if(plan.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1)assert(base->cAbi==XL6_IR_C_ABI_CLOSURE_PTR);
        else assert(plan.codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1&&base->cAbi==XL6_IR_C_ABI_UNION);
        ++plans;XL6_IrType saved=*schema,ownerSaved=*type,baseSaved=*base;
#define BAD_SCHEMA(Field,Value) do {schema->sequenceLayout.Field=(Value);refused(ir,owner);*schema=saved;}while(0)
        BAD_SCHEMA(codecAbi,XL6_IR_SEQUENCE_CODEC_DIRECT);
        BAD_SCHEMA(codecAbi,XL6_IR_SEQUENCE_CODEC_NONE);
        BAD_SCHEMA(slotAbi,XL6_IR_NATIVE_SLOT_FUTURE_PTR);
        BAD_SCHEMA(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF);
        BAD_SCHEMA(baseElementType,owner);
        BAD_SCHEMA(elementType,owner);
        BAD_SCHEMA(nullable,!saved.sequenceLayout.nullable);
#undef BAD_SCHEMA
        type->messageSchemaType=owner;refused(ir,owner);*type=ownerSaved;
        base->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;refused(ir,owner);*base=baseSaved;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    for(size_t i=0;i<ir->intrinsicCallCount;++i){XL6_IrIntrinsicCall* call=&ir->intrinsicCalls[i];
        if(call->admission!=XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT ||
            (call->runtimeResourceOp!=XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_CLOSE &&
             call->runtimeResourceOp!=XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_COUNT &&
             call->runtimeResourceOp!=XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_IS_CLOSED))continue;
        XL6_IrIntrinsicCall saved=*call;
#define BAD_CONTROL(Field,Value) do {call->Field=(Value);moduleRefused(ir);*call=saved;++genericRefusals;}while(0)
        BAD_CONTROL(admission,XL6_IR_INTRINSIC_ADMISSION_VERIFIED);
        BAD_CONTROL(admission,XL6_IR_INTRINSIC_ADMISSION_UNSUPPORTED_ABI);
        BAD_CONTROL(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
        BAD_CONTROL(errorAbi,XL6_IR_ERROR_ABI_NO_RAISE);
        BAD_CONTROL(resultAbi,saved.resultAbi==XL6_IR_INTRINSIC_RESULT_VOID?XL6_IR_INTRINSIC_RESULT_VALUE:XL6_IR_INTRINSIC_RESULT_VOID);
        BAD_CONTROL(operandCount,0);
#undef BAD_CONTROL
        assert(xl6IrVerifyModule(ir,NULL));
    }
}
int main(int argc,char** argv)
{
    assert(argc>=2);
    for(int i=1;i<argc;++i){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        if(!xl6CompileUnitLoadFile(&unit,argv[i],1)||!xl6IrLowerModule(&unit.ir,&unit.parse.diags)){
            for(size_t n=0;n<unit.parse.diags.count;++n)fprintf(stderr,"%s\n",unit.parse.diags.items[n].message);
            return 1;
        }
        check(&unit.ir);xl6CompileUnitFree(&unit);
    }
    assert(plans>=4&&refusals==plans*9&&genericRefusals>=6&&emissions==(refusals+genericRefusals)*2);
    printf("Channel value codec plans: %u closed logical/storage domains, %u schema refusals, %u generic-control refusals, %u zero-output C/header refusals\n",plans,refusals,genericRefusals,emissions);
    return 0;
}

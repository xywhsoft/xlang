#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static void reject(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_ARRAY_PRODUCER",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(void)
{
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit,"test/cases/stdlib_array_external_producers.xl",1) ||
        !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        for (size_t i=0;i<unit.parse.diags.count;++i) fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
        xl6CompileUnitFree(&unit); return 2;
    }
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL)); size_t count=0;
    size_t dynamic=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* call=&ir->instructions[i];
        if (call->kind!=XL6_IR_INSTR_RUNTIME_CALL || call->runtimeOp!=XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD) continue;
        assert(xl6IrCallInstructionErrorAbi(ir,call)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,call)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i+1<ir->instructionCount && ir->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction* test=&ir->instructions[i+1], saved=*test;
        test->kind=XL6_IR_INSTR_DISCARD; reject(ir); *test=saved;
        test->falseTarget=test->trueTarget; reject(ir); *test=saved; ++dynamic;
    }
    assert(dynamic==4);
    for (size_t i=0;i<ir->callTargetCount;++i) {
        XL6_IrCallTarget* target=&ir->callTargets[i];
        if (!target->nativeArrayResultAbi) continue;
        assert(target->nativeArrayResultAbi==XL6_IR_CALL_NATIVE_ARRAY_RESULT_PRIMITIVE_IDS_V1);
        assert(target->errorPhysicalAbi==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(xl6IrRuntimeDirectBridgeTargetMatches(ir,target));
        XL6_IrCallTarget saved=*target;
        target->nativeArrayResultAbi=XL6_IR_CALL_NATIVE_ARRAY_RESULT_NONE; reject(ir); *target=saved;
        target->nativeArrayResultAbi=(XL6_IrCallNativeArrayResultAbi)99; reject(ir); *target=saved;
        target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR; reject(ir); *target=saved;
        XL6_IrType* type=&ir->types.items[target->returnType-1]; XL6_IrType old=*type;
        type->sequenceLayout.nullable=true; reject(ir); *type=old;
        type->sequenceLayout.elementType=XL6_IR_TYPE_INVALID; reject(ir); *type=old;
        type->sequenceLayout.abi=XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2; reject(ir); *type=old;
        ++count;
    }
    assert(count==5 && xl6IrVerifyModule(ir,NULL));
    for (size_t i=0;i<ir->callTargetCount;++i) if (!ir->callTargets[i].nativeArrayResultAbi) {
        ir->callTargets[i].nativeArrayResultAbi=XL6_IR_CALL_NATIVE_ARRAY_RESULT_PRIMITIVE_IDS_V1; reject(ir);
        ir->callTargets[i].nativeArrayResultAbi=XL6_IR_CALL_NATIVE_ARRAY_RESULT_NONE; break;
    }
    xl6CompileUnitFree(&unit); assert(xrtMemDebugReset());
    puts("native Array producer: five closed recipes and forged ABI/nullable/element/layout/error contracts rejected"); return 0;
}

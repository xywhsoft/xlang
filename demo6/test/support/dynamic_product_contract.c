#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static void reject(XL6_IrModule* module)
{
    XL6_DiagList diags={0}; XL6_Output out; size_t length;
    assert(!xl6IrVerifyModule(module,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,module,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,module,"BAD_PRODUCT",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text); xl6DiagFree(&diags);
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags)); assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t count=0,mutations=0;
    for(size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&unit.ir.instructions[i];
        if(instruction->dynamicCallResultAbi!=XL6_IR_DYNAMIC_CALL_RESULT_OWNED_PRODUCT)continue;
        ++count; XL6_IrInstruction saved=*instruction;
        XL6_IrDynamicCallSite* site=&unit.ir.dynamicCallSites[instruction->dynamicCallSite];
        XL6_IrDynamicCallSite savedSite=*site;
        assert(xl6IrDynamicCallExpectedResultAbi(&unit.ir,site)==XL6_IR_DYNAMIC_CALL_RESULT_OWNED_PRODUCT);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        if(instruction->operandCount>1)assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,1)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        instruction->dynamicCallResultAbi=XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE; reject(&unit.ir); ++mutations; *instruction=saved;
        site->resultAbi=XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE; reject(&unit.ir); ++mutations; *site=savedSite;
        instruction->operandCount=0; reject(&unit.ir); ++mutations; *instruction=saved;
        instruction->type=xl6IrDynamicCallValueType(&unit.ir,site); reject(&unit.ir); ++mutations; *instruction=saved;
        XL6_IrType* product=(XL6_IrType*)xl6IrTypeGet(&unit.ir.types,instruction->type);
        XL6_IrTypeId field=product->params[1]; product->params[1]=XL6_IR_TYPE_INVALID;
        reject(&unit.ir); ++mutations; product->params[1]=field;
        site->resultType=XL6_IR_TYPE_INVALID; reject(&unit.ir); ++mutations; *site=savedSite;
    }
    assert(count>=4 && mutations==count*6); assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    printf("dynamic products=%zu mutations=%zu rejected before output\n",count,mutations);
    xl6CompileUnitFree(&unit); return 0;
}

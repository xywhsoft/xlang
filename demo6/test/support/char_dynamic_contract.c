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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,module,"BAD_CHAR",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text); xl6DiagFree(&diags);
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)); assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags)); size_t casts=0,conversions=0,mutations=0;
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&unit.ir.instructions[i];
        bool conversion=instruction->runtimeOp==XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED &&
            instruction->integerTarget==XL6_IR_INTEGER_TARGET_UNICODE_SCALAR && instruction->integerSourceAbi==XL6_IR_INTEGER_SOURCE_XVALUE;
        bool cast=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_CAST_CHECKED && instruction->castAbi==XL6_IR_CAST_XVALUE_TO_CHAR_CHECKED;
        if (!conversion && !cast) continue;
        if(conversion)++conversions;else ++casts;
        assert(xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        XL6_IrInstruction saved=*instruction;
        instruction->operandCount=0; reject(&unit.ir); ++mutations; *instruction=saved;
        instruction->type=XL6_IR_TYPE_INVALID; reject(&unit.ir); ++mutations; *instruction=saved;
        if (conversion) { instruction->integerSourceAbi=XL6_IR_INTEGER_SOURCE_I64; }
        else { instruction->castAbi=XL6_IR_CAST_XVALUE_TO_I64_CHECKED; }
        reject(&unit.ir); ++mutations; *instruction=saved;
        if (cast) { instruction->payloadType=XL6_IR_TYPE_INVALID; reject(&unit.ir); ++mutations; *instruction=saved; }
    }
    assert(conversions>=8 && casts>=3 && mutations>=36);
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    printf("char dynamic conversions=%zu casts=%zu mutations=%zu rejected before output\n",conversions,casts,mutations);
    xl6CompileUnitFree(&unit); return 0;
}

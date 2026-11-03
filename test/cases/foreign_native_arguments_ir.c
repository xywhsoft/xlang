#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_NATIVE_ARGUMENTS",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned boxes=0,releases=0,pointers=0;
    for(size_t i=0;i<module->types.count;++i){
        XL6_IrType* type=&module->types.items[i];
        if(type->kind!=XL6_IR_TYPE_NULLABLE || type->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLER)continue;
        ++pointers;XL6_IrType saved=*type;
        type->nullAbi=XL6_IR_NULL_ABI_TAGGED_VALUE;refused(module);*type=saved;
        type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;refused(module);*type=saved;
    }
    for(size_t i=0;i<module->instructionCount;++i){
        XL6_IrInstruction* instruction=&module->instructions[i];XL6_IrInstruction saved=*instruction;
        if(instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && instruction->runtimeOp==XL6_IR_RUNTIME_NATIVE_VALUE_BOX && boxes<4){
            ++boxes;instruction->payloadType=0;refused(module);*instruction=saved;
        }
        if(instruction->kind!=XL6_IR_INSTR_RELEASE || !xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,instruction->type)) || releases>=4)continue;
        ++releases;
        /* Remove a real native-owner release using an otherwise valid neutral
         * instruction. Ownership validation must still reject the escape. */
        *instruction=(XL6_IrInstruction){.kind=XL6_IR_INSTR_ERROR_CLEAR,.origin=saved.origin};
        refused(module);*instruction=saved;
    }
    assert(boxes && releases && pointers && xl6IrVerifyModule(module,NULL));
    printf("native arguments: boxes=%u removed releases=%u nullable pointers=%u mutations=%u zero-output=%u\n",boxes,releases,pointers,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

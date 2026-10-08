/* Mutate actual source-derived Lowered XLIR, never a synthetic type pool. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    XL6_Output out; assert(xl6OutputOpenMemory(&out));
    assert(xl6OutputPuts("KEEP",&out)>=0);
    assert(!xl6IrCgenEmitC(&out,ir,NULL));
    assert(out.buffer.Size==4 && !memcmp(out.buffer.Data,"KEEP",4));
    xl6OutputDiscard(&out);
}
int main(int argc,char** argv)
{
    assert(argc==2);
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"tree_contract") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags);
    if (!ok) {
        for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    unsigned producers=0,mutations=0;
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            (instruction->runtimeOp!=XL6_IR_RUNTIME_AVL_TREE_KEYS &&
             instruction->runtimeOp!=XL6_IR_RUNTIME_AVL_TREE_VALUES)) continue;
        XL6_IrInstruction saved=*instruction;
#define BAD(FIELD,VALUE) do { instruction->FIELD=(VALUE); refused(&ir); *instruction=saved; ++mutations; } while(0)
        BAD(sequenceAdmissionIdentitySet,UINT32_MAX);
        BAD(payloadType,XL6_IR_TYPE_INVALID);
        BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(runtimeResourceKeyPayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(runtimeResourceKeyOrderAbi,XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID);
        BAD(operandCount,0);
#undef BAD
        XL6_IrType* array=&ir.types.items[instruction->type-1];
        XL6_IrType savedArray=*array;
        array->sequenceLayout.slotAbi=XL6_IR_NATIVE_SLOT_NONE;
        refused(&ir); *array=savedArray; ++mutations;
        array->sequenceLayout.transferAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
        refused(&ir); *array=savedArray; ++mutations;
        array->typeValueId^=UINT64_C(1);
        refused(&ir); *array=savedArray; ++mutations;
        assert(xl6IrVerifyModule(&ir,NULL)); ++producers;
    }
    assert(producers>=6 && mutations==producers*9);
    printf("Tree projection IR: %u actual producers, %u rejected mutations; output retained\n",producers,mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}

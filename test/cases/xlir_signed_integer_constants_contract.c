#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reject_payload(XL6_IrModule* ir)
{
    XL6_Output output;
    XL6_DiagList diags={0};
    char* text;
    assert(!xl6IrVerifyModule(ir,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&output) && !xl6IrCgenEmitC(&output,ir,&diags));
    text=xl6OutputTakeMemoryText(&output,NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&output) && !xl6IrCgenEmitHeader(&output,ir,"BAD_CONSTANT",&diags));
    text=xl6OutputTakeMemoryText(&output,NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags);
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_Output output;
    unsigned minimums=0, unary=0, mutations=0;
    int ok=0;
    assert(argc==3);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"signed_constants") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if (instruction->kind==XL6_IR_INSTR_UNARY && instruction->opcode==XL6_IR_OPCODE_NEG) ++unary;
        if (instruction->kind!=XL6_IR_INSTR_CONST || instruction->immediateText==NULL ||
            strcmp(instruction->immediateText,"(-9223372036854775807LL - 1LL)")) continue;
        const XL6_IrType* type=xl6IrTypeGet(&ir.types,instruction->type);
        assert(type && type->cAbi==XL6_IR_C_ABI_I64 && type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64);
        char* original=instruction->immediateText;
        const char* invalid[]={"9223372036854775808ULL","-9223372036854775808LL",
            "(-9223372036854775807LL - 2LL)","-1ULL","18446744073709551616ULL","42"};
        for (size_t j=0;j<sizeof(invalid)/sizeof(invalid[0]);++j) {
            instruction->immediateText=(char*)invalid[j];
            reject_payload(&ir); ++mutations;
        }
        instruction->immediateText=original;
        assert(xl6IrVerifyModule(&ir,NULL)); ++minimums;
    }
    assert(minimums>=8 && unary==1);
    if (!xl6OutputOpenFile(&output,argv[2])) goto done;
    ok=xl6IrCgenEmitC(&output,&ir,&parse.diags); xl6OutputClose(&output);
    printf("signed constants: %u safe minimum values, one computed negation, %u canonical payload mutations rejected before C/header output\n",minimums,mutations);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return !ok;
}

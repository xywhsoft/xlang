#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"REJECTED_COLLECTION_ABI",&diagnostics)
                        : xl6IrCgenEmitC(&output,ir,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
}
int main(int argc, char** argv)
{
    assert(argc==3); int ok=0; unsigned checked=0, unrelated=0;
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"collection_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND) {
            assert(!instruction->collectionAdmissionAbi && !instruction->collectionAdmissionIdentitySet &&
                !instruction->collectionAdmissionNullable);
            if (!unrelated) {
                instruction->collectionAdmissionAbi=XL6_IR_COLLECTION_ADMISSION_PRIMITIVE;
                rejected(&ir); instruction->collectionAdmissionAbi=XL6_IR_COLLECTION_ADMISSION_NONE;
                instruction->collectionAdmissionIdentitySet=1;
                rejected(&ir); instruction->collectionAdmissionIdentitySet=0;
                instruction->collectionAdmissionNullable=true;
                rejected(&ir); instruction->collectionAdmissionNullable=false;
                ++unrelated;
            }
            continue;
        }
        XL6_IrCollectionAdmissionAbi family=instruction->collectionAdmissionAbi;
        XL6_IrNominalIdentitySetId identitySet=instruction->collectionAdmissionIdentitySet;
        bool nullable=instruction->collectionAdmissionNullable;
        instruction->collectionAdmissionAbi=XL6_IR_COLLECTION_ADMISSION_NONE; rejected(&ir);
        instruction->collectionAdmissionAbi=(XL6_IrCollectionAdmissionAbi)0x7fffffff; rejected(&ir);
        instruction->collectionAdmissionAbi=family==XL6_IR_COLLECTION_ADMISSION_PRIMITIVE
            ? XL6_IR_COLLECTION_ADMISSION_NOMINAL : XL6_IR_COLLECTION_ADMISSION_PRIMITIVE;
        rejected(&ir); instruction->collectionAdmissionAbi=family;
        instruction->collectionAdmissionIdentitySet=identitySet ? 0 : 1; rejected(&ir);
        instruction->collectionAdmissionIdentitySet=UINT32_MAX; rejected(&ir);
        instruction->collectionAdmissionIdentitySet=identitySet;
        instruction->collectionAdmissionNullable=!nullable; rejected(&ir);
        instruction->collectionAdmissionNullable=nullable;
        assert(xl6IrVerifyModule(&ir,NULL)); ++checked;
    }
    assert(unrelated && xl6IrVerifyModule(&ir,NULL));
    if (!xl6OutputOpenFile(&output,argv[2])) goto done;
    ok=xl6IrCgenEmitC(&output,&ir,&parse.diags); xl6OutputClose(&output);
    printf("collection contract: %u checked rebinds; family/set/nullable and unrelated fields refused before C/header output\n",checked);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}

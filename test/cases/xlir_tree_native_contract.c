/* Mutations operate on actual parsed, typed and lowered source. A fabricated
 * synthetic type table cannot prove the default/class/nominal producer paths. */
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
        xl6SemCheckModule(parse.root,&parse.diags,"tree_native_contract") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags);
    if (!ok) {
        for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    unsigned factories=0,nominalFactories=0,operations=0,mutations=0,ownedQueries=0;
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        const XL6_IrTreeQuerySpec* query=xl6IrTreeQuerySpecRuntime(instruction->runtimeOp);
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            (!query && (instruction->runtimeOp<XL6_IR_RUNTIME_AVL_TREE_CREATE ||
            instruction->runtimeOp>XL6_IR_RUNTIME_AVL_TREE_CLONE))) continue;
        XL6_IrInstruction saved=*instruction;
#define BAD(FIELD,VALUE) do { instruction->FIELD=(VALUE); refused(&ir); *instruction=saved; ++mutations; } while(0)
        BAD(payloadType,XL6_IR_TYPE_INVALID);
        BAD(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(runtimeResourceKeyType,XL6_IR_TYPE_INVALID);
        BAD(runtimeResourceKeyPayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(runtimeResourceKeyOrderAbi,XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID);
        if(query) {
            XL6_IrValueId receiver=ir.operands[instruction->firstOperand];
            const XL6_IrInstruction* owner=NULL;
            for(size_t at=0;at<ir.instructionCount;++at)
                if(ir.instructions[at].result==receiver) { owner=&ir.instructions[at]; break; }
            assert(owner && xl6IrTreeQueryInstructionCompatible(&ir,instruction));
            BAD(type,owner->type);
            BAD(result,XL6_IR_VALUE_INVALID);
            BAD(operandCount,instruction->operandCount+1);
            BAD(firstOperand,UINT32_MAX);
            BAD(sequenceAdmissionIdentitySet,1);
            BAD(runtimeIdentityType,owner->type);
            BAD(immediateText,"not-a-query-operand");
            ++ownedQueries;
        }
        if (instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_CREATE) {
            XL6_IrType* tree=&ir.types.items[instruction->type-1];
            XL6_IrType original=*tree;
            tree->treeKeySchemaType=0; refused(&ir); *tree=original; ++mutations;
            tree->treeValueSchemaType=0; refused(&ir); *tree=original; ++mutations;
            BAD(sequenceAdmissionIdentitySet,UINT32_MAX);
            if (saved.sequenceAdmissionIdentitySet) {
                BAD(sequenceAdmissionIdentitySet,0); ++nominalFactories;
            }
            ++factories;
        }
#undef BAD
        assert(xl6IrVerifyModule(&ir,NULL)); ++operations;
    }
    assert(factories && operations>=factories && mutations>=operations*5+factories*3);
    printf("Native Tree IR: %u actual factories (%u nominal), %u operations (%u owned queries), %u rejected mutations; output retained\n",
        factories,nominalFactories,operations,ownedQueries,mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}

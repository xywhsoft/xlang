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
        assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"REJECTED_CREATE",&diagnostics) :
            xl6IrCgenEmitC(&output,ir,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
}
int main(int argc,char** argv)
{
    assert(argc==3); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=0; unsigned producers=0,mutations=0,mask=0,dynamic=0,dynamicMutations=0;
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"array_create_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL &&
            ((instruction->runtimeOp>=XL6_IR_RUNTIME_XVALUE_BINARY_ADD &&
              instruction->runtimeOp<=XL6_IR_RUNTIME_XVALUE_BINARY_SHR) ||
             (instruction->runtimeOp>=XL6_IR_RUNTIME_XVALUE_UNARY_POS &&
              instruction->runtimeOp<=XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT))) {
            assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
            assert(xl6IrCallInstructionErrorPhysicalAbi(&ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
            assert(i+1<ir.instructionCount && ir.instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
            XL6_IrInstruction outcome=ir.instructions[i+1];
            ir.instructions[i+1].kind=XL6_IR_INSTR_CONST; rejected(&ir);
            ir.instructions[i+1]=outcome;
            XL6_IrNode* origin=&ir.nodes[instruction->origin-1];
            XL6_IrErrorAbi saved=origin->operatorErrorAbi;
            origin->operatorErrorAbi=XL6_IR_ERROR_ABI_NO_RAISE; rejected(&ir);
            origin->operatorErrorAbi=saved;
            ++dynamic; dynamicMutations+=2;
        }
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL || instruction->runtimeOp!=XL6_IR_RUNTIME_ARRAY_CREATE) continue;
        assert(xl6IrArrayCreateRuntimeInstructionCompatible(&ir,instruction));
        assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i+1<ir.instructionCount && ir.instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        const XL6_IrType* type=xl6IrTypeGet(&ir.types,instruction->type); assert(type);
        mask|=1u<<(unsigned)type->sequenceLayout.abi; ++producers;
        XL6_IrInstruction saved=*instruction;
#define MUTATE(Field,Value) do { instruction->Field=(Value); rejected(&ir); *instruction=saved; ++mutations; } while(0)
        MUTATE(type,XL6_IR_TYPE_INVALID);
        MUTATE(result,XL6_IR_VALUE_INVALID);
        MUTATE(payloadType,XL6_IR_TYPE_INVALID);
        MUTATE(payloadType,saved.type);
        MUTATE(collectionAbi,XL6_IR_COLLECTION_NONE);
        MUTATE(operandCount,1);
        MUTATE(runtimeIdentityType,saved.type);
        MUTATE(opcode,XL6_IR_OPCODE_ADD);
        MUTATE(sequenceAdmissionIdentitySet,UINT32_MAX);
        MUTATE(collectionAdmissionNullable,true);
        MUTATE(collectionAdmissionIdentitySet,UINT32_MAX);
        MUTATE(immediateText,"wire-array");
#undef MUTATE
        XL6_IrInstruction outcome=ir.instructions[i+1];
        ir.instructions[i+1].kind=XL6_IR_INSTR_CONST; rejected(&ir);
        ir.instructions[i+1]=outcome; ++mutations;
    }
    assert(producers && mutations==producers*13 && xl6IrVerifyModule(&ir,NULL));
    assert(xl6OutputOpenFile(&output,argv[2]));
    ok=xl6IrCgenEmitC(&output,&ir,&parse.diags); xl6OutputClose(&output);
    printf("Array create: %u producers; %u mutations refused before C/header; layout-mask=%u\n",producers,mutations,mask);
    printf("Dynamic arithmetic: %u checked producers; %u mutations refused before C/header\n",dynamic,dynamicMutations);
done:
    if (!ok) {
        char failurePath[4096];
        int size=snprintf(failurePath,sizeof(failurePath),"%s.failed.json",argv[2]);
        if (size>0 && (size_t)size<sizeof(failurePath) && xl6OutputOpenFile(&output,failurePath)) {
            /* Ordinary IR dumps deliberately refuse an unverified module.
             * Keep a non-executable structural trace for failed CFG audits. */
            xl6OutputPrintf(&output,"{\"instructions\":[");
            for (size_t n=0;n<ir.instructionCount;++n) {
                const XL6_IrInstruction* row=&ir.instructions[n];
                xl6OutputPrintf(&output,"%s{\"index\":%zu,\"kind\":%d,\"runtime\":%d,\"target\":%u,\"result\":%u,\"origin\":%u,\"true\":%u,\"false\":%u,\"operands\":[",
                    n?",":"",n,(int)row->kind,(int)row->runtimeOp,row->targetSymbol,row->result,row->origin,row->trueTarget,row->falseTarget);
                for (size_t p=0;p<row->operandCount;++p)
                    xl6OutputPrintf(&output,"%s%u",p?",":"",ir.operands[row->firstOperand+p]);
                xl6OutputPuts("]}",&output);
            }
            xl6OutputPuts("],\"blocks\":[",&output);
            for (size_t n=0;n<ir.blockCount;++n) {
                const XL6_IrBlock* row=&ir.blocks[n];
                xl6OutputPrintf(&output,"%s{\"id\":%zu,\"first\":%zu,\"count\":%zu}",n?",":"",n+1,row->firstInstruction,row->instructionCount);
            }
            xl6OutputPuts("]}\n",&output); xl6OutputClose(&output);
        }
    }
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}

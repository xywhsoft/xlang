#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_INDEX",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    assert(argc==3 || argc==4); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir); int ok=0; unsigned reads=0,mutations=0;
    if(!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"owned_index_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if(instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrOwnedIndexReadRuntimeOpIs(instruction->runtimeOp)) continue;
        assert(xl6IrOwnedIndexReadRuntimeInstructionCompatible(&ir,instruction));
        assert(xl6IrValueOwnershipGet(&ir,instruction->result)==XL6_IR_OWNERSHIP_MOVE);
        for(unsigned operand=0;operand<2;++operand)
            assert(xl6IrInstructionOperandOwnership(&ir,instruction,operand)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrRuntimeContainerAllocationErrorPhysicalAbi(instruction->runtimeOp)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        XL6_IrInstruction saved=*instruction;
#define MUTATE(Field,Value) do {instruction->Field=(Value); \
    assert(!xl6IrOwnedIndexReadRuntimeInstructionCompatible(&ir,instruction)); \
    rejected(&ir); *instruction=saved; ++mutations;} while(0)
        MUTATE(type,XL6_IR_TYPE_INVALID); MUTATE(result,XL6_IR_VALUE_INVALID);
        MUTATE(firstOperand,SIZE_MAX); MUTATE(operandCount,0); MUTATE(operandCount,1); MUTATE(operandCount,3);
        MUTATE(immediateText,(char*)"foreign"); MUTATE(immediateData,(unsigned char*)"foreign"); MUTATE(immediateSize,1);
        MUTATE(payloadType,saved.type); MUTATE(calleeType,saved.type); MUTATE(runtimeIdentityType,saved.type);
        MUTATE(targetValue,saved.result); MUTATE(targetSymbol,UINT32_MAX); MUTATE(collectionAbi,XL6_IR_COLLECTION_ARRAY);
        MUTATE(typedCollectionOp,XL6_IR_TYPED_COLLECTION_CLEAR); MUTATE(dynamicCollectionOp,XL6_IR_DYNAMIC_COLLECTION_CLEAR);
        MUTATE(sequenceAdmissionIdentitySet,1); MUTATE(opcode,XL6_IR_OPCODE_ADD);
        MUTATE(runtimeOp,XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64);
#undef MUTATE
        for(unsigned operand=0;operand<2;++operand) {
            XL6_IrValueId* cell=&ir.operands[saved.firstOperand+operand],value=*cell;
            *cell=XL6_IR_VALUE_INVALID; rejected(&ir); *cell=value; ++mutations;
        }
        XL6_IrInstruction* test=&ir.instructions[i+1]; XL6_IrInstruction outcome=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1 &&
            ir.operands[test->firstOperand]==saved.result);
        test->kind=XL6_IR_INSTR_INVALID; rejected(&ir); *test=outcome; ++mutations; ++reads;
    }
    assert(reads && xl6IrVerifyModule(&ir,NULL));
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    if(argc==4) {
        unsigned bindings=0; assert(xl6OutputOpenFile(&out,argv[3]));
        for(size_t i=0;i<ir.functionCount;++i) {
            const XL6_IrFunction* f=&ir.functions[i]; if(!f->debugName) continue;
            const char* suffix=!strcmp(f->debugName,"readChar") ? "CHAR" :
                !strcmp(f->debugName,"readSafe") ? "SAFE" : !strcmp(f->debugName,"readDynamic") ? "KEYED" : NULL;
            if(!suffix) continue;
            const XL6_IrType* type=xl6IrTypeGet(&ir.types,f->returnType); assert(type && type->errorResultCName);
            xl6OutputPrintf(&out,"#define XL6_INDEX_%s xlir_fn_%u\n#define XL6_INDEX_%s_RESULT %s\n",
                suffix,(unsigned)f->symbol,suffix,type->errorResultCName); ++bindings;
        }
        for(size_t i=0;i<ir.types.count;++i) {
            const XL6_IrType* type=&ir.types.items[i];
            if(type->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || type->collectionAbi!=XL6_IR_COLLECTION_ARRAY ||
                type->paramCount!=1 || !type->params) continue;
            const XL6_IrType* element=xl6IrTypeGet(&ir.types,type->params[0]);
            if(!element || element->cAbi!=XL6_IR_C_ABI_CHAR_U32) continue;
            xl6OutputPrintf(&out,"#define XL6_INDEX_OWNER 0x%016llXULL\n#define XL6_INDEX_ELEMENT 0x%016llXULL\n",
                (unsigned long long)type->typeValueId,(unsigned long long)element->typeValueId); ++bindings;
        }
        for(size_t i=0;i<ir.storageCount;++i) {
            const XL6_IrStorage* s=&ir.storages[i]; if(s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
            for(size_t j=0;j<ir.nodeCount;++j) {
                const XL6_IrNode* n=&ir.nodes[j];
                if(n->symbol==s->symbol && n->text && !strcmp(n->text,"cleanupCount")) {
                    xl6OutputPrintf(&out,"#define XL6_INDEX_CLEANUPS s%u\n",(unsigned)s->symbol); ++bindings; break;
                }
            }
        }
        assert(bindings==5); xl6OutputClose(&out);
    }
    printf("owned index contract: %u operations; %u mutations refused before C/header output\n",reads,mutations); ok=1;
done:
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}

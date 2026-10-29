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
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_CURSOR",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    assert(argc==3 || argc==4); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir); int ok=0; unsigned operations=0,mutations=0;
    if(!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"checked_cursor_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if(instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL || instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE) continue;
        assert(xl6IrValueIteratorAdvanceRuntimeInstructionCompatible(&ir,instruction));
        assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrRuntimeContainerAllocationErrorPhysicalAbi(instruction->runtimeOp)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        XL6_IrInstruction saved=*instruction;
#define MUTATE(Field,Value) do {instruction->Field=(Value); \
    assert(!xl6IrValueIteratorAdvanceRuntimeInstructionCompatible(&ir,instruction)); \
    rejected(&ir); *instruction=saved; ++mutations;} while(0)
        MUTATE(type,XL6_IR_TYPE_INVALID); MUTATE(result,XL6_IR_VALUE_INVALID);
        MUTATE(firstOperand,SIZE_MAX); MUTATE(operandCount,0); MUTATE(operandCount,2);
        MUTATE(immediateText,(char*)"foreign"); MUTATE(immediateData,(unsigned char*)"foreign"); MUTATE(immediateSize,1);
        MUTATE(payloadType,saved.type); MUTATE(calleeType,saved.type); MUTATE(runtimeIdentityType,saved.type);
        MUTATE(targetValue,saved.result); MUTATE(targetSymbol,UINT32_MAX); MUTATE(collectionAbi,XL6_IR_COLLECTION_ARRAY);
        MUTATE(typedCollectionOp,XL6_IR_TYPED_COLLECTION_CLEAR); MUTATE(dynamicCollectionOp,XL6_IR_DYNAMIC_COLLECTION_CLEAR);
        MUTATE(opcode,XL6_IR_OPCODE_ADD); MUTATE(runtimeOp,XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL);
#undef MUTATE
        XL6_IrValueId* operand=ir.operands+saved.firstOperand; XL6_IrValueId value=*operand;
        *operand=XL6_IR_VALUE_INVALID; rejected(&ir); *operand=value; ++mutations;
        XL6_IrInstruction* test=&ir.instructions[i+1]; XL6_IrInstruction outcome=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1 &&
            ir.operands[test->firstOperand]==saved.result);
        test->kind=XL6_IR_INSTR_INVALID; rejected(&ir); *test=outcome; ++mutations;
        XL6_IrValueIteratorStorageAbi storage=ir.abi.valueIteratorStorageAbi;
        ir.abi.valueIteratorStorageAbi=XL6_IR_VALUE_ITERATOR_STORAGE_NONE;
        rejected(&ir); ir.abi.valueIteratorStorageAbi=storage; ++mutations; ++operations;
    }
    assert(operations && xl6IrVerifyModule(&ir,NULL));
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    if(argc==4) {
        unsigned bindings=0; assert(xl6OutputOpenFile(&out,argv[3]));
        for(size_t i=0;i<ir.functionCount;++i) {
            const XL6_IrFunction* f=&ir.functions[i];
            if(!f->debugName) continue;
            const char* suffix=!strcmp(f->debugName,"counted") ? "COUNTED" :
                !strcmp(f->debugName,"countedArray") ? "ARRAY" :
                !strcmp(f->debugName,"countedTextArray") ? "TEXT_ARRAY" : NULL;
            if (!suffix) continue;
            const XL6_IrType* type=xl6IrTypeGet(&ir.types,f->returnType); assert(type && type->errorResultCName);
            xl6OutputPrintf(&out,"#define XL6_ITER_%s xlir_fn_%u\n#define XL6_ITER_%s_RESULT %s\n",
                suffix,(unsigned)f->symbol,suffix,type->errorResultCName); ++bindings;
        }
        for (size_t i=0;i<ir.types.count;++i) {
            const XL6_IrType* type=&ir.types.items[i];
            if (type->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || type->collectionAbi!=XL6_IR_COLLECTION_ARRAY ||
                type->paramCount!=1 || !type->params) continue;
            const XL6_IrType* element=xl6IrTypeGet(&ir.types,type->params[0]); assert(element);
            const char* suffix=element->cAbi==XL6_IR_C_ABI_I64 ? "ARRAY" :
                element->cAbi==XL6_IR_C_ABI_MANAGED_STRING ? "TEXT_ARRAY" : NULL;
            if (!suffix) continue;
            xl6OutputPrintf(&out,"#define XL6_ITER_%s_OWNER 0x%016llXULL\n#define XL6_ITER_%s_ELEMENT 0x%016llXULL\n",
                suffix,(unsigned long long)type->typeValueId,suffix,(unsigned long long)element->typeValueId); ++bindings;
        }
        for(size_t i=0;i<ir.storageCount;++i) {
            const XL6_IrStorage* s=&ir.storages[i]; if(s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
            for(size_t j=0;j<ir.nodeCount;++j) {
                const XL6_IrNode* n=&ir.nodes[j];
                if(n->symbol==s->symbol && n->text && !strcmp(n->text,"cleanupCount")) {
                    xl6OutputPrintf(&out,"#define XL6_ITER_CLEANUPS s%u\n",(unsigned)s->symbol); ++bindings; break;
                }
            }
        }
        assert(bindings==6); xl6OutputClose(&out);
    }
    printf("checked cursor contract: %u operations; %u mutations refused before C/header output\n",operations,mutations); ok=1;
done:
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}

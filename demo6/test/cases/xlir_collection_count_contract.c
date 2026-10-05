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
        XL6_Output output; XL6_DiagList diagnostics; size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"REJECTED_COUNT",&diagnostics) :
            xl6IrCgenEmitC(&output,ir,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
}
int main(int argc,char** argv)
{
    assert(argc==3 || argc==4);
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=0; unsigned counts=0,mutations=0;
    if(!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"collection_count_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if(instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            !xl6IrCollectionCountRuntimeOpIs(instruction->runtimeOp)) continue;
        assert(xl6IrCollectionCountRuntimeInstructionCompatible(&ir,instruction));
        assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        XL6_IrInstruction saved=*instruction;
#define MUTATE(Field,Value) do { instruction->Field=(Value); \
    assert(!xl6IrCollectionCountRuntimeInstructionCompatible(&ir,instruction)); \
    rejected(&ir); *instruction=saved; ++mutations; } while(0)
        MUTATE(type,XL6_IR_TYPE_INVALID);
        MUTATE(result,XL6_IR_VALUE_INVALID);
        MUTATE(firstOperand,SIZE_MAX);
        MUTATE(operandCount,0);
        MUTATE(operandCount,2);
        MUTATE(immediateText,(char*)"foreign");
        MUTATE(immediateData,(unsigned char*)"foreign");
        MUTATE(immediateSize,1);
        MUTATE(payloadType,saved.type);
        MUTATE(calleeType,saved.type);
        MUTATE(runtimeIdentityType,saved.type);
        MUTATE(targetValue,saved.result);
        MUTATE(targetSymbol,UINT32_MAX);
        MUTATE(collectionAbi,XL6_IR_COLLECTION_ARRAY);
        MUTATE(typedCollectionOp,XL6_IR_TYPED_COLLECTION_CLEAR);
        MUTATE(dynamicCollectionOp,XL6_IR_DYNAMIC_COLLECTION_CLEAR);
        MUTATE(opcode,XL6_IR_OPCODE_ADD);
        MUTATE(runtimeOp,saved.runtimeOp==XL6_IR_RUNTIME_XVALUE_COUNT ?
            XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL : XL6_IR_RUNTIME_XVALUE_COUNT);
#undef MUTATE
        assert(i+1<ir.instructionCount && ir.instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction outcome=ir.instructions[i+1];
        ir.instructions[i+1].kind=XL6_IR_INSTR_CONST;
        rejected(&ir); ir.instructions[i+1]=outcome; ++mutations;
        assert(xl6IrVerifyModule(&ir,NULL)); ++counts;
    }
    assert(counts);
    if(!xl6OutputOpenFile(&output,argv[2])) goto done;
    ok=xl6IrCgenEmitC(&output,&ir,&parse.diags); xl6OutputClose(&output);
    if(ok && argc==4) {
        assert(xl6OutputOpenFile(&output,argv[3]));
        unsigned bindings=0;
        for(size_t i=0;i<ir.functionCount;++i) {
            const XL6_IrFunction* function=&ir.functions[i];
            if(!function->debugName) continue;
            const char* macro=strcmp(function->debugName,"countErased")==0 ? "XL6_COUNT_ERASED" :
                strcmp(function->debugName,"countSafeErased")==0 ? "XL6_COUNT_SAFE" :
                strcmp(function->debugName,"countOptional")==0 ? "XL6_COUNT_OPTIONAL" : NULL;
            if(!macro) continue;
            xl6OutputPrintf(&output,"#define %s xlir_fn_%u\n",macro,(unsigned)function->symbol);
            if(strcmp(macro,"XL6_COUNT_OPTIONAL")==0)
                xl6OutputPrintf(&output,"#define XL6_COUNT_OPTIONAL_RESULT %s\n",
                    xl6IrTypeGet(&ir.types,function->returnType)->errorResultCName);
            ++bindings;
        }
        for(size_t i=0;i<ir.types.count;++i) {
            const XL6_IrType* type=&ir.types.items[i];
            if(type->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || type->collectionAbi!=XL6_IR_COLLECTION_ARRAY ||
                type->paramCount!=1 || !type->params) continue;
            const XL6_IrType* element=xl6IrTypeGet(&ir.types,type->params[0]);
            if(element->cAbi!=XL6_IR_C_ABI_I64 || element->integerTarget!=XL6_IR_INTEGER_TARGET_SIGNED_64) continue;
            xl6OutputPrintf(&output,"#define XL6_COUNT_OWNER_ID 0x%016llXULL\n#define XL6_COUNT_ELEMENT_ID 0x%016llXULL\n",
                (unsigned long long)type->typeValueId,(unsigned long long)element->typeValueId); ++bindings; break;
        }
        assert(bindings==4 && !xl6OutputHasFailed(&output)); xl6OutputClose(&output);
    }
    printf("count contract: %u operations; %u ABI/outcome mutations refused before C/header output\n",counts,mutations);
done:
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}

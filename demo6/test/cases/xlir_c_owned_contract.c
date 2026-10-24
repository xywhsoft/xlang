#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void checkSpanContracts(XL6_IrModule* ir)
{
    for(size_t i=0;i<ir->callTargetCount;++i) {
        XL6_IrCallTarget* target=&ir->callTargets[i];
        for(size_t p=0;p<target->parameterCount;++p) {
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_C_SPAN_BORROW)continue;
            XL6_DiagList diags={0};
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            assert(!xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_C_SPAN_BORROW;
            target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;
            assert(!xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
            target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_BORROW;
            assert(xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
        }
    }
}

static void checkOptionalContracts(XL6_IrModule* ir)
{
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        if(instruction->runtimeOp!=XL6_IR_RUNTIME_C_COMPARE_EQ || instruction->operandCount!=2)continue;
        const XL6_IrInstruction* source=NULL;
        XL6_IrValueId value=ir->operands[instruction->firstOperand];
        for(size_t j=0;j<ir->instructionCount;++j)
            if(ir->instructions[j].result==value){source=&ir->instructions[j];break;}
        const XL6_IrType* type=source ? xl6IrTypeGet(&ir->types,source->type) : NULL;
        if(!type || type->kind!=XL6_IR_TYPE_NULLABLE)continue;
        XL6_DiagList diags={0};
        instruction->runtimeOp=XL6_IR_RUNTIME_C_COMPARE_LT;
        assert(!xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
        instruction->runtimeOp=XL6_IR_RUNTIME_C_COMPARE_EQ;
        assert(xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
    }
}

static void checkStringResultContracts(XL6_IrModule* ir)
{
    for(size_t i=0;i<ir->callableBridgeResultCount;++i) {
        XL6_IrCallableBridgeResult* slot=&ir->callableBridgeResults[i];
        if(slot->encodeAbi!=XL6_IR_CALLABLE_BRIDGE_STRING_MOVE)continue;
        XL6_DiagList diags={0};
        slot->encodeAbi=XL6_IR_CALLABLE_BRIDGE_STRING_COPY;
        assert(!xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
        slot->encodeAbi=XL6_IR_CALLABLE_BRIDGE_STRING_MOVE;
        assert(xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);
    }
}

static void checkVariadicPackContracts(XL6_IrModule* ir)
{
    unsigned rejected=0;
    for(size_t s=0;s<ir->callSiteCount;++s) {
        XL6_IrCallSite* site=&ir->callSites[s];
        if(site->argumentCount!=site->fixedParameterCount+1)continue;
        XL6_IrCallArgument* pack=&ir->callArguments[site->firstArgument+site->fixedParameterCount];
        if(pack->source!=XL6_IR_CALL_ARGUMENT_VARIADIC_PACK)continue;
        XL6_IrNode* expression=&ir->nodes[pack->expression-1];
        XL6_IrNode* wrapper=&ir->nodes[expression->parent-1];
        XL6_DiagList diags={0};
#define REJECT_PACK_MUTATION(change,restore) do { change; \
    assert(!xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags);++rejected; \
    restore;assert(xl6IrVerifyModule(ir,&diags));xl6DiagFree(&diags); } while(0)
        XL6_IrCallArgumentSource source=pack->source;
        REJECT_PACK_MUTATION(pack->source=XL6_IR_CALL_ARGUMENT_ARRAY_VARARG_ITEM,pack->source=source);
        REJECT_PACK_MUTATION(pack->source=XL6_IR_CALL_ARGUMENT_DICT_VARARG_ITEM,pack->source=source);
        unsigned flags=wrapper->flags;
        REJECT_PACK_MUTATION(wrapper->flags=0,wrapper->flags=flags);
        REJECT_PACK_MUTATION(wrapper->flags|=XL6_AST_FLAG_BYVAL,wrapper->flags=flags);
        XL6_IrTypeId type=expression->storageType;
        REJECT_PACK_MUTATION(expression->storageType=XL6_IR_TYPE_INVALID,expression->storageType=type);
        size_t parameter=pack->parameter,evaluation=pack->evaluationIndex;
        REJECT_PACK_MUTATION(++pack->parameter,pack->parameter=parameter);
        REJECT_PACK_MUTATION(++pack->evaluationIndex,pack->evaluationIndex=evaluation);
        size_t count=site->argumentCount;
        REJECT_PACK_MUTATION(--site->argumentCount,site->argumentCount=count);
        XL6_IrCallVariadicAbi abi=site->variadicAbi;
        REJECT_PACK_MUTATION(site->variadicAbi=XL6_IR_CALL_VARIADIC_NONE,site->variadicAbi=abi);
#undef REJECT_PACK_MUTATION
    }
    if(rejected)printf("variadic pack contract: %u rejected mutations\n",rejected);
}

static void emitPackBindings(XL6_IrModule* ir,const char* generated)
{
    char path[4096];XL6_Output out;
    snprintf(path,sizeof(path),"%s.bind.h",generated);
    assert(xl6OutputOpenFile(&out,path));
    for(size_t s=0;s<ir->storageCount;++s) {
        const XL6_IrStorage* storage=&ir->storages[s];
        const XL6_IrNode* declaration=xl6IrNodeGet(ir,storage->declaration);
        if(storage->kind!=XL6_IR_STORAGE_GLOBAL || !declaration || !declaration->text)continue;
        if(!strcmp(declaration->text,"identityArray"))
            xl6OutputPrintf(&out,"#define OWNED_ARRAY_ADAPTER s%u\n",(unsigned)storage->symbol);
        if(!strcmp(declaration->text,"identityDict"))
            xl6OutputPrintf(&out,"#define OWNED_DICT_ADAPTER s%u\n",(unsigned)storage->symbol);
    }
    xl6OutputClose(&out);
}

int main(int argc,char** argv)
{
    XL6_ParseResult parsed; XL6_IrModule ir; XL6_Output out;
    if(argc!=3) return 2;
    xl6ParseResultInit(&parsed); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) &&
        xl6SemCheckModule(parsed.root,&parsed.diags,"c_owned") &&
        xl6IrBuildModuleFromAst(&ir,parsed.root,&parsed.diags);
    if(ok) {
        ok=xl6IrLowerModule(&ir,&parsed.diags);
        if(strcmp(argv[2],"check")) {
            char path[4096];snprintf(path,sizeof(path),"%s.json",argv[2]);
            if(xl6OutputOpenFile(&out,path)){xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);}
        }
    }
    if(ok){checkSpanContracts(&ir);checkOptionalContracts(&ir);checkStringResultContracts(&ir);checkVariadicPackContracts(&ir);}
    if(ok && strcmp(argv[2],"check")) {
        ok=xl6OutputOpenFile(&out,argv[2]);
        if(ok) {
            ok=xl6IrCgenEmitC(&out,&ir,&parsed.diags);
            if(ok) xl6OutputPrintf(&out,"\nint main(void) { return xlir_module_init() ? 0 : 3; }\n");
            xl6OutputClose(&out);
            if(ok)emitPackBindings(&ir,argv[2]);
        }
    }
    if(!ok) {
        xl6DiagPrintText(&parsed.diags,argv[1]);
        for(size_t f=0;f<ir.functionCount;++f) {
            const XL6_IrFunction* fn=&ir.functions[f];
            fprintf(stderr,"function %u blocks %zu..%zu\n",fn->declaration,fn->firstBlock+1,fn->firstBlock+fn->blockCount);
            for(size_t b=fn->firstBlock;b<fn->firstBlock+fn->blockCount;++b) {
                const XL6_IrBlock* block=&ir.blocks[b];
                const XL6_IrInstruction* last=block->instructionCount ? &ir.instructions[block->firstInstruction+block->instructionCount-1] : NULL;
                fprintf(stderr," block %zu count %zu last %d origin %u edges %u/%u\n",b+1,block->instructionCount,
                    last ? (int)last->kind : -1,last ? last->origin : 0,last ? last->trueTarget : 0,last ? last->falseTarget : 0);
            }
        }
    }
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parsed);
    return ok ? 0 : 1;
}

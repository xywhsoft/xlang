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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_BOUND_VIEW",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned raw=0,managed=0;
    for(size_t i=0;i<module->closureTargetCount;++i){
        XL6_IrClosureTarget* closure=&module->closureTargets[i];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER)continue;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,closure->adaptedTargetSymbol);assert(target && closure->captureCount==1);
        XL6_IrClosureCapture* capture=&module->closureCaptures[closure->firstCapture];
        const XL6_IrType* type=xl6IrTypeGet(&module->types,capture->type);
        if(type->cAbi==XL6_IR_C_ABI_RUNTIME_XVALUE_PTR){assert(capture->abi==XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE);++managed;continue;}
        assert(type->cAbi==XL6_IR_C_ABI_NATIVE_HANDLE && type->foreignHeader && capture->abi==XL6_IR_CLOSURE_CAPTURE_COPY_VALUE && !capture->cellType && !closure->boundMethodDispatchSlot);++raw;
        for(size_t n=0;n<module->instructionCount;++n){
            const XL6_IrInstruction* call=&module->instructions[n];
            if(call->kind!=XL6_IR_INSTR_CALL || call->origin!=closure->declaration || call->targetSymbol!=target->symbol)continue;
            assert(call->operandCount);
            XL6_IrValueId receiver=module->operands[call->firstOperand];
            const XL6_IrInstruction* load=NULL;
            for(size_t definition=0;definition<module->instructionCount;++definition)
                if(module->instructions[definition].result==receiver){load=&module->instructions[definition];break;}
            assert(load && load->kind==XL6_IR_INSTR_LOAD && load->targetSymbol==capture->storageSymbol && load->type==capture->type);
        }
        XL6_IrClosureCapture saved=*capture;
#define BAD(F,V) do{capture->F=(V);refused(module);*capture=saved;}while(0)
        BAD(abi,XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE);BAD(abi,XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL);
        BAD(sourceSymbol,0);BAD(storageSymbol,0);BAD(type,0);
#undef BAD
        XL6_IrClosureTarget old=*closure;
        closure->boundMethodDispatchSlot=1;refused(module);*closure=old;
        closure->captureCount=0;refused(module);*closure=old;
        closure->adaptedTargetSymbol=0;refused(module);*closure=old;
        for(size_t t=0;t<module->callTargetCount;++t){
            const XL6_IrCallTarget* other=&module->callTargets[t];
            if(other->symbol==target->symbol || other->parameterCount!=target->parameterCount ||
                !other->parameterCount || other->parameterTypes[0]!=target->parameterTypes[0] ||
                other->kind!=XL6_IR_CALL_TARGET_EXTERN_C)continue;
            closure->adaptedTargetSymbol=other->symbol;refused(module);*closure=old;
            if(other->returnType==target->returnType){
                for(size_t n=0;n<module->instructionCount;++n){
                    XL6_IrInstruction* call=&module->instructions[n];
                    if(call->kind!=XL6_IR_INSTR_CALL || call->origin!=closure->declaration || call->targetSymbol!=target->symbol)continue;
                    XL6_SymbolId selected=call->targetSymbol;call->targetSymbol=other->symbol;refused(module);call->targetSymbol=selected;
                }
            }
            break;
        }
        XL6_IrParameterAbi abi=target->parameterAbis[0];
        target->parameterAbis[0]=XL6_IR_PARAMETER_ABI_OWNED;refused(module);target->parameterAbis[0]=abi;
    }
    assert(raw>=5 && managed>=1 && xl6IrVerifyModule(module,NULL));
    printf("bound views: raw=%u managed=%u mutations=%u zero-output=%u\n",raw,managed,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

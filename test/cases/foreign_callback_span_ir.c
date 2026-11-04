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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_CALLBACK_SPAN",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned spans=0;
    for(size_t i=0;i<module->foreignCallbackSpanCount;++i){
        XL6_ForeignCallbackSpanContract* row=&module->foreignCallbackSpans[i];XL6_ForeignCallbackSpanContract saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,node->symbol);assert(target);++spans;
        size_t parameter=row->parameter; /* Both test producers are free functions. */
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,parameter);
        assert(signature && xl6IrForeignCallbackParameterMatches(module,target,parameter));
        row->declaration=0;refused(module);*row=saved;
        row->parameter=0;refused(module);*row=saved;
        row->parameter=64;refused(module);*row=saved;
        row->argument=64;refused(module);*row=saved;
        row->argument^=1;refused(module);*row=saved;
        row->countAbi=0;refused(module);*row=saved;
        row->pointerCName[0]='-';refused(module);*row=saved;
        memset(row->pointerCName,'x',sizeof(row->pointerCName));refused(module);*row=saved;
        node->foreignCall.callbackSpanMappings=0;refused(module);node->foreignCall=facts;
        ++node->foreignCall.callbackSpanMappings;refused(module);node->foreignCall=facts;
        XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&module->types,signature->params[row->argument]);XL6_IrType original=*type;
        type->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(module);*type=original;
        XL6_IrParameterAbi abi=signature->parameterAbis[row->argument];signature->parameterAbis[row->argument]=XL6_IR_PARAMETER_ABI_OWNED;refused(module);signature->parameterAbis[row->argument]=abi;
    }
    size_t count=module->foreignCallbackSpanCount;assert(count==3);--module->foreignCallbackSpanCount;refused(module);module->foreignCallbackSpanCount=count;
    assert(sizeof(XL6_ForeignCallFacts)==88 && xl6IrVerifyModule(module,NULL));
    printf("callback byte spans: spans=%u mutations=%u zero-output=%u\n",spans,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

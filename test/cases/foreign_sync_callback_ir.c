#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"accepted sync mutation %u\n",mutations);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_SYNC_CALLBACK",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned callbacks=0;
    for(size_t i=0;i<module->foreignParameterCount;++i){
        XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(row->kind!=XL6_FOREIGN_PARAMETER_SYNC_CALLBACK)continue;
        XL6_ForeignParameterContract saved=*row;XL6_IrNode* node=&module->nodes[row->declaration-1];
        XL6_ForeignCallFacts facts=node->foreignCall;XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(module,node->symbol);
        assert(target);size_t p=0;while(p<target->parameterCount && xl6IrForeignParameterGet(module,target,p)!=row)++p;
        assert(p<target->parameterCount && xl6IrForeignParameterWidth(module,target,p)==2);++callbacks;
        row->kind=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK;refused(module);*row=saved;
        row->kind=0;refused(module);*row=saved;
        row->callbackOwnerIndexPlusOne=65;refused(module);*row=saved;
        row->callbackOwnerCName[0]='x';refused(module);*row=saved;
        row->callbackTypeCName[0]='z';refused(module);*row=saved;
        row->callbackResultKind=XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT;refused(module);*row=saved;
        row->countAbi=XL6_FOREIGN_SPAN_COUNT_INT;refused(module);*row=saved;
        row->readOnly=true;refused(module);*row=saved;
        node->foreignCall.physicalPairs=0;refused(module);node->foreignCall=facts;
        node->foreignCall.retainedCallbacks|=UINT64_C(1)<<row->parameter;refused(module);node->foreignCall=facts;
        XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
        target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK;refused(module);target->parameterPhysicalAbis[p]=abi;
        target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK;refused(module);target->parameterPhysicalAbis[p]=abi;
        uint32 flags=node->flags;node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module);node->flags=flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_NONBLOCKING;refused(module);node->flags=flags;
        XL6_IrErrorAbi error=target->errorAbi;target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);target->errorAbi=error;
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,p);
        if(!signature->returnCount){row->callbackFailureCode=17;refused(module);*row=saved;}
    }
    assert(callbacks==12 && sizeof(XL6_ForeignCallFacts)==88 && xl6IrVerifyModule(module,NULL));
    printf("sync callbacks: callbacks=%u mutations=%u zero-output=%u\n",callbacks,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

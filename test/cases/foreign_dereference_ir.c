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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_SCOPE_VIEW",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    unsigned projections=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(target->foreignOperationAbi!=XL6_IR_FOREIGN_OPERATION_DEREF_READ && target->foreignOperationAbi!=XL6_IR_FOREIGN_OPERATION_DEREF_WRITE)continue;
        ++projections;XL6_IrCallTarget saved=*target;
        XL6_IrNode* node=&module->nodes[target->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;uint32_t flags=node->flags;
        target->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_CALL;refused(module);*target=saved;
        target->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_FIELD_READ;refused(module);*target=saved;
        target->foreignOperationAbi=target->foreignOperationAbi==XL6_IR_FOREIGN_OPERATION_DEREF_READ ? XL6_IR_FOREIGN_OPERATION_DEREF_WRITE : XL6_IR_FOREIGN_OPERATION_DEREF_READ;refused(module);*target=saved;
        target->cName="different";refused(module);*target=saved;
        target->runtimeBridge=XL6_IR_RUNTIME_BRIDGE_BUFFER;refused(module);*target=saved;
        node->foreignCall.receiverDereference=0;refused(module);node->foreignCall=facts;
        node->foreignCall.receiverDereference=2;refused(module);node->foreignCall=facts;
        node->foreignCall.inputMappingFacts=1;refused(module);node->foreignCall=facts;
        node->flags|=XL6_AST_FLAG_FOREIGN_FIELD;refused(module);node->flags=flags;
        node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module);node->flags=flags;
    }
    assert(projections>=4 && sizeof(XL6_ForeignCallFacts)==88 && xl6IrVerifyModule(module,NULL));
    printf("native pointee projections: operations=%u mutations=%u zero-output=%u\n",projections,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

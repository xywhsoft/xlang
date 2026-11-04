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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_RETAIN",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned results=0;
    for(size_t i=0;i<module->foreignRetainCount;++i){
        XL6_ForeignRetainContract* row=&module->foreignRetains[i];XL6_ForeignRetainContract saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,node->symbol);assert(target && xl6IrForeignRetainResultMatches(module,target));++results;
        row->declaration=0;refused(module);*row=saved;
        row->retainSymbol=0;refused(module);*row=saved;
        row->retainSymbol=saved.rollbackSymbol;refused(module);*row=saved;
        row->retainSymbol=node->symbol;refused(module);*row=saved;
        row->rollbackSymbol=saved.retainSymbol;refused(module);*row=saved;
        row->rollbackSymbol=node->symbol;refused(module);*row=saved;
        row->success=0;refused(module);*row=saved;
        row->success=99;refused(module);*row=saved;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=1;refused(module);node->foreignCall=facts;
        node->foreignCall.inplaceOutput=1;refused(module);node->foreignCall=facts;
        for(unsigned helper=0;helper<2;++helper){
            XL6_IrCallTarget* entry=(XL6_IrCallTarget*)xl6IrCallTargetGet(module,helper ? row->rollbackSymbol:row->retainSymbol);XL6_IrCallTarget original=*entry;
            XL6_IrNode* decl=&module->nodes[entry->declaration-1];uint32 flags=decl->flags;
            decl->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module);decl->flags=flags;
            decl->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module);decl->flags=flags;
            entry->returnType=target->returnType;refused(module);*entry=original;
            entry->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_BORROW;refused(module);*entry=original;
            XL6_IrParameterAbi parameter=entry->parameterAbis[0];entry->parameterAbis[0]=XL6_IR_PARAMETER_ABI_OWNED;refused(module);entry->parameterAbis[0]=parameter;
            XL6_IrCallParameterPhysicalAbi physical=entry->parameterPhysicalAbis[0];entry->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);entry->parameterPhysicalAbis[0]=physical;
        }
    }
    size_t count=module->foreignRetainCount;assert(count==4);--module->foreignRetainCount;refused(module);module->foreignRetainCount=count;
    assert(sizeof(XL6_ForeignCallFacts)==88 && sizeof(XL6_ForeignRetainContract)==16 && xl6IrVerifyModule(module,NULL));
    printf("independent retained native results: declarations=%u mutations=%u zero-output=%u\n",results,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

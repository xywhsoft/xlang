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
    assert(module->foreignFieldLengthCount>=3);
    for(size_t i=0;i<module->foreignFieldLengthCount;++i){
        XL6_ForeignFieldLengthContract* row=&module->foreignFieldLengths[i],saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;uint32_t flags=node->flags;
        row->declaration=0;refused(module);*row=saved;
        row->countAbi=0;refused(module);*row=saved;
        row->field[0]=0;refused(module);*row=saved;
        row->field[0]='+';refused(module);*row=saved;
        memset(row->field,'x',sizeof(row->field));refused(module);*row=saved;
        if(i){row->declaration=module->foreignFieldLengths[0].declaration;refused(module);*row=saved;}
        node->flags&=~XL6_AST_FLAG_FOREIGN_FIELD;refused(module);node->flags=flags;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_TEXT;refused(module);node->foreignCall=facts;
        node->foreignCall.outputIndexPlusOne=1;refused(module);node->foreignCall=facts;
    }
    size_t count=module->foreignFieldLengthCount;--module->foreignFieldLengthCount;refused(module);module->foreignFieldLengthCount=count;
    assert(xl6IrVerifyModule(module,NULL));
    printf("counted native fields: rows=%zu mutations=%u zero-output=%u\n",count,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

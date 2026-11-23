#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_SIGNED_OUTPUT",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignOwnedOutputContract)==28);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL) && m->foreignOwnedOutputsCount==4);
    for(size_t i=0;i<m->foreignOwnedOutputsCount;++i){
        XL6_ForeignOwnedOutputContract* row=&m->foreignOwnedOutputs[i];
        XL6_ForeignOwnedOutputContract saved=*row;assert(row->sizeAbi==XL6_FOREIGN_OWNED_OUTPUT_INT);
        row->sizeAbi=0;refused(m);*row=saved;
        row->sizeAbi=99;refused(m);*row=saved;
        row->sizeIndex=row->dataIndex;refused(m);*row=saved;
        row->successCode^=1;refused(m);*row=saved;
        row->scalarIndexPlusOne=(uint8)(row->sizeIndex+1);refused(m);*row=saved;
        row->scalarIndexPlusOne=65;refused(m);*row=saved;
        row->scalarIndexPlusOne=saved.scalarIndexPlusOne ? 0:4;refused(m);*row=saved;
    }
    assert(xl6IrVerifyModule(m,NULL));
    printf("Owned signed IR: exact int rows, stable 28-byte contract, %u mutations rejected before C/header emission\n",mutations);
    xl6CompileUnitFree(&unit);return 0;
}

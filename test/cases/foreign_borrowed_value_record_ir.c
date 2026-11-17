#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;

static void refused(XL6_IrModule* module,const char* name)
{
    if(xl6IrVerifyModule(module,NULL)){
        fprintf(stderr,"IR accepted an invalid borrowed value field: %s\n",name);
        assert(0);
    }
    for(unsigned header=0;header<2;++header){
        XL6_Output out;
        size_t size=SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        int accepted=header ? xl6IrCgenEmitHeader(&out,module,"INVALID_BORROWED_VALUE",NULL):
            xl6IrCgenEmitC(&out,module,NULL);
        assert(!accepted);
        char* bytes=xl6OutputTakeMemoryText(&out,&size);
        assert(bytes && size==0);
        xrtFree(bytes);
        xl6OutputDiscard(&out);
    }
    ++mutations;
}

int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;
    assert(xl6IrVerifyModule(module,NULL));
    unsigned borrowedFields=0;
    for(size_t i=0;i<module->foreignOutputViewCount;++i){
        XL6_ForeignOutputViewContract* row=&module->foreignOutputViews[i];
        XL6_ForeignOutputViewContract saved=*row;
        if(saved.conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_BORROWED_VALUE)continue;
        ++borrowedFields;
        row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_OWNED_VALUE;
        refused(module,"owned transfer from a borrowed record");
        *row=saved;
        row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING;
        refused(module,"text conversion on var");
        *row=saved;
        row->ordinal=63;refused(module,"result ordinal");*row=saved;
        row->physicalIndex=63;refused(module,"physical slot");*row=saved;
        row->logicalType=0;refused(module,"logical type");*row=saved;
        row->retainSymbol=1;refused(module,"custom retain");*row=saved;
        row->rollbackSymbol=1;refused(module,"custom release");*row=saved;
        row->retainSuccess=XL6_FOREIGN_RETAIN_ZERO;refused(module,"retain result ABI");*row=saved;
        XL6_IrNode* node=&module->nodes[saved.declaration-1];
        uint64 flags=node->flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(module,"pure producer");node->flags=flags;
        node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(module,"missing producer contract");node->flags=flags;
        uint8 mode=node->foreignCall.valueOutput;
        node->foreignCall.valueOutput=XL6_FOREIGN_VALUE_OUTPUT_STATUS|XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;
        refused(module,"borrowed value outside a record");
        node->foreignCall.valueOutput=mode;
    }
    assert(borrowedFields==5 && xl6IrVerifyModule(module,NULL));
    printf("BORROWED_VALUE_RECORD_IR_OK %u\n",mutations);
    xl6CompileUnitFree(&unit);
    return 0;
}

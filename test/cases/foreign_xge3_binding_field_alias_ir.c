#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_FIELD_ALIAS",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && bytes==0);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    unsigned found=0;
    for(size_t i=0;i<ir->aggregateFieldCount;++i){
        XL6_IrAggregateField* field=&ir->aggregateFields[i];
        if(strcmp(field->name,"endValue"))continue;
        assert(!strcmp(field->cName,"end"));++found;
        XL6_IrAggregateField saved=*field;
        strcpy(field->cName,"start");refused(ir);*field=saved;
        field->name="start";refused(ir);*field=saved;
        XL6_IrNode* source=&ir->nodes[field->declaration-1];
        XL6_IrNode sourceSaved=*source;
        source->sourceKind=XL6_AST_FIELD_ATTR;refused(ir);*source=sourceSaved;
        source->childCount=1;refused(ir);*source=sourceSaved;
        XL6_IrNode* attr=&ir->nodes[ir->children[source->firstChild+1]-1];
        XL6_IrNode attrSaved=*attr;
        attr->text="other";refused(ir);*attr=attrSaved;
        attr->childCount=2;refused(ir);*attr=attrSaved;
        XL6_IrNode* literal=&ir->nodes[ir->children[attr->firstChild]-1];
        XL6_IrNode literalSaved=*literal;
        literal->sourceKind=XL6_AST_NAME;refused(ir);*literal=literalSaved;
        literal->text="\"start\"";refused(ir);*literal=literalSaved;
        literal->text="\"end.other\"";refused(ir);*literal=literalSaved;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(found==1 && mutations==9);
    printf("C member aliases: %u invalid IR mutations rejected before C/header output\n",mutations);
    xl6CompileUnitFree(&unit);return 0;
}

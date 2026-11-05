#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m,const char* label)
{
    if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"unexpected valid nested/NULL IR: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_NESTED",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char* bytes=xl6OutputTakeMemoryText(&out,&size);assert(bytes && !size);xrtFree(bytes);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));assert(m->foreignNullCount>=7);
    for(size_t i=0;i<m->foreignPackFieldCount;++i){XL6_ForeignPackFieldContract* row=&m->foreignPackFields[i];XL6_ForeignPackFieldContract saved=*row;
        row->recordParentIndexPlusOne=UINT32_MAX;refused(m,"parent row out of bounds");*row=saved;
        if(strchr(row->field,'.')){row->recordParentIndexPlusOne=0;refused(m,"missing nested parent identity");*row=saved;}
        strcpy(row->field,"view->payload");refused(m,"path expression injection");*row=saved;
        strcpy(row->field,"view..payload");refused(m,"empty path member");*row=saved;
        row->packPhysicalIndex=63;refused(m,"orphan field");*row=saved;
        if(row->kind==XL6_FOREIGN_PACK_FIELD_RECORD){
            row->parameter=0;refused(m,"record witness consumes logical input");*row=saved;
            row->logicalType=1;refused(m,"record witness becomes language record");*row=saved;
            strcpy(row->physicalCName,"void*");refused(m,"pointer intermediate");*row=saved;
            row->countAbi=XL6_FOREIGN_SPAN_COUNT_INT;refused(m,"record witness span count");*row=saved;
        }
    }
    for(size_t i=0;i<m->foreignNullCount;++i){XL6_ForeignNullContract* row=&m->foreignNulls[i];XL6_ForeignNullContract saved=*row;
        row->declaration=0;refused(m,"orphan NULL");*row=saved;
        row->physicalIndex=63;refused(m,"unreachable NULL slot");*row=saved;
        strcpy(row->pointerCName,"int");refused(m,"nonpointer NULL");*row=saved;
        strcpy(row->pointerCName,"int(*)(int)");refused(m,"function pointer NULL");*row=saved;
        XL6_IrNode* node=&m->nodes[row->declaration-1];uint16 old=node->foreignCall.inputMappingFacts;
        node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_NULLS_PRESENT;refused(m,"lost NULL presence");node->foreignCall.inputMappingFacts=old;
        node->foreignCall.inputMappingFacts|=32768u;refused(m,"unknown input metadata bit");node->foreignCall.inputMappingFacts=old;
        uint64 flags=node->flags;node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(m,"NULL native status effects");node->flags=flags;
    }
    size_t count=m->foreignNullCount;m->foreignNullCount=m->foreignNullCapacity+1;refused(m,"NULL storage count");m->foreignNullCount=count;
    XL6_ForeignNullContract* rows=m->foreignNulls;m->foreignNulls=NULL;refused(m,"NULL row storage missing");m->foreignNulls=rows;
    assert(xl6IrVerifyModule(m,NULL));printf("nested pack/NULL: %u mutations rejected with zero C/header output\n",mutations);
    xl6CompileUnitFree(&unit);return 0;
}

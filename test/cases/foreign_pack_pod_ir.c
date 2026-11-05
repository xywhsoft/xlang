#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m,const char* label)
{
    if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"unexpected valid packed POD IR: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_PACKED_POD",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char* bytes=xl6OutputTakeMemoryText(&out,&size);assert(bytes && !size);xrtFree(bytes);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));unsigned records=0;
    for(size_t i=0;i<m->foreignPackFieldCount;++i){XL6_ForeignPackFieldContract* row=&m->foreignPackFields[i];
        if(row->kind!=XL6_FOREIGN_PACK_FIELD_VALUE)continue;
        XL6_IrType* type=&m->types.items[row->logicalType-1];if(type->kind!=XL6_IR_TYPE_C_RECORD)continue;
        ++records;XL6_ForeignPackFieldContract saved=*row;
        row->logicalType=0;refused(m,"logical POD type");*row=saved;
        row->parameter=63;refused(m,"POD parameter");*row=saved;
        strcpy(row->physicalCName,"void*");refused(m,"POD raw-pointer substitution");*row=saved;
        row->kind=XL6_FOREIGN_PACK_FIELD_VIEW_RAW;refused(m,"record as resource view");*row=saved;
        row->kind=XL6_FOREIGN_PACK_FIELD_SPAN_DATA;refused(m,"record as span");*row=saved;
        XL6_IrType original=*type;
        type->foreignHeader=NULL;refused(m,"POD missing header");*type=original;
        type->kind=XL6_IR_TYPE_UNION;refused(m,"POD union substitution");*type=original;
        type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;refused(m,"POD managed storage");*type=original;
        type->cAbi=XL6_IR_C_ABI_RAW_POINTER;refused(m,"POD pointer ABI");*type=original;
        const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(m,row->logicalType);assert(layout && layout->fieldCount);
        XL6_IrAggregateField* member=&m->aggregateFields[layout->firstField];XL6_IrTypeId memberType=member->type;member->type=0;refused(m,"nested field type");member->type=memberType;
    }
    assert(records==6 && xl6IrVerifyModule(m,NULL));printf("packed POD: %u value fields, %u mutations rejected with zero C/header output\n",records,mutations);
    xl6CompileUnitFree(&unit);return 0;
}

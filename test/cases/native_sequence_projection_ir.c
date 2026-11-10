#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(XL6_IrModule* module,const char* name)
{
    if(xl6IrVerifyModule(module,NULL)){
        fprintf(stderr,"accepted damaged record projection IR: %s\n",name);assert(0);
    }
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t count=SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_PROJECTION",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* value=xl6OutputTakeMemoryText(&out,&count);
        assert(value && !count);xrtFree(value);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1)){
        xl6DiagPrintTextWithPhase(&unit.parse.diags,argv[1],unit.phase);
        xl6CompileUnitFree(&unit);return 2;
    }
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* module=&unit.ir;
    assert(xl6IrVerifyModule(module,NULL));size_t records=0,nested=0;
    for(size_t n=0;n<module->types.count;++n){XL6_IrType* type=&module->types.items[n],old=*type;
        if(type->foreignRecordAbi!=XL6_IR_FOREIGN_RECORD_PROJECTED)continue;
        ++records;bool root=false;
        for(size_t p=0;p<module->foreignNativeSequenceCount;++p)
            if(module->foreignNativeSequences[p].elementType==n+1)root=true;
        if(!root)++nested;
        type->foreignRecordSource=NULL;refused(module,"missing backing identity, including nested records");*type=old;
        type->foreignRecordSource="invalid spelling*";refused(module,"invalid backing identity");*type=old;
        type->foreignRecordAbi=XL6_IR_FOREIGN_RECORD_NONE;refused(module,"missing layout discriminator");*type=old;
        type->foreignRecordAbi=XL6_IR_FOREIGN_RECORD_EXACT;refused(module,"projected backing on exact layout");*type=old;
        type->foreignRecordAbi=UINT8_MAX;refused(module,"unknown layout discriminator");*type=old;
        type->foreignHeader=NULL;refused(module,"missing native header proof");*type=old;
    }
    assert(records==3 && nested==1);
    bool unrelated=false;
    for(size_t n=0;n<module->types.count;++n){XL6_IrType* type=&module->types.items[n],old=*type;
        if(type->kind!=XL6_IR_TYPE_BUILTIN)continue;
        type->foreignRecordAbi=XL6_IR_FOREIGN_RECORD_EXACT;refused(module,"record layout on non-record");*type=old;
        type->foreignRecordSource="NativeProjectionInner";refused(module,"native backing on non-record");*type=old;
        unrelated=true;break;
    }
    assert(unrelated);
    for(size_t n=0;n<module->foreignNativeSequenceCount;++n){
        XL6_ForeignNativeSequenceContract* row=&module->foreignNativeSequences[n],old=*row;
        strcpy(row->dataCName,"const XL_Projection_Row*");refused(module,"logical layout used as native stride");*row=old;
        row->retainSymbol=0;refused(module,"missing const retaining helper");*row=old;
    }
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    printf("Read-only record projection IR: %u mutations, %zu records, %zu nested records\n",mutations,records,nested);
    return 0;
}

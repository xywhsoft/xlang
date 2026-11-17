#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"result snapshot mutation accepted: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_RESULT_SNAPSHOT",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88 && sizeof(XL6_ForeignDataSnapshotContract)==12);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    size_t count=ir->foreignDataSnapshotCount,capacity=ir->foreignDataSnapshotCapacity;
    XL6_ForeignDataSnapshotContract* rows=ir->foreignDataSnapshots;assert(count>=11);
    ir->foreignDataSnapshotCount=capacity+1;refused(ir,"count");ir->foreignDataSnapshotCount=count;
    ir->foreignDataSnapshots=NULL;refused(ir,"storage");ir->foreignDataSnapshots=rows;
    ir->foreignDataSnapshotCount=count-1;refused(ir,"lost row");ir->foreignDataSnapshotCount=count;
    for(size_t i=0;i<count;++i){
        XL6_ForeignDataSnapshotContract saved=rows[i];XL6_IrNode* node=&ir->nodes[saved.declaration-1];
        int result=(saved.parameter&XL6_FOREIGN_DATA_SNAPSHOT_RESULT)!=0;
        rows[i].declaration=0;refused(ir,"declaration");rows[i]=saved;
        rows[i].parameter=result ? XL6_FOREIGN_DATA_SNAPSHOT_RESULT|63:63;refused(ir,"range");rows[i]=saved;
        rows[i].parameter^=XL6_FOREIGN_DATA_SNAPSHOT_RESULT;refused(ir,"wrong direction");rows[i]=saved;
        rows[i].parameter|=UINT32_C(0x40000000);refused(ir,"unknown tag bits");rows[i]=saved;
        rows[i].logicalType=0;refused(ir,"logical type");rows[i]=saved;
        uint8 n=node->foreignCall.dataSnapshotCount;node->foreignCall.dataSnapshotCount=0;
        refused(ir,"closed count");node->foreignCall.dataSnapshotCount=n;
        uint16 flags=node->foreignCall.inputMappingFacts;
        node->foreignCall.inputMappingFacts&=~(result ? XL6_FOREIGN_RESULT_DATA_SNAPSHOTS_PRESENT:XL6_FOREIGN_INPUT_DATA_SNAPSHOTS_PRESENT);
        refused(ir,"closed presence");node->foreignCall.inputMappingFacts=flags;
        if(i+1<count){XL6_ForeignDataSnapshotContract other=rows[i+1];rows[i+1]=saved;
            refused(ir,"duplicate");rows[i+1]=other;}
        for(size_t t=0;t<ir->callTargetCount;++t){XL6_IrCallTarget* target=&ir->callTargets[t];
            if(target->declaration!=saved.declaration)continue;
            XL6_IrCallTarget original=*target;
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(ir,"raising admission");*target=original;
            target->errorPhysicalAbi=0;refused(ir,"foreign scope");*target=original;
            target->kind=XL6_IR_CALL_TARGET_INTERNAL;refused(ir,"native target kind");*target=original;
            target->runtimeBridge=(XL6_IrRuntimeBridgeKind)1;refused(ir,"runtime bridge");*target=original;
            if(result){
                target->returnType=0;refused(ir,"carrier type");*target=original;
                target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_BORROWED_VALUE_COPY;
                refused(ir,"borrowed result");*target=original;
            }
        }
    }
    size_t plans=ir->persistenceProjectPlanCount;assert(plans);
    ir->persistenceProjectPlanCount=0;refused(ir,"public projection plan");ir->persistenceProjectPlanCount=plans;
    assert(xl6IrVerifyModule(ir,NULL));xl6CompileUnitFree(&unit);
    printf("RESULT_SNAPSHOT_IR_MUTATIONS=%u FACT_BYTES=%zu ROW_BYTES=%zu\n",mutations,sizeof(XL6_ForeignCallFacts),sizeof(XL6_ForeignDataSnapshotContract));
    return 0;
}

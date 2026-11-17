#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"snapshot mutation accepted: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_SNAPSHOT",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    size_t count=ir->foreignDataSnapshotCount,capacity=ir->foreignDataSnapshotCapacity;
    XL6_ForeignDataSnapshotContract* rows=ir->foreignDataSnapshots;assert(count>=9);
    ir->foreignDataSnapshotCount=capacity+1;refused(ir,"count");ir->foreignDataSnapshotCount=count;
    ir->foreignDataSnapshots=NULL;refused(ir,"missing storage");ir->foreignDataSnapshots=rows;
    ir->foreignDataSnapshotCount=count-1;refused(ir,"lost row");ir->foreignDataSnapshotCount=count;
    for(size_t i=0;i<count;++i){
        XL6_ForeignDataSnapshotContract saved=rows[i];XL6_IrNode* node=&ir->nodes[saved.declaration-1];
        rows[i].declaration=0;refused(ir,"declaration");rows[i]=saved;
        rows[i].parameter=63;refused(ir,"parameter");rows[i]=saved;
        rows[i].logicalType=0;refused(ir,"logical type");rows[i]=saved;
        uint8 n=node->foreignCall.dataSnapshotCount;node->foreignCall.dataSnapshotCount=0;
        refused(ir,"closed count");node->foreignCall.dataSnapshotCount=n;
        uint16 flags=node->foreignCall.inputMappingFacts;
        node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_DATA_SNAPSHOTS_PRESENT;
        refused(ir,"closed presence");node->foreignCall.inputMappingFacts=flags;
        if(i+1<count){XL6_ForeignDataSnapshotContract other=rows[i+1];rows[i+1]=saved;
            refused(ir,"duplicate row");rows[i+1]=other;}
        for(size_t t=0;t<ir->callTargetCount;++t){XL6_IrCallTarget* target=&ir->callTargets[t];
            if(target->declaration!=saved.declaration)continue;
            XL6_IrParameterAbi abi=target->parameterAbis[saved.parameter];
            target->parameterAbis[saved.parameter]=XL6_IR_PARAMETER_ABI_OWNED;
            refused(ir,"owned input");target->parameterAbis[saved.parameter]=abi;
            XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[saved.parameter];
            target->parameterPhysicalAbis[saved.parameter]=XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK;
            refused(ir,"physical ABI");target->parameterPhysicalAbis[saved.parameter]=physical;
            XL6_IrErrorAbi effect=target->errorAbi;target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
            refused(ir,"missing raising admission");target->errorAbi=effect;
        }
    }
    size_t plans=ir->persistenceProjectPlanCount;assert(plans);
    ir->persistenceProjectPlanCount=0;refused(ir,"missing public projection plan");ir->persistenceProjectPlanCount=plans;
    assert(xl6IrVerifyModule(ir,NULL));xl6CompileUnitFree(&unit);
    printf("DATA_SNAPSHOT_IR_MUTATIONS=%u FACT_BYTES=%zu\n",mutations,sizeof(XL6_ForeignCallFacts));
    return 0;
}

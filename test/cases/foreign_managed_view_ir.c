#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output output;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_MANAGED_VIEW",NULL) : xl6IrCgenEmitC(&output,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&output,&bytes);assert(text && !bytes);
        xrtFree(text);xl6OutputDiscard(&output);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    unsigned inputs=0,results=0,globals=0,frames=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        XL6_IrNode* node=target->declaration ? &module->nodes[target->declaration-1] : NULL;
        for(size_t p=0;p<target->parameterCount;++p){
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)continue;
            ++inputs;XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);target->parameterPhysicalAbis[p]=physical;
            XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(module);target->parameterAbis[p]=abi;
            XL6_IrCallTarget saved=*target;target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);*target=saved;
        }
        if(target->resultConversionAbi!=XL6_IR_CALL_RESULT_CONVERSION_NATIVE_VIEW_ADMIT)continue;
        ++results;assert(node);XL6_IrCallTarget saved=*target;XL6_ForeignCallFacts facts=node->foreignCall;
        target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(module);*target=saved;
        target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_STORAGE;refused(module);*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module);*target=saved;
        node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_NONE;refused(module);node->foreignCall=facts;
        if(facts.outputIndexPlusOne){
            node->foreignCall.rollbackSymbol=0;refused(module);node->foreignCall=facts;
            node->foreignCall.rollbackSymbol=target->symbol;refused(module);node->foreignCall=facts;
        }
        if(facts.resultOwnerIndexPlusOne){
            node->foreignCall.resultOwnerIndexPlusOne=64;refused(module);node->foreignCall=facts;
            node->foreignCall.controlViews|=UINT64_C(1)<<(facts.resultOwnerIndexPlusOne-1);refused(module);node->foreignCall=facts;
        }
    }
    for(size_t i=0;i<module->types.count;++i){
        XL6_IrType* type=&module->types.items[i];if(!type->foreignPolicy)continue;
        XL6_IrType saved=*type;
        type->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(module);*type=saved;
        type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;refused(module);*type=saved;
        type->nativeValueAbi=XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1;refused(module);*type=saved;
        type->cAbi=XL6_IR_C_ABI_NATIVE_HANDLE;refused(module);*type=saved;
        if(type->foreignOwner){type->foreignOwner=(XL6_IrTypeId)(i+1);refused(module);*type=saved;}
    }
    for(size_t i=0;i<module->storageCount;++i){
        XL6_IrStorage* storage=&module->storages[i];if(storage->moduleTraceAbi!=XL6_IR_FRAME_TRACE_NATIVE_GRAPH)continue;
        ++globals;storage->moduleTraceAbi=XL6_IR_FRAME_TRACE_NONE;refused(module);storage->moduleTraceAbi=XL6_IR_FRAME_TRACE_NATIVE_GRAPH;
    }
    for(size_t i=0;i<module->asyncMachineFrameSlotCount;++i){
        XL6_IrContinuationFrameSlot* slot=&module->asyncMachineFrameSlots[i];if(slot->traceAbi!=XL6_IR_FRAME_TRACE_NATIVE_GRAPH)continue;
        ++frames;slot->traceAbi=XL6_IR_FRAME_TRACE_NONE;refused(module);slot->traceAbi=XL6_IR_FRAME_TRACE_NATIVE_GRAPH;
    }
    assert(module->abi.closureStorageAbi==XL6_IR_CLOSURE_STORAGE_RESIDENT_GRAPH_LAYOUT_V4);
    module->abi.closureStorageAbi=XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3;refused(module);
    module->abi.closureStorageAbi=XL6_IR_CLOSURE_STORAGE_RESIDENT_GRAPH_LAYOUT_V4;
    assert(module->abi.cellStorageAbi==XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_GRAPH_V3);
    module->abi.cellStorageAbi=XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_LEAF_V2;refused(module);
    module->abi.cellStorageAbi=XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_GRAPH_V3;
    assert(inputs && results>=3 && globals>=2 && frames && xl6IrVerifyModule(module,NULL));
    printf("managed view: inputs=%u results=%u globals=%u frames=%u mutations=%u zero-output=%u\n",
        inputs,results,globals,frames,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

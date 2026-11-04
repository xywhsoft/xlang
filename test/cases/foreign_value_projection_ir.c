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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_POINTEE_VALUE",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    assert(module->foreignValueCount==5 && sizeof(XL6_ForeignCallFacts)==88);
    for(size_t i=0;i<module->foreignValueCount;++i){
        XL6_ForeignValueContract* row=&module->foreignValues[i];XL6_ForeignValueContract saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];
        XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(module,node->symbol);
        assert(target && xl6IrForeignValueMatches(module,target,row->parameter));
        row->declaration=0;refused(module);*row=saved;
        row->declaration=UINT32_MAX;refused(module);*row=saved;
        row->parameter=64;refused(module);*row=saved;
        row->parameter=(uint32)target->parameterCount;refused(module);*row=saved;
        row->cName[0]='-';refused(module);*row=saved;
        memset(row->cName,'a',sizeof(row->cName));refused(module);*row=saved;
        size_t p=row->parameter;
        XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
        target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module);
        target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW;refused(module);
        target->parameterPhysicalAbis[p]=physical;
        XL6_IrParameterAbi abi=target->parameterAbis[p];
        target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(module);target->parameterAbis[p]=abi;
        XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[p];
        target->parameterPhysicalFieldDeclarations[p]=target->declaration;refused(module);target->parameterPhysicalFieldDeclarations[p]=field;
        XL6_IrCallTargetKind kind=target->kind;target->kind=XL6_IR_CALL_TARGET_INTERNAL;refused(module);target->kind=kind;
        XL6_IrForeignOperationAbi operation=target->foreignOperationAbi;
        target->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_DEREF_READ;refused(module);target->foreignOperationAbi=operation;
        uint64 control=node->foreignCall.controlViews;node->foreignCall.controlViews|=UINT64_C(1)<<p;refused(module);node->foreignCall.controlViews=control;
        XL6_IrTypeId* types=target->parameterTypes;target->parameterTypes=NULL;refused(module);target->parameterTypes=types;
        XL6_IrParameterAbi* abis=target->parameterAbis;target->parameterAbis=NULL;refused(module);target->parameterAbis=abis;
        XL6_IrCallParameterPhysicalAbi* physicals=target->parameterPhysicalAbis;target->parameterPhysicalAbis=NULL;refused(module);target->parameterPhysicalAbis=physicals;
        XL6_IrCallParameterConstraintAbi* constraints=target->parameterConstraintAbis;target->parameterConstraintAbis=NULL;refused(module);target->parameterConstraintAbis=constraints;
        XL6_IrNodeId* fields=target->parameterPhysicalFieldDeclarations;target->parameterPhysicalFieldDeclarations=NULL;refused(module);target->parameterPhysicalFieldDeclarations=fields;
    }
    size_t count=module->foreignValueCount,capacity=module->foreignValueCapacity;
    module->foreignValueCount=0;refused(module);module->foreignValueCount=count;
    module->foreignValueCapacity=0;refused(module);module->foreignValueCapacity=capacity;
    XL6_ForeignValueContract* original=module->foreignValues;
    module->foreignValues=NULL;refused(module);module->foreignValues=original;
    XL6_IrCallTarget* targets=module->callTargets;module->callTargets=NULL;refused(module);module->callTargets=targets;
    XL6_ForeignValueContract* duplicate=xrtCalloc(count+1,sizeof(*duplicate));assert(duplicate);
    memcpy(duplicate,original,count*sizeof(*original));duplicate[count]=duplicate[0];
    module->foreignValues=duplicate;module->foreignValueCount=count+1;module->foreignValueCapacity=count+1;refused(module);
    module->foreignValues=original;module->foreignValueCount=count;module->foreignValueCapacity=capacity;xrtFree(duplicate);
    assert(xl6IrVerifyModule(module,NULL));
    printf("pointee values: declarations=%zu mutations=%u zero-output=%u\n",count,mutations,mutations*2);
    xl6CompileUnitFree(&unit);return 0;
}

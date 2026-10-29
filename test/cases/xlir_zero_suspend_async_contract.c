#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t mutations;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_ZERO_SUSPENSION",&errors):xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));size_t queued=0,immediate=0;
    assert(module->abi.taskPoolAbi==XL6_IR_MODULE_TASK_POOL_NONE &&
        module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=module->callTargets+i;
        if(target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC)continue;
        if(target->asyncDispatch==XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE){
            assert(target->asyncMachine==SIZE_MAX);++immediate;continue;
        }
        assert(target->asyncDispatch==XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE && target->asyncMachine<module->asyncMachineCount);
        XL6_IrAsyncMachine* machine=module->asyncMachines+target->asyncMachine;
        assert(!machine->suspensionCount&&machine->entryCount==1&&machine->blockCount&&
            machine->executionAbi==XL6_IR_ASYNC_EXECUTION_READY_QUEUE_ROOT_V2);
        for(size_t p=0;p<target->parameterCount;++p){
            size_t found=0;
            for(size_t f=0;f<machine->frameSlotCount;++f){
                XL6_IrAsyncMachineFrameSlot* frame=module->asyncMachineFrameSlots+machine->firstFrameSlot+f;
                if(frame->residence==XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER&&frame->parameter==p)++found;
            }
            assert(found==1);
        }
        size_t link=target->asyncMachine;target->asyncMachine=SIZE_MAX;reject(module);target->asyncMachine=link;
        size_t entries=machine->entryCount;machine->entryCount=0;reject(module);machine->entryCount=entries;
        XL6_IrAsyncDispatchKind dispatch=target->asyncDispatch;
        target->asyncDispatch=XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE;reject(module);target->asyncDispatch=dispatch;
        ++queued;
    }
    assert(queued>=4&&immediate>=1&&xl6IrVerifyModule(module,NULL));
    XL6_IrModuleTaskPoolAbi pool=module->abi.taskPoolAbi;
    module->abi.taskPoolAbi=XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE;reject(module);module->abi.taskPoolAbi=pool;
    printf("zero-suspension async: %zu queued programs, %zu immediate, %zu zero-C/header refusals; no implicit task pool\n",queued,immediate,mutations);
    xl6CompileUnitFree(&unit);return 0;
}

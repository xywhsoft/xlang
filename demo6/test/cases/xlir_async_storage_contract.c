#include "xlir_continuation_owner_test.h"
/* Shared sparse activation layout assertions are below the compiler headers. */
#include "xlir_activation_capture_assert.h"
int main(int argc,char** argv)
{
    const char* fixtures[]={"test/cases/xlir_async_storage.xl","test/cases/xlir_frame_ownership.xl",
        "test/cases/xlir_cell_continuation_outcome.xl","test/cases/xlir_async_pool_launch_allocation.xl",
        "test/cases/xlir_async_launch_allocation.xl"};
    size_t machines=0,pools=0,ready=0;
    assert(argc==4);
    for(size_t n=0;n<5;++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"async_storage") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        verify(&ir); assert(ir.abi.asyncStorageAbi==XL6_IR_ASYNC_STORAGE_NONE);
        ir.abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_RESIDENT_PHYSICAL_FRAME_V1; reject(&ir,"async physical storage ABI");
        ir.abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V2; reject(&ir,"async physical storage ABI");
        ir.abi.asyncStorageAbi=(XL6_IrAsyncStorageAbi)99; reject(&ir,"async physical storage ABI");
        ir.abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_NONE; assert(xl6IrLowerModule(&ir,&parse.diags)); verify(&ir);
        ir.abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_NONE; reject(&ir,"async physical storage ABI");
        ir.abi.asyncStorageAbi=(XL6_IrAsyncStorageAbi)99; reject(&ir,"async physical storage ABI");
        ir.abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_RESIDENT_PHYSICAL_FRAME_V1; reject(&ir,"async physical storage ABI");
        ir.abi.asyncStorageAbi=XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V3; verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags)); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && !strstr(text,"xrtFree(frame);") && !strstr(text,"xrtFree(next);"));
        if(ir.asyncMachineCount) {
            for(size_t m=0;m<ir.asyncMachineCount;++m){const XL6_IrAsyncMachine* machine=&ir.asyncMachines[m];
                assert_activation_layout(text,"xlir_async_machine",machine->targetSymbol,
                    machine->frameSlotCount?ir.asyncMachineFrameSlots+machine->firstFrameSlot:NULL,machine->frameSlotCount,machine->firstFrameSlot,
                    machine->destroyActionCount?ir.asyncFrameActions+machine->firstDestroyAction:NULL,machine->destroyActionCount);}
            assert(strstr(text,"__xl_runtime/xl6_continuation_future.h") && strstr(text,"xl6ContinuationFrameRuntimeV2Get()->Discard") &&
                strstr(text,"xlir_async_frame_allocate(sizeof(*frame), &xlir_async_machine_storage_") && strstr(text,"xlir_async_frame_dispose(data);"));
            if(ir.asyncMachineCount) { ++machines; assert(strstr(text,"xlir_async_frame_allocate(sizeof(*next), &xlir_async_machine_storage_") &&
                strstr(text,"xlir_async_frame_dispose(next);")); }
        } else if(n==3) {
            ++pools; assert(strstr(text,"__xl_runtime/xl6_task.h") && strstr(text,"xl6TaskRuntimeV2Get()->Allocate") &&
                strstr(text,"XL6_CaptureLayoutV1") && strstr(text,"(xl6TaskRuntimeV2Get()->Cleanup)(frame)") &&
                strstr(text,"xlir_task_allocate(sizeof(*frame),") && strstr(text,"xlir_task_discard(data);"));
        } else ++ready;
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"async-storage: 3")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"asyncStorageAbi\": 3")); xrtFree(text);
        if(n==0) {
            assert(xl6OutputOpenFile(&out,argv[1]) && xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[2])); size_t bindings=0;
            for(size_t i=0;i<ir.functionCount;++i) {
                const XL6_IrFunction* fn=&ir.functions[i]; const XL6_IrNode* decl=xl6IrNodeGet(&ir,fn->declaration);
                const XL6_IrNode* name=decl && decl->childCount?xl6IrNodeGet(&ir,ir.children[decl->firstChild]):NULL;
                if(name && name->text && !strcmp(name->text,"asyncStorageProbe")) {
                    xl6OutputPrintf(&out,"#define ASYNC_STORAGE_START xlir_async_start_raw_%u\n",fn->symbol); ++bindings;
                }
            }
            assert(bindings==1); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[3])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
        }
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations==30 && machines==3 && pools==1 && ready==1);
    puts("Async storage IR: 5 fixtures, 30 rejected C/header-gated mutations, 3 machine fixtures, 1 pool frame, 1 frame-free ready future; explicit text/JSON ABI");
    return 0;
}

#include "xlir_continuation_owner_test.h"
int main(int argc,char** argv)
{
    const char* fixtures[]={"xlir_callback_boundary","xlir_callback_boundary_future_only","stdlib_future_continuation"};
    size_t futures=0,tasks=0;
    assert(argc==4);
    for(size_t n=0;n<3;++n){
        XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char path[256],*text;
        snprintf(path,sizeof(path),"test/cases/%s.xl",fixtures[n]);xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
        assert(xl6ParseFile(path,&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"future_callback_storage") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        verify(&ir);assert(ir.callbackBoundaryCount==0 && xl6IrLowerModule(&ir,&parse.diags));verify(&ir);
        for(size_t i=0;i<ir.callbackBoundaryCount;++i){
            XL6_IrCallbackBoundary* b=&ir.callbackBoundaries[i];XL6_IrCallbackStorageAbi saved=b->storageAbi;
            assert(saved==(b->transportAbi==XL6_IR_CALLBACK_TRANSPORT_TASK_POOL
                ? XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3:XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1));
            if(saved==XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3)++tasks;else ++futures;
            b->storageAbi=XL6_IR_CALLBACK_STORAGE_INVALID;reject(&ir,"callback");
            b->storageAbi=(XL6_IrCallbackStorageAbi)99;reject(&ir,"callback");
            b->storageAbi=saved==XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3
                ? XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1:XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3;
            reject(&ir,"callback");
            b->storageAbi=XL6_IR_CALLBACK_STORAGE_RESIDENT_TASK_FRAME_V1;reject(&ir,"callback");
            b->storageAbi=XL6_IR_CALLBACK_STORAGE_RESIDENT_FUTURE_FRAME_V1;reject(&ir,"callback");
            b->storageAbi=XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V2;reject(&ir,"callback");
            b->storageAbi=XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_FUTURE_FRAME_V2;reject(&ir,"callback");b->storageAbi=saved;
            if(saved==XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1){
                XL6_IrCallbackBoundary sealed=*b;
                b->storageAbi=XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_FUTURE_FRAME_V3;reject(&ir,"callback");*b=sealed;
                b->transportAbi=XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH;reject(&ir,"callback");*b=sealed;
                b->cleanupAbi=XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL;reject(&ir,"callback");*b=sealed;
                b->errorAbi=XL6_IR_CALLBACK_ERROR_PRESERVE;reject(&ir,"callback");*b=sealed;
            }
        }
        verify(&ir);assert(xl6OutputOpenMemory(&out));
        if(!xl6IrCgenEmitC(&out,&ir,&parse.diags)){xl6DiagPrintText(&parse.diags,path);assert(0);}
        text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"__xl_runtime/xl6_future_callback.h") && strstr(text,"xl6FutureCallbackRuntimeV5Get()->Start(xlir_continuation_executor,") &&
            strstr(text,"xlir_future_callback_allocate(sizeof(*frame),") && strstr(text,"xlir_future_callback_discard(data)") &&
            strstr(text,"XL6_CaptureLayoutV2") && strstr(text,"XL6_CaptureSlotV2") &&
            strstr(text,"xl6FutureCallbackRuntimeV5Get") &&
            strstr(text,"xerror* callbackError = xrtTakeError();") &&
            strstr(text,"xl6_error_result outcome = { ok && !callbackError, callbackError };") &&
            !strstr(text,"{ ok && !xrtGetError(), xrtTakeError() }") &&
            !strstr(text,"static void xlir_future_continue_frame_drop_") &&
            !strstr(text,"static bool xlir_future_continue_frame_trace_") &&
            !strstr(text,"static void xlir_task_spawn_frame_drop_") &&
            !strstr(text,"static bool xlir_task_spawn_frame_trace_") && !strstr(text,"xlir_future_continue_notify_"));
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleText(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"transport=3 storage=7"));xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleJson(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"storageAbi\":7"));xrtFree(text);
        if(n==2){
            assert(xl6OutputOpenFile(&out,argv[1]) && xl6IrCgenEmitC(&out,&ir,&parse.diags));xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[2]) && xl6IrCgenEmitHeader(&out,&ir,"FUTURE_CALLBACK_STORAGE",&parse.diags));xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[3]));xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);
        }
        xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    }
    /* Four + one + ten Future sites; one + zero + nine task.spawn sites.
     * Every boundary, including the language fixture's producers, is mutated. */
    assert(futures==15 && tasks==10 && mutations==235);
    puts("Future callback storage IR: 3 fixtures, 15 Future continuations, 10 tasks, 235 rejected C/header-gated mutations including retired inline transport/cleanup/error ABIs; exact text/JSON ABI");return 0;
}

#include "xlir_continuation_owner_test.h"
int main(int argc,char** argv)
{
    const char* fixtures[]={"xlir_async_pool_launch_allocation","xlir_callback_boundary","xlir_callback_boundary_future_only",
        "xlir_module_task_pool","xlir_frame_ownership","xlir_generator_ownership","xlir_async_pool_outcome","xlir_async_launch_allocation"};
    size_t parameters=0,captures=0,pools=0;
    assert(argc==4);
    for(size_t n=0;n<8;++n){
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char path[256],*text;
        snprintf(path,sizeof(path),"test/cases/%s.xl",fixtures[n]);
        xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
        assert(xl6ParseFile(path,&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"task_storage") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        verify(&ir);assert(ir.abi.taskStorageAbi==XL6_IR_TASK_STORAGE_NONE);
        assert(ir.abi.valueIteratorStorageAbi==XL6_IR_VALUE_ITERATOR_STORAGE_NONE);
        ir.abi.valueIteratorStorageAbi=XL6_IR_VALUE_ITERATOR_STORAGE_RESIDENT_SNAPSHOT_V1;reject(&ir,"value iterator storage ABI");
        ir.abi.valueIteratorStorageAbi=(XL6_IrValueIteratorStorageAbi)99;reject(&ir,"value iterator storage ABI");
        ir.abi.valueIteratorStorageAbi=XL6_IR_VALUE_ITERATOR_STORAGE_NONE;
        ir.abi.taskStorageAbi=XL6_IR_TASK_STORAGE_RESIDENT_FRAME_POOL_V1;reject(&ir,"task frame/pool storage ABI");
        ir.abi.taskStorageAbi=XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V2;reject(&ir,"task frame/pool storage ABI");
        ir.abi.taskStorageAbi=(XL6_IrTaskStorageAbi)99;reject(&ir,"task frame/pool storage ABI");
        ir.abi.taskStorageAbi=XL6_IR_TASK_STORAGE_NONE;assert(xl6IrLowerModule(&ir,&parse.diags));verify(&ir);
        ir.abi.taskStorageAbi=XL6_IR_TASK_STORAGE_NONE;reject(&ir,"task frame/pool storage ABI");
        ir.abi.taskStorageAbi=XL6_IR_TASK_STORAGE_RESIDENT_FRAME_POOL_V1;reject(&ir,"task frame/pool storage ABI");
        ir.abi.taskStorageAbi=(XL6_IrTaskStorageAbi)99;reject(&ir,"task frame/pool storage ABI");
        ir.abi.taskStorageAbi=XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V2;
        ir.abi.valueIteratorStorageAbi=XL6_IR_VALUE_ITERATOR_STORAGE_NONE;reject(&ir,"value iterator storage ABI");
        ir.abi.valueIteratorStorageAbi=(XL6_IrValueIteratorStorageAbi)99;reject(&ir,"value iterator storage ABI");
        ir.abi.valueIteratorStorageAbi=XL6_IR_VALUE_ITERATOR_STORAGE_RESIDENT_SNAPSHOT_V1;
        for(size_t i=0;i<ir.callTargetCount;++i){
            XL6_IrCallTarget* t=&ir.callTargets[i];
            if(t->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC || !t->parameterCount){
                XL6_IrContinuationFrameTraceAbi invalid=XL6_IR_FRAME_TRACE_NONE;
                assert(!t->asyncCaptureTraceAbis);t->asyncCaptureTraceAbis=&invalid;
                reject(&ir,"stray async capture trace ABI");t->asyncCaptureTraceAbis=NULL;
                XL6_IrAsyncFrameDestroyAbi stray=XL6_IR_ASYNC_FRAME_DESTROY_NONE;
                assert(!t->asyncCaptureDestroyAbis);t->asyncCaptureDestroyAbis=&stray;
                reject(&ir,"stray async capture destroy ABI");t->asyncCaptureDestroyAbis=NULL;continue;
            }
            XL6_IrContinuationFrameTraceAbi* vector=t->asyncCaptureTraceAbis;
            t->asyncCaptureTraceAbis=NULL;reject(&ir,"complete canonical ABI");t->asyncCaptureTraceAbis=vector;
            XL6_IrAsyncFrameDestroyAbi* destroy=t->asyncCaptureDestroyAbis;
            t->asyncCaptureDestroyAbis=NULL;reject(&ir,"complete canonical ABI");t->asyncCaptureDestroyAbis=destroy;
            for(size_t p=0;p<t->parameterCount;++p){
                XL6_IrContinuationFrameTraceAbi saved=vector[p];
                vector[p]=XL6_IR_FRAME_TRACE_INVALID;reject(&ir,"exact trace ABI");
                vector[p]=(XL6_IrContinuationFrameTraceAbi)99;reject(&ir,"exact trace ABI");
                vector[p]=saved==XL6_IR_FRAME_TRACE_NONE?XL6_IR_FRAME_TRACE_XVALUE:XL6_IR_FRAME_TRACE_NONE;
                reject(&ir,"exact trace ABI");vector[p]=saved;++parameters;
                XL6_IrAsyncFrameDestroyAbi drop=destroy[p];
                destroy[p]=XL6_IR_ASYNC_FRAME_DESTROY_INVALID;reject(&ir,"exact destroy ABI");
                destroy[p]=(XL6_IrAsyncFrameDestroyAbi)99;reject(&ir,"exact destroy ABI");
                destroy[p]=drop==XL6_IR_ASYNC_FRAME_DESTROY_NONE?XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE:XL6_IR_ASYNC_FRAME_DESTROY_NONE;
                reject(&ir,"exact destroy ABI");destroy[p]=drop;
            }
        }
        for(size_t i=0;i<ir.callbackCaptureCount;++i){
            XL6_IrCallbackCapture* c=&ir.callbackCaptures[i];XL6_IrContinuationFrameTraceAbi saved=c->traceAbi;
            c->traceAbi=XL6_IR_FRAME_TRACE_INVALID;reject(&ir,"capture contract is malformed");
            c->traceAbi=(XL6_IrContinuationFrameTraceAbi)99;reject(&ir,"capture contract is malformed");
            c->traceAbi=saved==XL6_IR_FRAME_TRACE_NONE?XL6_IR_FRAME_TRACE_CLOSURE:XL6_IR_FRAME_TRACE_NONE;
            reject(&ir,"capture contract is malformed");c->traceAbi=saved;++captures;
        }
        verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && !strstr(text,"xlir_task_outcome_watch") && !strstr(text,"xlir_async_pool_gate") &&
            !strstr(text,"static xtaskoutcome ") && !strstr(text,"xrtTaskSubmit(") &&
            !strstr(text,"static bool xlir_async_frame_trace_") && !strstr(text,"static void xlir_async_frame_drop_") &&
            !strstr(text,"static const xrtownershipops xlir_xvalue_iterator_ownership_ops"));
        if(ir.abi.taskPoolAbi!=XL6_IR_MODULE_TASK_POOL_NONE){
            ++pools;assert(strstr(text,"__xl_runtime/xl6_task.h") && strstr(text,"xl6TaskRuntimeV1Get()->PoolQuiesce") &&
                ((strstr(text,"xlir_task_allocate(sizeof(*frame),") && strstr(text,"&xlir_async_layout_") &&
                  strstr(text,"(xl6TaskRuntimeV2Get()->Cleanup)(frame)") && strstr(text,"(xl6TaskRuntimeV2Get()->Allocate)(xlir_task_owner")) ||
                 (strstr(text,"(xl6TaskRuntimeV2Get()->Allocate)(xlir_task_owner,") &&
                  strstr(text,"XL6_CaptureLayoutV1") && strstr(text,"XL6_CaptureSlot"))));
        }
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleText(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"task-storage: 2") && strstr(text,"capture-traces=[") &&
            strstr(text,"capture-destroys=[") && strstr(text,"value-iterator-storage: 1"));xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleJson(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"asyncCaptureDestroyAbis\":[") && strstr(text,"\"valueIteratorStorageAbi\": 1"));xrtFree(text);
        if(n==0){
            assert(xl6OutputOpenFile(&out,argv[1]) && xl6IrCgenEmitC(&out,&ir,&parse.diags));xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[2]) && xl6IrCgenEmitHeader(&out,&ir,"TASK_STORAGE",&parse.diags));xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[3]));xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);
        }
        xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    }
    assert(parameters && captures && pools && mutations>32);
    printf("Task storage IR: 8 fixtures, %llu rejected C/header-gated mutations, %llu async parameter traces, %llu callback capture traces, %llu task pool modules; exact storage/text/JSON ABI\n",
        (unsigned long long)mutations,(unsigned long long)parameters,(unsigned long long)captures,(unsigned long long)pools);
    return 0;
}

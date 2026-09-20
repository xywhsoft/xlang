#include "xlir_continuation_owner_test.h"
int main(int argc,char** argv)
{
    XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char path[4096],marker[160];char* generated;XL6_IrCallTarget* target=NULL;
    assert(argc==4);xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    if(!xl6ParseFile("test/cases/xlir_task_closed_capture.xl",&parse)||!xl6StdlibExpandModule(parse.root,&parse.diags)||
        !xl6SemCheckModule(parse.root,&parse.diags,"closed_capture")||!xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags)||
        !xl6IrVerifyModule(&ir,&parse.diags)||!xl6IrLowerModule(&ir,&parse.diags)||!xl6IrVerifyModule(&ir,&parse.diags)){
        xl6DiagPrintText(&parse.diags,"closed capture");return 1;}
    for(size_t i=0;i<ir.callTargetCount;++i)if(ir.callTargets[i].kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC){assert(!target);target=&ir.callTargets[i];}
    assert(target&&target->parameterCount==6&&target->asyncMachine==SIZE_MAX&&target->asyncDispatch==XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL);
    const XL6_IrAsyncFrameDestroyAbi expected[]={XL6_IR_ASYNC_FRAME_DESTROY_NONE,XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING,
        XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE,XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE,
        XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE,XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR};
    for(size_t p=0;p<6;++p){assert(target->asyncCaptureDestroyAbis[p]==expected[p]);
        target->asyncCaptureDestroyAbis[p]=XL6_IR_ASYNC_FRAME_DESTROY_INVALID;reject(&ir,"exact destroy ABI");
        target->asyncCaptureDestroyAbis[p]=expected[p];}
    verify(&ir);
    assert(xl6OutputOpenMemory(&out)&&xl6IrCgenEmitC(&out,&ir,&parse.diags));
    generated=xl6OutputTakeMemoryText(&out,NULL);assert(generated);
    assert(snprintf(marker,sizeof(marker),"static XL6_TaskOutcome xlir_async_task_%u(",(unsigned)target->symbol)>0);
    assert(strstr(generated,marker));
    assert(snprintf(marker,sizeof(marker),"&xlir_async_layout_%u",(unsigned)target->symbol)>0);
    assert(strstr(generated,marker)&&strstr(generated,"xl6TaskRuntimeV2Get()->Allocate")&&
        strstr(generated,"xl6TaskRuntimeV2Get()->Cleanup")&&!strstr(generated,"xlir_async_frame_drop_")&&
        !strstr(generated,"xlir_async_frame_trace_"));
    assert(xl6OutputOpenFile(&out,argv[1]));xl6OutputPrintf(&out,"%s",generated);xl6OutputClose(&out);xrtFree(generated);
    assert(xl6OutputOpenFile(&out,argv[2])&&xl6IrCgenEmitHeader(&out,&ir,"CLOSED_CAPTURE",&parse.diags));xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3]));xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);
    assert(snprintf(path,sizeof(path),"%s.bind.h",argv[2])>0&&xl6OutputOpenFile(&out,path));
    xl6OutputPrintf(&out,"#define CLOSED_CAPTURE_START xlir_async_start_%u\n#define CLOSED_CAPTURE_LAYOUT xlir_async_layout_%u\n",(unsigned)target->symbol,(unsigned)target->symbol);
    xl6OutputClose(&out);xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    printf("Closed async capture IR: 6 exact physical captures, no state-machine fallback, 6 C/header-gated destroy mutations\n");return 0;
}

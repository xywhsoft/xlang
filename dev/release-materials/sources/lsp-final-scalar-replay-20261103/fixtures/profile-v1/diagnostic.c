#include "profile.h"
    #include <stdio.h>
    #include <string.h>
    typedef struct{double ms;unsigned long long calls;} lifecycle_cell;
    static _Thread_local lifecycle_cell frames[8][128];
    static _Thread_local size_t depth;
    /* OS thread IDs can be reused after a worker exits. A process-wide
     * sequence distinguishes later lifetimes without allocating TLS records. */
    static xatomic64 sequence;
    static const char* const labels[]={"session.xlsSemanticCheckInputUsage","session.xlsSemanticCheckCreateBatch","session.xlsSyntaxBuild","session.xlsSemanticCheckValidate","session.xlsInputBatchCreateInContext","session.total","import.xlsRawBodyRegisterSyntax","import.captureSyntax","import.xlsSyntaxAdoptOrdinary","import.xlsTextCreate","import.capture","import.xl6ParseSourceWithCompletion","import.xlsRawBodyCopyParse","capture.xlsAstClone","syntax.decompose","syntax.xl6ParseSource","syntax.xl6ParseSourceWithCompletion","syntax.xlsAstSetSourceTreeSized","adopt.cacheTreeBytes","adopt.decompose","body.setup","body.parse","body.import","body.stdlib","body.semantic","body.index","body.ir-and-finish","ir.xl6IrVerifyModule","ir.xl6IrRefreshExitPlans","ir.xl6IrBuildImplicitConversionAbis","ir.xl6IrRefreshLoopAbis","ir.xl6IrBuildIndexWriteAbis","ir.xl6IrBuildDynamicCalls","ir.xl6IrBuildMemberAbis","ir.xl6IrBuildCastAbis","ir.xl6IrBuildOperatorErrorAbis","ir.xl6IrBuildNativeHandleProjects","ir.xl6IrBuildNativeHandleBinds","ir.xl6IrBuildValueInitializers","ir.xl6IrBuildCallSites","ir.xl6IrRefreshDynamicMethodAdmissions","ir.xl6IrBuildIntrinsicCalls","ir.xl6IrBuildDynamicMethodRegistry","ir.xl6IrNormalizeExpressionCallKinds","ir.xl6IrRefreshNominalIdentitySets","ir.xl6IrBuildProtocolDispatchTables","ir.xl6IrBuildClassDispatchTables","ir.xl6IrBuildImplicitNameAbis","ir.xl6IrBuildClassStaticFields","ir.xl6IrRefreshClassLayoutsDetailed","ir.xl6IrBuildEnumLayouts","ir.xl6IrBuildUnionLayouts","ir.xl6IrBuildOwnedReadResults","ir.xl6IrTypePoolFinalizeCallableResults","ir.xl6IrInternProjectionDomains","ir.xl6IrBuildNamedCallableAbis","ir.xl6IrInternAsyncInvocationTypes","ir.xl6IrBuildForeignClassBases","ir.xl6IrBuildRuntimeTypeMap","ir.xl6IrInternAstType","ir.xl6IrInternDeclaredClassTypes","ir.xl6IrBuildAstNode","adapter.validateGraph","adapter.xlsSemanticCheckValidate","adapter.xideSemanticDefinitionsAcquireTyped","adapter.buildDiagnostics","adapter.loadGraph","adapter.xrtJsonParse","adapter.xrtJsonRead","adapter.nativeAnalysis","adapter.total","worker.total","destroy.xl6SourceReportFree","destroy.xlsInputBatchDestroy","destroy.xlsFrontendOverlayClear","destroy.xl6ParseResultFree","destroy.xlsTypedModuleRelease","destroy.xl6IrModuleFree","destroy.xlsFrontendIrResultDestroy","destroy.xlsModuleGraphRelease","destroy.xlsInputTopologyDestroy","destroy.xlsFrontendSemMemoDestroy","destroy.xlsFrontendIndexDestroy","destroy.total","projection.valueDestroy","navigation.destroy","navigation.xlsProjectionPublish","navigation.xideSemanticDefinitionsCreateTyped","navigation.xlsProjectionAcquire","navigation.navigationKey","retire.xrtValueRelease","retire.xideSemanticDefinitionsReleaseTyped","retire.xlsContextRelease","retire.xlsTextRelease","retire.WaitForSingleObject","retire.total","adapter.xlsQueryDestroy"};
    void xlsLifecycleBegin(void){if(depth<8)memset(frames[depth],0,sizeof(frames[depth]));++depth;}
    void xlsLifecycleAdd(size_t at,double begin){if(depth && depth<=8 && at<128){lifecycle_cell* cell=&frames[depth-1][at];cell->ms+=(xrtTimer()-begin)*1000;++cell->calls;}}
    void xlsLifecycleStep(size_t at,double* begin){xlsLifecycleAdd(at,*begin);*begin=xrtTimer();}
    void xlsLifecycleEmit(const char* group){
     char record[32768];size_t at=0;
     if(!depth || depth>8){if(depth)--depth;fprintf(stderr,"XLS_LIFECYCLE_ERROR depth\n");return;}
     const lifecycle_cell* cells=frames[--depth];
     int written=snprintf(record,sizeof(record),"XLS_LIFECYCLE {\"group\":\"%s\",\"thread\":%llu,\"sequence\":%llu,\"cells\":{",group,(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtAtomic64FetchAdd(&sequence,1,XMEMORY_RELAXED)+1);
     if(written<0 || (size_t)written>=sizeof(record)){return;}
     at=(size_t)written;int comma=0;
     for(size_t i=0;i<sizeof(labels)/sizeof(*labels);++i)if(cells[i].calls){
      written=snprintf(record+at,sizeof(record)-at,"%s\"%s\":{\"ms\":%.9f,\"calls\":%llu}",comma ? "," : "",labels[i],cells[i].ms,cells[i].calls);
      if(written<0 || (size_t)written>=sizeof(record)-at){return;}
      at+=(size_t)written;comma=1;
     }
     if(sizeof(record)-at<4){return;}
     memcpy(record+at,"}}\n",3);at+=3;fwrite(record,1,at,stderr);
    }
    
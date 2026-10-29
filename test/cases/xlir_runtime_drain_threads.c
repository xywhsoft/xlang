/* Real public module retirement, parked in a valid checked preflight.
 * No network, fake module records, sleeps or replacement runtime providers. */
#include "release/include/xl_clock.h"
#include "xl.h"
#include "xruntime_decl.h"
#include <assert.h>
#include <stdio.h>

static xatomic32 entered, release_preflight, worker_done;
static XL_Module* retiring;
static unsigned reentries;
static unsigned selected_phase;
static bool armed;
static int stage(void) {return selected_phase==0?0:1;}
static int park(void)
{
    if(!armed)return 0;
    const long long limits[] = {0, 1, -1};
    for (size_t i=0;i<sizeof(limits)/sizeof(*limits);++i) {
        XL_RuntimeDrainResult r=xlRuntimeDrainRetiringChecked(limits[i]);
        assert(!r.ready&&r.ok&&!xrtGetError());++reentries;
    }
    xrtAtomic32Store(&entered,1,XMEMORY_RELEASE);
    XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(5*XRT_TIME_SECOND);
    while(!xrtAtomic32Load(&release_preflight,XMEMORY_ACQUIRE)) {
        assert(!xl6DeadlineExpiredV1(limit));xrtThreadYield();
    }
    return 1;
}
static int32 retire(void* unused)
{
    (void)unused;
    if(selected_phase==1){XL_RuntimeDrainResult r=xlRuntimeDrainRetiringChecked(-1);assert(r.ready&&r.ok);}
    else xlDestroyModule(retiring);
    assert(!xrtGetError());xrtAtomic32Store(&worker_done,1,XMEMORY_RELEASE);return 0;
}
static int32 unpark(void* unused)
{
    (void)unused;XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(20*XRT_TIME_MILLISECOND);
    while(!xl6DeadlineExpiredV1(limit))xrtThreadYield();
    xrtAtomic32Store(&release_preflight,1,XMEMORY_RELEASE);return 0;
}
static const char artifact[]=
    "#include <__xl_runtime/xruntime_decl.h>\n"
    "#include <__xl_runtime/xl6_code_lease.h>\n"
    "extern int drain_park(void);extern int drain_stage(void);\n"
    "static XL6_CodeOwnerGraph owner;\n"
    "int xl_script_bind_code_owner(const XL6_CodeOwner* p){if(!p||p->size!=sizeof(owner))return 0;owner=*(const XL6_CodeOwnerGraph*)p;return 1;}\n"
    "int xl_script_module_init(xvalue* arg){(void)arg;return 1;}\n"
    "bool xl_script_module_ownership_trace(XL6_ModuleOwnershipVisit visit,void* data){(void)data;return visit!=0;}\n"
    "int xl_script_module_unit_checked(void){return 1;}\n"
    "static XL6_ModulePrepareResult prepare(void){return drain_stage()==0&&!drain_park()?XL6_MODULE_PREPARE_BUSY:XL6_MODULE_PREPARE_READY;}\n"
    "static XL6_ModuleRetireResult step(void){return drain_stage()==1&&!drain_park()?XL6_MODULE_RETIRE_BUSY:XL6_MODULE_RETIRE_READY;}\n"
    "static const XL6_ModuleRetirementV1 graph={sizeof(graph),0x584C364D525431ULL,xl_script_module_ownership_trace,xl_script_module_unit_checked};\n"
    "const XL6_ModuleRetirementV1* xl_script_module_retirement_v1(void){return &graph;}\n"
    "const XL6_ModuleRetirementV4* xl_script_module_retirement_v4(void){static const XL6_ModuleRetirementV4 api={sizeof(api),0x584C364D525434ULL,&graph,prepare,step};return &api;}\n";

static void cycle(int argc,char** argv,unsigned phase)
{
    selected_phase=phase;armed=phase!=1;
    xrtAtomic32Store(&entered,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&release_preflight,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&worker_done,0,XMEMORY_RELEASE);
    XL_ContextOptions options=xlDefaultContextOptions();
    /* Source-cohort validation may select its existing TCC provider/SDK.
     * Ordinary product validation passes none and uses its embedded SDK. */
    assert(argc==1||argc>=3);
    if(argc>=3){options.tccDllPath=argv[1];options.tccLibPath=argv[2];}
    XL_Context* context=xlCreateContext(&options);assert(context);
    for(int i=3;i<argc;++i)assert(xlAddIncludePath(context,argv[i]));
    assert(xlRegisterCFunction(context,"drain_park",(void*)park));
    assert(xlRegisterCFunction(context,"drain_stage",(void*)stage));
    retiring=xlCompileCSourceStringToModule(context,artifact);
    if(!retiring)fprintf(stderr,"fixture: %s\n",xlGetLastError(context));
    assert(retiring&&xlActivateModule(context,retiring,NULL));
    if(phase==1){xlDestroyModule(retiring);assert(!xrtGetError());armed=true;}
    xthread* worker=xrtThreadCreate(retire,NULL,0);assert(worker);
    XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(5*XRT_TIME_SECOND);
    while(!xrtAtomic32Load(&entered,XMEMORY_ACQUIRE)) {
        assert(!xl6DeadlineExpiredV1(limit));xrtThreadYield();
    }
    XL_RuntimeDrainResult zero=xlRuntimeDrainRetiringChecked(0);
    uint64 begin=xl6MonotonicMillisecondsV1();XL_RuntimeDrainResult bounded=xlRuntimeDrainRetiringChecked(20);
    uint64 elapsed=xl6MonotonicMillisecondsV1()-begin;
    xthread* releaser=xrtThreadCreate(unpark,NULL,0);assert(releaser);
    XL_RuntimeDrainResult waiting=xlRuntimeDrainRetiringChecked(-1);
    bool released=xrtAtomic32Load(&release_preflight,XMEMORY_ACQUIRE)!=0;
    assert(xrtThreadWaitFor(releaser,5*XRT_TIME_SECOND)==XWAIT_OK);
    assert(xrtThreadWaitFor(worker,5*XRT_TIME_SECOND)==XWAIT_OK);
    xrtThreadDestroy(releaser);xrtThreadDestroy(worker);
    XL_RuntimeDrainResult final=xlRuntimeDrainRetiringChecked(-1);
    assert(final.ready&&final.ok&&xrtAtomic32Load(&worker_done,XMEMORY_ACQUIRE));
    xlDestroyContext(context);
    printf("drain threads: phase=%u same-driver=%u zero=%d/%d bounded=%d/%d elapsed=%llu wait=%d/%d released=%d\n",
        phase,reentries,zero.ready,zero.ok,bounded.ready,bounded.ok,(unsigned long long)elapsed,waiting.ready,waiting.ok,released);
    fflush(stdout);
    assert(reentries==(phase+1)*3&&!zero.ready&&zero.ok&&!bounded.ready&&bounded.ok&&elapsed>=20000);
    assert(waiting.ready&&waiting.ok&&released&&!xrtGetError());
}
int main(int argc,char** argv)
{
    assert(xlRuntimeInit());
    for(unsigned phase=0;phase<3;++phase)cycle(argc,argv,phase);
    xlRuntimeUnit();return 0;
}

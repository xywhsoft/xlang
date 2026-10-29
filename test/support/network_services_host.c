#define XL_STATIC_EMBED 1
#include "xlc.h"
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_execution_resource.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static xvalue* held;
static int mode,finalized,reentry;
static int64_t hostMode(void){return mode;}
static void hostHold(xvalue* value)
{assert(!held&&value);held=xrtValueRetain(value);assert(held);}
static void hostFinalized(void)
{
    ++finalized;
    XL_RuntimeDrainResult result=xlRuntimeDrainRetiringChecked(0);
    assert(!result.ready&&result.ok);++reentry;
}
static XL6_ExecutionResourceV1* resource(const char* field)
{
    xvalue* object=xrtValueObjectGet(held,(xstrview){field,strlen(field)});assert(object);
    xvalue* handle=xrtValueObjectGet(object,XRT_STR_LITERAL("handle"));assert(handle);
    void *raw=NULL,*user=NULL;assert(xrtValueGetHandle(handle,&raw,NULL,&user)&&raw&&!user);return raw;
}
int main(int argc,char** argv)
{
    /* root, source, source-SDK, TCC runtime/core, TCC library dir, temps, mode */
    assert(argc==8);mode=atoi(argv[7]);assert(mode>=1&&mode<=3);
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=argv[1];options.tempDir=argv[6];
    options.tccDllPath=argv[4];options.tccLibPath=argv[5];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcAddIncludePath(compiler,argv[3]));
    char headers[4096];int n=snprintf(headers,sizeof(headers),"%s/release/include",argv[1]);assert(n>0&&(size_t)n<sizeof(headers));
    assert(xlcAddIncludePath(compiler,headers)&&xlcRegisterCFunction(compiler,"network_services_mode",(void*)hostMode)&&
        xlcRegisterCFunction(compiler,"network_services_hold",(void*)hostHold)&&
        xlcRegisterCFunction(compiler,"network_services_finalized",(void*)hostFinalized));
    assert(xlcAddSourceFile(compiler,argv[2]));
    if(!xlcCompile(compiler)){fprintf(stderr,"service compile: %s\n",xlcGetLastError(compiler));return 2;}
    XL_Module* module=xlcRelocateModule(compiler);assert(module);
    if(!xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL)){fprintf(stderr,"service main: %s\n",xlcGetLastError(compiler));return 3;}
    assert(held&&!finalized);
    XL6_ExecutionResourceV1* engine=resource("network");
    XL6_ExecutionResourceSnapshotV1 before={.size=sizeof(before)};
    assert(xl6ExecutionResourceSnapshotV1Get(engine,&before)&&before.nativePresent&&before.owners);
    XL6_ExecutionScopeV1 entry={0};
    if(mode==2)assert(xl6ExecutionResourceEnterV1(engine,false,&entry));
    xlDestroyModule(module);module=NULL;
    /* A compiler/context lifetime must not masquerade as the external root's
     * code lease. The actual class, Futures and native children now stand alone. */
    xlcDestroyContext(compiler);compiler=NULL;
    XL_RuntimeDrainResult result=xlRuntimeDrainRetiringChecked(0);assert(!result.ready&&result.ok&&!finalized);
    XL6_ExecutionResourceSnapshotV1 after={.size=sizeof(after)};
    assert(xl6ExecutionResourceSnapshotV1Get(engine,&after)&&after.nativePresent&&after.owners);
    xvalue* owned=held;held=NULL;xrtValueRelease(owned);
    if(mode==2){
        result=xlRuntimeDrainRetiringChecked(0);assert(!result.ready&&result.ok);
        xl6ExecutionResourceLeaveV1(&entry);
    }
    bool allOk=true;double until=xrtTimer()+15;
    do{
        result=xlRuntimeDrainRetiringChecked(100);allOk=allOk&&result.ok;
        xrtClearError();assert(xrtTimer()<until);
    }while(!result.ready);
    assert(allOk==(mode!=3)&&finalized==1&&reentry==1);
    xrtClearError();
    result=xlRuntimeDrainRetiringChecked(15000);assert(result.ready&&result.ok);
    printf("Network service host: mode=%d actual external root/entry, finalizer reentry/outcome, owned retirement; no rescue\n",mode);return 0;
}

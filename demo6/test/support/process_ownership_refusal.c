/* Real process + shared terminal Future, never a fabricated native payload.
 * A refusal is a safety boundary, NOT acceptance of the process owning graph. */
#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>

static bool forbiddenEdge(xrtownershipref ref,void* context)
{ (void)ref;(void)context;assert(!"uncertified process must not publish partial edges");return false; }
static bool admit(xrtownershipref ref,void* context)
{
    const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;
    return xl6OwnershipCatalogResolveV1(ref,context,&adapter,&preparation,&finish);
}
static bool refused(xvalue* value,XL6_OwnershipCatalog* catalog,const char* phase)
{
    const XL6_NativeHandlePolicyV1* policy=xl6ProcessHandlePolicyV1Get();
    xrtSetErrorKind(XERR_RANGE);const xerror* original=xrtGetError();assert(original);
    bool direct=!policy->Trace(value,forbiddenEdge,NULL);
    assert(xrtGetError()==original);xrtClearError();
    xrtownershipref root=xrtValueOwnership(value);
    xrtownershipsnapshot* snapshot=NULL;xrtownershipscope scope={0};
    double deadline=xrtTimer()+5;
    while(!xrtOwnershipFreezeTryBegin(&scope)){assert(xrtTimer()<deadline);xrtThreadYield();}
    size_t count=0;assert(root.Ops->Count(root.Data,&count));
    bool accepted=xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,catalog,&snapshot);
    assert(accepted ? snapshot!=NULL : snapshot==NULL);
    xrtOwnershipSnapshotDestroy(snapshot);
    size_t after=0;assert(root.Ops->Count(root.Data,&after) && after==count);
    assert(xrtOwnershipScopeEnd(&scope));xrtClearError();
    printf("process %s: direct-refused=%d snapshot-refused=%d\n",phase,direct,!accepted);
    return direct && !accepted;
}
int main(int argc,char** argv)
{
    assert(argc==2);setvbuf(stdout,NULL,_IONBF,0);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainProcessLiveV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    xvalue* args=xrtValueArray();assert(args);
    assert(xrtValueArrayAppendNew(args,xrtValueString(XRT_STR_LITERAL("echo"))));
    void* command=xl6ProcessCommandDirectV1((xstrview){argv[1],strlen(argv[1])},args,-1,-1);
    xrtValueRelease(args);assert(command);
    void* raw=xl6ProcessSpawnV1(command);xl6ProcessCommandDestroyV1(command);assert(raw);
    XL6_ProcessLive* process=raw;
    xvalue* value=xl6NativeHandleTakeV1(&raw,xl6ProcessHandlePolicyV1Get());assert(value && !raw);
    /* The child is blocked reading stdin, not made "probably live" by a sleep. */
    assert(xl6ProcessRunningV1(process));
    bool ok=refused(value,catalog,"running-without-future");
    xfuture* first=xrtProcessWaitAsync(process->native);assert(first);
    xfuture* second=xrtProcessWaitAsync(process->native);assert(second==first);
    assert(!xrtFutureDone(first));
    ok=refused(value,catalog,"running-shared-future") && ok;
    assert(!xrtFutureDone(first) && xl6ProcessRunningV1(process));
    assert(xl6ProcessCloseV1(process,XPROCESS_STDIN));
    assert(xrtFutureWaitFor(first,UINT64_C(5000000))==XWAIT_OK);
    xfutureresult result={0};assert(xrtFutureResult(first,&result));
    assert(result.State==XFUTURE_RESOLVED && result.Value && !result.Error);
    xprocessstatus status;assert(xrtProcessStatus(process->native,&status));
    assert(!status.Code);
    ok=refused(value,catalog,"exited-shared-future") && ok;
    /* Ordinary last-user release still works; the independent shared Future
     * keeps its actual status snapshot alive after the language Process dies. */
    xrtValueRelease(value);
    xfutureresult retained={0};assert(xrtFutureResult(second,&retained));
    assert(retained.State==result.State && retained.Value==result.Value && !retained.Error);
    assert(!((const xprocessstatus*)retained.Value)->Code);
    xrtFutureDestroy(first);xrtFutureDestroy(second);xl6OwnershipCatalogDestroyV1(catalog);
    assert(!xrtGetError());
    if(!ok){fputs("uncertified process was accepted as a leaf\n",stderr);return 1;}
    puts("process refusal: 3 physical states, shared Future and ordinary destruction preserved");
    return 0;
}

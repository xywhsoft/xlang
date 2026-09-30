#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_STRING
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_cell.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const XL6_CellRuntimeV1* api;
typedef union Payload { bool b; uint32_t c; int64_t i; uint64_t u; double f; char* s; xvalue* v;
    struct { bool has; uint64_t value; } tagged; } Payload;
static size_t sizes[]={sizeof(bool),sizeof(int64_t),sizeof(double),sizeof(char*),sizeof(xvalue*),
    sizeof(uint64_t),sizeof(((Payload*)0)->tagged),sizeof(uint64_t),sizeof(uint32_t)};
static LONG flag(volatile LONG* p) { return InterlockedCompareExchange(p,0,0); }
static void wait_flag(volatile LONG* p) {
    DWORD start=GetTickCount(); while(!flag(p)) { assert((DWORD)(GetTickCount()-start)<10000); SwitchToThread(); }
}
static void joined(HANDLE h) { DWORD code; assert(WaitForSingleObject(h,10000)==WAIT_OBJECT_0 && GetExitCodeThread(h,&code) && !code); CloseHandle(h); }
static bool scope_empty(const xrtownershipscope* p) { return !p->Self && !p->Parent && !p->Thread && !p->Children && !p->Mode; }
static void balanced(const xmemdebugsnapshot* before) {
    xmemdebugsnapshot after; assert(!xrtGetError()); xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
}
static Payload payload(unsigned kind,unsigned next) {
    Payload p={0};
    switch(kind) {
    case XL6_CELL_BOOL: p.b=next!=0; break;
    case XL6_CELL_I64: p.i=-169-(int64_t)next; break;
    case XL6_CELL_F64: p.f=169.25+next; break;
    case XL6_CELL_STRING: p.s=xrtStrDup(next?"new":"old"); assert(p.s); break;
    case XL6_CELL_XVALUE: p.v=xrtValueArray(); assert(p.v && xrtValueReserve(p.v,4)); break;
    case XL6_CELL_U64: case XL6_CELL_TYPE_ID: p.u=UINT64_MAX-next; break;
    case XL6_CELL_TRIVIAL_BYTES: p.tagged.has=next!=0; p.tagged.value=169+next; break;
    case XL6_CELL_CHAR: p.c=next?'Z':'A'; break;
    default: assert(0);
    }
    return p;
}
static void drop_payload(unsigned kind,Payload* p) {
    if(kind==XL6_CELL_STRING) xrtFree(p->s); else if(kind==XL6_CELL_XVALUE) xrtValueRelease(p->v);
    memset(p,0,sizeof(*p));
}
static void family(unsigned kind) {
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    Payload old=payload(kind,0),next=payload(kind,1),borrowed={0},taken={0};
    XL6_Cell* cell=api->Create((XL6_CellKind)kind,sizes[kind]); assert(cell);
    api->Initialize(cell,&old,sizes[kind]); api->Load(cell,&borrowed,sizes[kind]);
    assert(!memcmp(&old,&borrowed,sizes[kind]));
    bool owns=kind==XL6_CELL_STRING || kind==XL6_CELL_XVALUE;
    if(owns) {
        api->Exchange(cell,&next,&taken,sizes[kind]); assert(!memcmp(&old,&taken,sizes[kind]));
        if(kind==XL6_CELL_STRING) assert(!strcmp(taken.s,"old"));
        else { xrtownershipref view=xrtValueOwnership(taken.v); size_t count; assert(view.Ops->Count(view.Data,&count) && count==1); }
        drop_payload(kind,&taken);
    } else api->Store(cell,&next,sizes[kind]);
    api->Load(cell,&borrowed,sizes[kind]); assert(!memcmp(&next,&borrowed,sizes[kind]));
    xrtownershipref view=api->Ownership(cell); size_t count;
    assert(api->Retain(cell)==cell && view.Ops->Count(view.Data,&count) && count==2); api->Release(cell);
    xrtownershipresult graph={0}; bool reachable=false;
    const size_t nodes=kind==XL6_CELL_XVALUE?3u:1u;
    const size_t physicalEdges=kind==XL6_CELL_XVALUE?2u:0u;
    assert(xrtOwnershipInspectReachable(&view,1,NULL,0,&reachable,&graph,NULL,NULL) && reachable &&
        graph.ExternalRootCount==1 && graph.NodeCount==nodes && graph.EdgeCount==physicalEdges);
    /* Internal owning slots are incoming edges too, not just RC subtractions. */
    assert(xrtOwnershipInspectReachable(&view,1,&view,1,&reachable,&graph,NULL,NULL) && !reachable && !graph.ExternalRootCount);
    assert(graph.NodeCount==nodes && graph.EdgeCount==physicalEdges+1u);
    api->Release(cell); balanced(&before);
    for(unsigned fail=0;fail<(kind==XL6_CELL_TRIVIAL_BYTES?2u:1u);++fail) {
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(fail));
        assert(!api->Create((XL6_CellKind)kind,sizes[kind]) && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear(); assert(xrtErrorKind(xrtGetError())==XERR_MEMORY); xrtClearError(); balanced(&before);
    }
}
typedef struct Work {
    XL6_Cell* cell; Payload next,old; volatile LONG ready,done,go; unsigned kind;
} Work;
static DWORD WINAPI mutation(void* data) {
    Work* w=data; xrtownershipscope refused={0};
    assert(!xrtOwnershipFreezeTryBegin(&refused) && scope_empty(&refused)); InterlockedExchange(&w->ready,1);
    switch(w->kind) {
    case 0: api->Initialize(w->cell,&w->next,sizeof(xvalue*)); break;
    case 1: api->Store(w->cell,&w->next,sizeof(uint64_t)); break;
    case 2: api->Exchange(w->cell,&w->next,&w->old,sizeof(xvalue*)); break;
    case 3: assert(api->Retain(w->cell)==w->cell); break;
    case 4: api->Release(w->cell); break;
    default: assert(0);
    }
    InterlockedExchange(&w->done,1); return 0;
}
static void frozen(unsigned kind) {
    Work w={0}; xmemdebugsnapshot before; xrtownershipscope freeze={0};
    xrtMemDebugSnapshot(&before); w.kind=kind;
    unsigned carrier=kind==1?XL6_CELL_U64:XL6_CELL_XVALUE;
    w.cell=api->Create((XL6_CellKind)carrier,sizes[carrier]); assert(w.cell);
    if(kind!=0) { Payload p=payload(carrier,0); api->Initialize(w.cell,&p,sizes[carrier]); }
    if(kind==0 || kind==1 || kind==2) w.next=payload(carrier,1);
    if(kind==4) api->Retain(w.cell);
    xrtownershipref view=api->Ownership(w.cell); xrtownershipresult beforeGraph={0},during={0}; size_t count,observed;
    assert(xrtOwnershipFreezeTryBegin(&freeze) && view.Ops->Count(view.Data,&count) && xrtOwnershipInspect(&view,1,NULL,0,&beforeGraph));
    HANDLE thread=CreateThread(NULL,0,mutation,&w,0,NULL); assert(thread); wait_flag(&w.ready);
    for(unsigned i=0;i<2000;++i) SwitchToThread();
    assert(!flag(&w.done) && view.Ops->Count(view.Data,&observed) && observed==count &&
        xrtOwnershipInspect(&view,1,NULL,0,&during) && !memcmp(&beforeGraph,&during,sizeof(during)));
    assert(xrtOwnershipScopeEnd(&freeze)); joined(thread);
    assert(view.Ops->Count(view.Data,&observed) && observed==count+(kind==3?1u:0u)-(kind==4?1u:0u));
    if(kind==3) api->Release(w.cell);
    if(kind==2) drop_payload(carrier,&w.old);
    if(kind<=2) { Payload p={0}; api->Load(w.cell,&p,sizes[carrier]); assert(!memcmp(&p,&w.next,sizes[carrier])); }
    api->Release(w.cell); balanced(&before);
}
static void hold_finalizer(xvalue* value,void* data) { Work* w=data; (void)value; InterlockedExchange(&w->ready,1); wait_flag(&w->go); }
static DWORD WINAPI last_release(void* data) { Work* w=data; api->Release(w->cell); InterlockedExchange(&w->done,1); return 0; }
static void finalizer_tail(void) {
    Work w={0}; xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* value=xrtValueObjectLifo(); assert(value && xrtValueObjectFinalizerBind(value,hold_finalizer,&w));
    w.cell=api->Create(XL6_CELL_XVALUE,sizeof(value)); assert(w.cell); api->Initialize(w.cell,&value,sizeof(value));
    HANDLE thread=CreateThread(NULL,0,last_release,&w,0,NULL); assert(thread); wait_flag(&w.ready);
    for(unsigned i=0;i<100;++i) { xrtownershipscope refused={0}; assert(!xrtOwnershipFreezeTryBegin(&refused) && scope_empty(&refused)); }
    InterlockedExchange(&w.go,1); joined(thread); assert(flag(&w.done)); balanced(&before);
}
int main(void) {
    assert(xrtMemDebugEnable(true)); api=xl6CellRuntimeV1Get(); assert(api && !api->Retain(NULL)); api->Release(NULL);
    for(unsigned round=0;round<50;++round) {
        for(unsigned kind=0;kind<9;++kind) family(kind);
        for(unsigned kind=0;kind<5;++kind) frozen(kind);
        finalizer_tail();
    }
    puts("Resident cells: 450 typed lifetime/exchange/graph families, 500 actual allocation refusals, 250 frozen init/store/exchange/ref transitions, 50 in-flight payload finalizers; actual counts and exact allocator balance, no generated descriptors or native root registration");
    return 0;
}

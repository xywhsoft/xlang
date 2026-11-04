/* Exact version admission and last real snapshot-reference release through
 * the resident plan. Native Proxy/Font payloads have their separate CPU gate. */
#define XRT_MODULE_MEMORY_DEBUG
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_completion.h"
#include "src/runtime/xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef struct Node {
    size_t refs,checkedDrops,legacyDrops,finalized,cleared,finished,restored;
    const void* claim;
    struct Node* edge;
    bool fail,diagnostic;
} Node;
static const xrtownershipops ops;
static const xrtownershipadapterv1 adapter;
static const xrtownershipfinishv2 completion;
static const xrtownershipfinishv1* selected;
static size_t counts,traces;
static xrtownershipref view(Node* n){return (xrtownershipref){n,&ops};}
static bool count(const void* data,size_t* out)
{const Node* n=data;assert(n->refs);++counts;*out=n->refs;return true;}
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{const Node* n=data;++traces;return !n->edge || visit(view(n->edge),context);}
static const xrtownershipops ops={count,trace};
static bool hold(const void* data)
{Node* n=(Node*)data;++n->refs;return true;}
static bool release(Node* n)
{
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(n->refs);bool last=--n->refs==0;assert(xrtOwnershipScopeEnd(&mutation));
    if(!last)return true;
    ++n->finalized;
    if(n->diagnostic)xrtSetErrorKind(XERR_VALUE);
    return !n->fail;
}
static void drop(const void* data){Node* n=(Node*)data;++n->legacyDrops;assert(release(n));}
static bool checked_drop(const void* data){Node* n=(Node*)data;++n->checkedDrops;return release(n);}
static bool claim(const void* data,const void* token)
{Node* n=(Node*)data;assert(!n->claim);n->claim=token;return true;}
static void restore(const void* data,const void* token)
{Node* n=(Node*)data;assert(n->claim==token);n->claim=NULL;++n->restored;}
static void clear(const void* data,const void* token)
{Node* n=(Node*)data;assert(n->claim==token);++n->cleared;}
static xrtownershipfinishresult finish(const void* data,const void* token)
{
    Node* n=(Node*)data;assert(n->claim==token && n->cleared && n->edge==n);
    n->edge=NULL;assert(release(n));++n->finished;return XRT_OWNERSHIP_FINISH_READY;
}
static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,NULL};
static const xrtownershipfinishv2 completion={{sizeof(completion),&adapter,finish},XRT_OWNERSHIP_FINISH_V2_TAG,checked_drop};
static const xrtownershipfinishv1 legacy={sizeof(legacy),&adapter,finish};
static bool resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** out,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** step)
{
    (void)context;*out=NULL;*preparation=NULL;*step=NULL;
    if(ref.Ops!=&ops)return false;
    *out=&adapter;*step=selected;return true;
}
static bool domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** out,const xrtownershippreparationv1** preparation,
    const xrtownershipfinishv1** step)
{(void)policies;return resolve(ref,NULL,out,preparation,step);}
static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,domain_resolve,NULL};
static bool blocked;
static bool enter(void* data){(void)data;return !blocked;}
static void leave(void* data){(void)data;}
static const XL6_OwnershipBoundaryV1 boundary={sizeof(boundary),enter,leave,NULL,NULL};
static XL6_OwnershipAdmissionV1 probe(xrtownershipref ref,void* context,
    const xrtownershipadapterv1** out,const xrtownershippreparationv1** preparation,
    const xrtownershipfinishv1** step)
{return resolve(ref,context,out,preparation,step)?XL6_OWNERSHIP_ADMISSION_ADMITTED:XL6_OWNERSHIP_ADMISSION_REFUSED;}
static void empty_mailbox(void)
{xl6_error_result result={0};uint64_t n=99;assert(xl6CallbackFailureTakeV1(&result,&n));assert(result.ok && !result.error && n==0);}
static void cycle(bool committed,bool failed,bool diagnostic,bool scope,bool prior,bool nullPrimary)
{
    Node n={.refs=1,.fail=failed,.diagnostic=diagnostic};
    if(committed)n.edge=&n;
    selected=&completion.V1;XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV3(view(&n),resolve,NULL,&plan)==
        (committed?XL6_OWNERSHIP_PLAN_READY:XL6_OWNERSHIP_PLAN_ROOTED));
    assert(n.refs==2);
    if(committed){
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(n.refs==1 && n.cleared==1 && n.finished==1);
    }else assert(release(&n) && n.refs==1);
    xl6_error_scope errors={0};if(scope)assert(xl6ErrorScopeBegin(&errors));
    if(nullPrimary)xl6ErrorScopeCaptureFailure(&errors);
    if(prior)xrtSetErrorKind(XERR_ARGUMENT);
    const xerror* earlier=xrtGetError();
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(n.refs==0 && n.finalized==1 && n.checkedDrops==1 && !n.legacyDrops);
    if(scope){
        xl6_error_result result=xl6ErrorScopeEnd(&errors);
        assert(result.ok==!(failed||diagnostic||prior||nullPrimary));
        if(nullPrimary)assert(!result.error);
        else if(prior)assert(result.error==earlier);
        else if(diagnostic)assert(xrtErrorKind(result.error)==XERR_VALUE);
        else assert(!result.error);
        xrtErrorFree(result.error);empty_mailbox();
    }else{
        if(prior)assert(xrtGetError()==earlier);
        xl6_error_result result={0};uint64_t amount=99;
        assert(xl6CallbackFailureTakeV1(&result,&amount));
        assert(result.ok==!(failed||diagnostic));assert(amount==((failed||diagnostic)?1u:0u));
        if(diagnostic)assert(xrtErrorKind(result.error)==XERR_VALUE);else assert(!result.error);
        xrtErrorFree(result.error);
    }
    xrtClearError();
}
static void abort_and_busy(void)
{
    Node n={.refs=1};n.edge=&n;selected=&completion.V1;XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV7(view(&n),probe,NULL,false,NULL,&boundary,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(n.claim==plan && n.refs==2);blocked=true;
    xrtSetErrorKind(XERR_ARGUMENT);const xerror* prior=xrtGetError();
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_BUSY);
    assert(xrtGetError()==prior && n.claim==plan && !n.checkedDrops && n.refs==2);
    xrtClearError();blocked=false;
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(n.restored==1 && !n.claim && n.refs==1 && n.checkedDrops==1);
    n.edge=NULL;assert(release(&n) && n.finalized==1);
}
static bool discover(xrtownershipvisitor visit,ptr visitorContext,void* data)
{return visit(view(data),visitorContext);}
static void multiple_failures(void)
{
    Node a={.refs=1,.diagnostic=true},b={.refs=1,.fail=true};
    selected=&completion.V1;XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV4(view(&a),resolve,discover,&b,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(release(&a) && release(&b));
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    assert(!result.ok && !result.error); /* b failed first without a diagnostic. */
    assert(a.finalized==1 && b.finalized==1 && a.checkedDrops==1 && b.checkedDrops==1);
    assert(!a.legacyDrops && !b.legacyDrops);empty_mailbox();
}
static void failed_creation(void)
{
    unsigned failedAfterPin=0;
    for(unsigned budget=0;budget<24;++budget){
        /* Independent native control storage permits pinRoot to acquire its
         * first actual reference. The plan must drop it on snapshot OOM. */
        Node n={.fail=true};selected=&completion.V1;XL6_OwnershipPlan* plan=NULL;
        assert(xrtMemDebugFailAfter(budget));
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV5(view(&n),resolve,discover,&n,&plan);
        bool allocationFailed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(status==XL6_OWNERSHIP_PLAN_ERROR){
            assert(allocationFailed && !plan && !n.refs);
            if(n.checkedDrops){++failedAfterPin;assert(n.finalized==1 && !n.legacyDrops);}
            xrtClearError();
        }else{
            assert(status==XL6_OWNERSHIP_PLAN_READY && plan && n.refs==1 && n.claim==plan);
            xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
            assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
            xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(!result.ok && !result.error);
        }
    }
    assert(failedAfterPin>0);empty_mailbox();
}
static void invalid_descriptors(void)
{
    Node n={.refs=1};n.edge=&n;xrtownershipfinishv2 bad=completion;
    for(unsigned i=0;i<5;++i){
        bad=completion;
        switch(i){case 0:--bad.V1.size;break;case 1:++bad.V1.size;break;
            case 2:bad.AbiTag^=1;break;case 3:bad.DropChecked=NULL;break;default:bad.V1.Adapter=NULL;break;}
        selected=&bad.V1;XL6_OwnershipPlan* plan=NULL;size_t c=counts,t=traces;
        assert(xl6OwnershipPlanTryCreateV3(view(&n),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
        assert(counts==c && traces==t && n.refs==1 && !n.claim);xrtClearError();
        const XL6_OwnershipDomainV1* domains[]={&domain};
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
        const xrtownershipadapterv1* a=NULL;const xrtownershippreparationv1* p=NULL;const xrtownershipfinishv1* f=NULL;
        assert(!xl6OwnershipCatalogResolveV1(view(&n),catalog,&a,&p,&f) && !a && !p && !f);
        xrtClearError();xl6OwnershipCatalogDestroyV1(catalog);
    }
    n.edge=NULL;assert(release(&n));
}
static void legacy_page(void)
{
    /* Any accidental read of V2.AbiTag on a real V1 descriptor faults. */
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
    char* memory=VirtualAlloc(NULL,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(memory);
    DWORD old;assert(VirtualProtect(memory+page,page,PAGE_NOACCESS,&old));
#else
    long value=sysconf(_SC_PAGESIZE);assert(value>0);size_t page=(size_t)value;
    char* memory=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);assert(memory!=MAP_FAILED);
    assert(mprotect(memory+page,page,PROT_NONE)==0);
#endif
    xrtownershipfinishv1* exact=(xrtownershipfinishv1*)(memory+page-sizeof(*exact));*exact=legacy;
    assert(xl6OwnershipCompletionValid(exact,&adapter) && !xl6OwnershipCompletionV2Get(exact));
    Node n={.refs=1};selected=exact;XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV3(view(&n),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(n.refs==1 && n.legacyDrops==1 && !n.checkedDrops);assert(release(&n));
#if defined(_WIN32)
    assert(VirtualFree(memory,0,MEM_RELEASE));
#else
    assert(munmap(memory,page*2)==0);
#endif
}
int main(void)
{
    assert(xrtMemDebugEnable(true));empty_mailbox();
    /* Warm context/error storage before checking the balanced live delta. */
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    xrtSetErrorKind(XERR_ARGUMENT);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned round=0;round<40;++round){
        for(unsigned bits=0;bits<32;++bits)
            cycle((bits&1)!=0,(bits&2)!=0,(bits&4)!=0,(bits&8)!=0,(bits&16)!=0,false);
        cycle(false,true,true,true,false,true);
        abort_and_busy();multiple_failures();invalid_descriptors();legacy_page();
    }
    failed_creation();
    empty_mailbox();xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    puts("Checked snapshot release: V1 guard page, exact V2 admission, committed/aborted last reference, BUSY retry, NULL first failure, prior diagnostic, mailbox and balanced resources passed");
    return 0;
}

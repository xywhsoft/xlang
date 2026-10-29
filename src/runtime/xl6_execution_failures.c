#define XRT_MODULE_ATOMIC
#define XRT_MODULE_MEMORY
#define XRT_MODULE_THREAD
#include "src/common/xl6_xrt.h"
#include "xl6_execution_failures.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

struct XL6_ExecutionFailuresV1 {
    xatomic64 gate;
    size_t references;
    uint64_t count;
    xerror* error;
    const void* claim;
    bool saturated,cleared,unreported;
};
static void begin(XL6_ExecutionFailuresV1* state,xrtownershipscope* scope)
{
    if(!xrtOwnershipMutationBegin(scope))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&state->gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();}
}
static void end(XL6_ExecutionFailuresV1* state,xrtownershipscope* scope)
{xrtAtomic64Store(&state->gate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(scope))abort();}
XL6_ExecutionFailuresV1* xl6ExecutionFailuresCreateV1(void)
{
    XL6_ExecutionFailuresV1* state=xrtCalloc(1,sizeof(*state));
    if(state)state->references=1;
    return state;
}
XL6_ExecutionFailuresV1* xl6ExecutionFailuresRetainV1(XL6_ExecutionFailuresV1* state)
{
    if(!state){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope mutation={0};begin(state,&mutation);
    bool ok=state->references&&state->references<SIZE_MAX&&!state->cleared;
    if(ok)++state->references;
    end(state,&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok?state:NULL;
}
void xl6ExecutionFailuresReleaseV1(XL6_ExecutionFailuresV1* state)
{
    if(!state)return;
    xrtownershipscope mutation={0};begin(state,&mutation);
    if(!state->references)abort();
    bool last=--state->references==0;
    if(last&&state->claim&&!state->cleared)abort();
    xerror* error=last?state->error:NULL;if(last)state->error=NULL;
    end(state,&mutation);
    if(last){xrtErrorFree(error);xrtFree(state);}
}
void xl6ExecutionFailuresRecordV1(XL6_ExecutionFailuresV1* state,bool ok,xerror* error)
{
    xl6_error_result result={ok,error};
    if(result.ok){xrtErrorFree(result.error);return;}
    xrtownershipscope mutation={0};begin(state,&mutation);
    if(!state->references||state->cleared)abort();
    if(!state->count){state->error=result.error;result.error=NULL;}
    if(state->count==UINT64_MAX)state->saturated=true;else ++state->count;
    state->unreported=true;
    end(state,&mutation);xrtErrorFree(result.error);
}
bool xl6ExecutionFailuresSnapshotV1(XL6_ExecutionFailuresV1* state,XL6_ExecutionFailureSnapshotV1* output)
{
    if(!state||!output||output->size!=sizeof(*output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(state,&mutation);
    bool ok=state->references&&!state->cleared;
    if(ok)*output=(XL6_ExecutionFailureSnapshotV1){sizeof(*output),state->count,state->saturated};
    end(state,&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok;
}
static bool checkFailure(XL6_ExecutionFailuresV1* state,bool retirement)
{
    if(!state){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(state,&mutation);
    bool valid=state->references&&!state->cleared;
    bool pending=valid&&state->count&&(!retirement||state->unreported),ok=valid&&!pending;
    xerror* error=pending?xrtErrorRef(state->error):NULL;
    if(pending&&!retirement)state->unreported=false;
    end(state,&mutation);
    if(!valid){xrtSetErrorKind(XERR_STATE);return false;}
    if(!ok)xrtSetErrorTake(error);
    return ok;
}
bool xl6ExecutionFailuresCheckV1(XL6_ExecutionFailuresV1* state){return checkFailure(state,false);}
bool xl6ExecutionFailuresUnreportedV1(XL6_ExecutionFailuresV1* state){return checkFailure(state,true);}
static bool count(const void* raw,size_t* output)
{const XL6_ExecutionFailuresV1* state=raw;if(!state||!output||!state->references||state->cleared)return false;*output=state->references;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{const XL6_ExecutionFailuresV1* state=raw;size_t refs;return visit&&count(raw,&refs)&&(!state->error||visit(xrtErrorOwnership(state->error),context));}
const xrtownershipops xl6ExecutionFailuresOpsV1={count,trace};
xrtownershipref xl6ExecutionFailuresOwnershipV1(const XL6_ExecutionFailuresV1* state)
{return (xrtownershipref){state,state?&xl6ExecutionFailuresOpsV1:NULL};}
static bool hold(const void* raw){return xl6ExecutionFailuresRetainV1((XL6_ExecutionFailuresV1*)raw)!=NULL;}
static void drop(const void* raw){xl6ExecutionFailuresReleaseV1((XL6_ExecutionFailuresV1*)raw);}
static bool claim(const void* raw,const void* token)
{XL6_ExecutionFailuresV1* s=(XL6_ExecutionFailuresV1*)raw;if(!token||s->cleared||(s->claim&&s->claim!=token))return false;s->claim=token;return true;}
static void restore(const void* raw,const void* token)
{XL6_ExecutionFailuresV1* s=(XL6_ExecutionFailuresV1*)raw;if(!token||s->claim!=token||s->cleared)abort();s->claim=NULL;}
static void clear(const void* raw,const void* token)
{XL6_ExecutionFailuresV1* s=(XL6_ExecutionFailuresV1*)raw;if(!token||s->claim!=token||s->cleared)abort();s->cleared=true;}
static bool finish(const void* raw,const void* token)
{
    XL6_ExecutionFailuresV1* s=(XL6_ExecutionFailuresV1*)raw;xrtownershipscope mutation={0};begin(s,&mutation);
    if(!token||s->claim!=token||!s->cleared)abort();
    bool pending=s->unreported;xerror* error=pending?xrtErrorRef(s->error):NULL;s->unreported=false;end(s,&mutation);
    /* All participants have physically prepared before Clear. Notify once;
     * an independent host outcome retains failure through any Finish retry. */
    if(pending)xrtSetErrorTake(error);
    return !pending;
}
const xrtownershipadapterv1* xl6ExecutionFailuresAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finish};
    size_t refs;return ref.Ops==&xl6ExecutionFailuresOpsV1&&count(ref.Data,&refs)?&adapter:NULL;
}

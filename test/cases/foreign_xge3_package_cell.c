/* Real shared native RC and a Cell -> callback -> closure -> view cycle. */
#include <xge3_runtime.h>
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_callback_failure.h"
#include <assert.h>
#include <stdio.h>

static int render(xui_widget_t* widget,int row,int column,const xui_table_view_cell_output_t* cell,
    xui_draw_context_t* draw,xui_rect_t rect,int state,void* user)
{(void)widget;(void)row;(void)column;(void)cell;(void)draw;(void)rect;(void)state;(void)user;return XUI_OK;}
static XL6_ForeignView* create(xui_table_view_cell_output_t** output)
{
    assert(xuiTableViewCellOutputCreate(output)==XUI_OK);
    XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(*output,&xl6Xui3TableCellOutputPolicy,
        &xuiTableViewCellOutputOwnershipDomainV1,NULL);assert(view);return view;
}
static void shared(void)
{
    xui_table_view_cell_output_t* cell=NULL;XL6_ForeignView* view=create(&cell);
    assert(xuiTableViewCellOutputAddRef(cell)==XUI_OK);
    XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(view,&call,NULL,0));
    assert(xuiTableViewCellOutputSetText(call.raw,(xstrview){"shared",6})==XUI_OK);
    assert(xl6ForeignViewCloseV1(view)); /* Deferred until the active call returns. */
    assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(view));
    /* Closing one language owner must not destroy another native owner. */
    assert(xuiTableViewCellOutputSetText(cell,(xstrview){"still alive",11})==XUI_OK);
    bool consumed=false;assert(xuiTableViewCellOutputReleaseOwnerChecked(cell,&consumed)==XUI_OK && consumed);
}
typedef struct DropOwner {unsigned refs,drops;} DropOwner;
static bool owner_count(const void* raw,size_t* output)
{const DropOwner* owner=raw;if(!owner || !output || !owner->refs)return false;*output=owner->refs;return true;}
static bool owner_trace(const void* raw,xrtownershipvisitor visit,void* user)
{(void)raw;(void)visit;(void)user;return true;}
static const xrtownershipops owner_ops={owner_count,owner_trace};
static bool owner_hold(const void* raw){++((DropOwner*)raw)->refs;return true;}
static bool owner_drop(const void* raw)
{DropOwner* owner=(DropOwner*)raw;assert(owner->refs);--owner->refs;++owner->drops;return false;}
static void failed_consumption(bool oom)
{
    xui_table_view_cell_output_t* cell=NULL;XL6_ForeignView* view=create(&cell);DropOwner owner={0};
    xui_callback_owner_t native={{&owner,&owner_ops},owner_hold,owner_drop};
    assert(xuiTableViewCellOutputSetRendererOwned(cell,render,NULL,&native)==XUI_OK);
    if(oom)assert(xrtMemDebugFailAfter(0));
    assert(!xl6ForeignViewCloseV1(view));
    /* Formatting the native status diagnostic may fail, but consumption and
     * the checked failure bit must survive without manufacturing success. */
    xerror* diagnostic=xrtTakeError();
    if(oom){assert(xrtMemDebugFailTriggered());assert(!diagnostic || xrtErrorKind(diagnostic)==XERR_MEMORY);xrtMemDebugFailClear();}
    else assert(diagnostic && xrtErrorKind(diagnostic)==XERR_STATE && xrtErrorCode(diagnostic)==XUI_ERROR_CALLBACK_FAILED);
    xrtErrorFree(diagnostic);
    assert(!owner.refs && owner.drops==1);
    /* The failed finalizer consumed its native owner and invalidated the view. */
    assert(xl6ForeignViewCloseV1(view) && xl6ForeignViewReleaseV1(view));
    uint64_t pending,total;assert(xl6CallbackFailureCountV1(&pending,&total) && !pending);
}
typedef struct Capture {XL6_ForeignView* view;} Capture;
static unsigned captures_dropped;
static const int capture_identity=1;
static bool capture_valid(const void* layout,const void* raw,size_t bytes)
{return layout==&capture_identity && raw && bytes==sizeof(Capture) && ((const Capture*)raw)->view;}
static bool capture_trace(const void* layout,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{assert(capture_valid(layout,raw,bytes));return visit(xl6ForeignViewOwnershipV1(((const Capture*)raw)->view),user);}
static bool capture_drop(const void* layout,void* raw,size_t bytes)
{
    assert(capture_valid(layout,raw,bytes));Capture* capture=raw;XL6_ForeignView* view=capture->view;
    xrtFree(capture);++captures_dropped;return xl6ForeignViewReleaseV1(view);
}
static const XL6_ClosureEnvironmentOpsV1 captures={sizeof(captures),capture_valid,capture_trace,capture_drop};
static void invoke(void){}
static void cycle(void)
{
    xui_table_view_cell_output_t* cell=NULL;XL6_ForeignView* view=create(&cell);
    Capture* capture=xrtMalloc(sizeof(*capture));assert(capture);capture->view=xl6ForeignViewRetainV1(view);assert(capture->view);
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,invoke,NULL,capture,sizeof(*capture),&capture_identity,
        &captures,NULL,NULL,0);assert(closure);
    const XL6_NativeCallbackRuntimeV1* callbacks=xl6NativeCallbackRuntimeV1Get();
    XL6_NativeCallback* callback=callbacks->Create(NULL,closure,NULL,9);assert(callback);
    xui_callback_owner_t owner={callbacks->Ownership(callback),callbacks->HoldNative,callbacks->DropNative};
    assert(xuiTableViewCellOutputSetRendererOwned(cell,render,callback,&owner)==XUI_OK);
    assert(callbacks->Release(callback));xl6ClosureRuntimeV1Get()->Release(closure);
    assert(xuiTableViewCellOutputAddRef(cell)==XUI_OK);
    assert(xl6ForeignViewReleaseV1(view)); /* Only the real closure capture remains. */
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),
        xl6OwnershipDomainClosureV1Get(),xl6OwnershipDomainNativeCallbackV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);
    const XL6_OwnershipBoundaryV1* boundary=xl6OwnershipCatalogBoundaryV1Get(catalog);assert(boundary);
    XL6_OwnershipPlan* plan=NULL;
    /* The independent native AddRef is a genuine external root. */
    assert(xl6OwnershipPlanTryCreateV7(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,false,
        catalog,boundary,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    bool consumed=false;assert(xuiTableViewCellOutputReleaseOwnerChecked(cell,&consumed)==XUI_OK && consumed);
    xui_table_cell_scope_t busy={0};assert(xuiTableViewCellOutputCallScopeBegin(cell,&busy)==XUI_OK);plan=NULL;
    assert(xl6OwnershipPlanTryCreateV7(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,false,
        catalog,boundary,&plan)==XL6_OWNERSHIP_PLAN_BUSY && !plan);
    assert(xuiTableViewCellOutputCallScopeEnd(&busy)==XUI_OK);
    XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreateV7(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,false,
        catalog,boundary,&plan);assert(result==XL6_OWNERSHIP_PLAN_FINALIZERS || result==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && captures_dropped==1);
    xl6OwnershipCatalogDestroyV1(catalog);assert(!xl6ForeignViewPendingV1(0));
}
int main(void)
{
    assert(!xl6Xge3RuntimeBindV1());shared();failed_consumption(false);failed_consumption(true);cycle();
    puts("Cell: shared native owner, deferred close, native status/OOM consumed failure, busy refusal and real callback/view cycle: PASS");return 0;
}

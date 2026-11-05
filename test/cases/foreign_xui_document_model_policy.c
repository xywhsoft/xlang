/* CPU-only integration of real default-provenance Native model roots with
 * resident ForeignView and a leased semantic-finalization domain. */
#include <xge3_runtime.h>
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <__xl_runtime/xl6_callback_failure.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned captures,drops,notifications;
static int notificationStatus;
static bool captureHold(const void* data)
{if(!xl6ForeignViewRetainV1((XL6_ForeignView*)data))return false;++captures;return true;}
static bool captureDrop(const void* data)
{++drops;return xl6ForeignViewReleaseV1((XL6_ForeignView*)data);}
static int notify(xui_document document,xui_document_change_set change,void* user)
{assert(document&&change&&user);++notifications;return notificationStatus;}
static size_t count(xrtownershipref ref)
{
    size_t result=0;xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));
    assert(ref.Ops->Count(ref.Data,&result));assert(xrtOwnershipScopeEnd(&scope));return result;
}
static XL6_ForeignView* wrap(void* raw,const XL6_ForeignViewDescriptorV1* policy)
{XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(raw,policy,&xl6Xui3DocumentModelDomainV1,NULL);assert(view);return view;}
static void closeView(XL6_ForeignView* view)
{assert(xl6ForeignViewCloseV1(view));assert(xl6ForeignViewReleaseV1(view));assert(xl6ForeignViewDrainV1());}
static void sevenRoots(void)
{
    xui_document document=NULL;xui_document_snapshot snapshot=NULL;
    xui_doc_desc_t desc={.iSize=sizeof(desc),.iProfile=XUI_DOCUMENT_MARKDOWN};
    assert(xuiDocumentCreateOwned(&desc,&document)==XUI_OK);
    assert(xuiDocumentLoadMarkdown(document,"abc\n",4)==XUI_OK);
    assert(xuiDocumentAcquireSnapshot(document,&snapshot)==XUI_OK);
    xui_document_source_index index=NULL;
    assert(xuiDocumentSnapshotCreateSourceIndex(snapshot,NULL,&index)==XUI_OK);
    xui_doc_source_patch_t patch={.iSize=sizeof(patch),.iStart=0,.iEnd=1,.sText="Z",.iTextBytes=1};
    xui_document_prepare prepare=NULL;assert(xuiDocumentPrepareSource(document,NULL,&patch,1,&prepare)==XUI_OK);
    XL6_ForeignView* prepareView=wrap(prepare,&xl6Xui3DocumentPreparePolicy);
    closeView(prepareView); /* Consume only this owner; Native owns pending work. */
    closeView(wrap(index,&xl6Xui3DocumentSourceIndexPolicy));
    closeView(wrap(snapshot,&xl6Xui3DocumentSnapshotPolicy));
    closeView(wrap(document,&xl6Xui3DocumentPolicy));

    /* A rich fragment has an actual independent retained Snapshot. */
    assert(xuiDocumentCreateOwned(NULL,&document)==XUI_OK);
    xui_document_transaction txn=NULL;
    assert(xuiDocumentBeginTransaction(document,NULL,&txn)==XUI_OK);
    xui_doc_node_desc_t node={.iSize=sizeof(node),.iKind=XUI_DOC_PARAGRAPH};xui_doc_node_id id=0;
    assert(xuiDocumentTxnInsertNode(txn,XUI_DOCUMENT_ROOT,XUI_DOCUMENT_APPEND,&node,&id)==XUI_OK);
    node=(xui_doc_node_desc_t){.iSize=sizeof(node),.iKind=XUI_DOC_TEXT,.sText="abc",.iTextBytes=3};
    xui_doc_node_id text=0;assert(xuiDocumentTxnInsertNode(txn,id,XUI_DOCUMENT_APPEND,&node,&text)==XUI_OK);
    xui_document_change_set change=NULL;assert(xuiDocumentTxnCommit(txn,&change)==XUI_OK);
    closeView(wrap(txn,&xl6Xui3DocumentTxnPolicy));
    closeView(wrap(change,&xl6Xui3DocumentChangeSetPolicy));
    assert(xuiDocumentAcquireSnapshot(document,&snapshot)==XUI_OK);
    xui_doc_position_t pos={.iSize=sizeof(pos),.iKind=XUI_DOC_POSITION_TEXT,
        .iDocumentId=xuiDocumentGetIdentity(document),.iRevision=xuiDocumentGetRevision(document),
        .iNodeId=text,.iOffset=0,.iAffinity=XUI_DOC_AFTER};
    xui_doc_range_t range={.tAnchor=pos,.tCaret=pos};range.tCaret.iOffset=3;
    xui_document_fragment fragment=NULL;assert(xuiDocumentFragmentCreateRange(snapshot,&range,&fragment)==XUI_OK);
    closeView(wrap(fragment,&xl6Xui3DocumentFragmentPolicy));
    assert(xuiDocumentSnapshotReleaseChecked(snapshot)==XUI_OK && xuiDocumentReleaseChecked(document)==XUI_OK);
}
static void activeClose(void)
{
    xui_document document=NULL;assert(xuiDocumentCreateOwned(NULL,&document)==XUI_OK);
    assert(xuiDocumentAddRef(document)==XUI_OK);xrtownershipref ref=xuiDocumentOwnership(document);
    XL6_ForeignView* view=wrap(document,&xl6Xui3DocumentPolicy);XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(view,&call,NULL,0));assert(xl6ForeignViewCloseV1(view));
    assert(count(ref)==2); /* The active C call still owns the original root. */
    assert(xl6ForeignViewCallEndV1(&call));assert(count(ref)==1);
    assert(xl6ForeignViewReleaseV1(view));assert(xuiDocumentReleaseChecked(document)==XUI_OK);
}
static void cycle(void)
{
    xui_document document=NULL;assert(xuiDocumentCreateOwned(NULL,&document)==XUI_OK);
    XL6_ForeignView* view=wrap(document,&xl6Xui3DocumentPolicy);
    xui_callback_owner_t owner={xl6ForeignViewOwnershipV1(view),captureHold,captureDrop};uint64_t token=0;
    assert(xuiDocumentSubscribeOwned(document,notify,view,&owner,&token)==XUI_OK);
    assert(captures==1&&drops==0&&token);assert(xl6ForeignViewReleaseV1(view));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    XL6_OwnershipPlan* plan=NULL;
    XL6_OwnershipPlanResult created=xl6OwnershipPlanTryCreateV8(xl6ForeignViewOwnershipV1(view),
        xl6OwnershipCatalogProbeV1,NULL,xl6OwnershipCatalogResolveFinalizationV1,false,catalog,
        xl6OwnershipCatalogBoundaryV1Get(catalog),&plan);
    assert(created==XL6_OWNERSHIP_PLAN_PREPARATIONS || created==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    bool failed=false;assert(xl6OwnershipPlanFinalizeStepV1(plan,&failed)==XL6_OWNERSHIP_STEP_READY&&!failed);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1());assert(drops==captures);
}
static void nullableNotificationFailure(void)
{
    captures=drops=0;notificationStatus=1; /* Must not be mistaken for DEFERRED. */
    xui_document document=NULL;assert(xuiDocumentCreateOwned(NULL,&document)==XUI_OK);
    XL6_ForeignView* view=wrap(document,&xl6Xui3DocumentPolicy);
    xui_callback_owner_t owner={xl6ForeignViewOwnershipV1(view),captureHold,captureDrop};uint64_t token=0;
    assert(xuiDocumentSubscribeOwned(document,notify,view,&owner,&token)==XUI_OK);
    xui_document_transaction txn=NULL;assert(xuiDocumentBeginTransaction(document,NULL,&txn)==XUI_OK);
    xui_doc_node_desc_t node={.iSize=sizeof(node),.iKind=XUI_DOC_PARAGRAPH};xui_doc_node_id inserted=0;
    assert(xuiDocumentTxnInsertNode(txn,XUI_DOCUMENT_ROOT,XUI_DOCUMENT_APPEND,&node,&inserted)==XUI_OK);
    assert(xuiDocumentTxnCommit(txn,NULL)==XUI_OK && xuiDocumentTxnReleaseChecked(txn)==XUI_OK);
    assert(notifications==1 && xuiDocumentGetNotificationFailureCount(document)==1 &&
        xuiDocumentGetNotificationFailureStatus(document)==1 && !xrtGetError());
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
    assert(xrtMemDebugFailAfter(0));
    bool ok=xl6ForeignViewCloseV1(view);assert(!ok && xrtMemDebugFailTriggered());
    xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(!result.ok);
    /* XRT may provide its allocation-free MEM diagnostic; the original
     * Native notification intentionally supplied no diagnostic at all. */
    xrtErrorFree(result.error);
    xrtMemDebugFailClear();assert(captures==drops);
    /* Physical owner consumption is independent from the nullable failure. */
    assert(xl6ForeignViewCloseV1(view));assert(xl6ForeignViewReleaseV1(view));
    xl6_error_result pending={0};uint64_t pendingCount=0;
    assert(xl6CallbackFailureTakeV1(&pending,&pendingCount)&&pending.ok&&!pending.error&&!pendingCount);
    notificationStatus=0;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));assert(xl6Xge3RuntimeBindV1()==0);
    sevenRoots();activeClose();cycle();nullableNotificationFailure();
    assert(notifications==1&&!xl6ForeignViewPendingV1(0)&&!xrtGetError());
    puts("Seven real Native model policies, no-scope active close, leased V2 semantic cycle and nullable notification cleanup: PASS");return 0;
}

/* Real CPU Native paint readiness. No window, device, browser or clipboard. */
#include <xge3_runtime.h>
#define XUI_OWNED_RENDERER_HELPERS
#include "xui_owned_renderer_test.c"
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>
static void gone(void* user){++*(unsigned*)user;}
static void pureWait(bool oom)
{
    LayoutProvider p={0};xui_font font;layoutSetup(&p,&font);
    xui_document_renderer renderer=NULL;
    xui_doc_renderer_desc_t desc={.iSize=sizeof(desc),.tFonts={.normal=font}};
    assert(xuiDocumentRendererCreate(p.context,&desc,&renderer)==XUI_OK);
    unsigned invalidated=0;xui_lifetime_link_t watch={0};
    assert(xuiDocumentRendererLifetimeWatch(renderer,&watch,gone,&invalidated)==XUI_OK);
    xui_document document=NULL;xui_doc_desc_t dd={.iSize=sizeof(dd),.iProfile=XUI_DOCUMENT_MARKDOWN};
    assert(xuiDocumentCreateOwned(&dd,&document)==XUI_OK);
    const char text[]="one **two** and *three* different fragments\n\nsecond paragraph\n";
    assert(xuiDocumentLoadMarkdown(document,text,sizeof(text)-1)==XUI_OK);
    xui_document_snapshot snapshot=NULL;assert(xuiDocumentAcquireSnapshot(document,&snapshot)==XUI_OK);
    assert(xuiDocumentRendererSetSnapshot(renderer,snapshot,NULL)==XUI_OK);
    p.busyNextShape=true;int status=xuiDocumentRendererLayout(renderer,220,0,400);
    assert((status==XUI_OK || status==XUI_ERROR_BUSY) && p.shapes>0 && !p.ready);
    xrtClearError();uint64_t pending=0,total=0;assert(xl6CallbackFailureCountV1(&pending,&total));
    const XL6_ForeignViewDescriptorV1* policy=xl6Xui3DocumentRendererPolicyGet();
    assert(policy && policy->Destroy && policy->CallBegin && policy->CallEnd && policy->Watch && policy->Unwatch);
    assert(policy->watchBytes==sizeof(xui_lifetime_link_t));
    assert(policy->flags&XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    if(oom)assert(xrtMemDebugFailAfter(0));
    assert(!policy->Destroy(renderer));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    xrtMemDebugFailClear();
    assert(outcome.ok && !outcome.error && !xrtGetError() && !invalidated);
    uint64_t afterPending=0,afterTotal=0;assert(xl6CallbackFailureCountV1(&afterPending,&afterTotal));
    assert(afterPending==pending && afterTotal==total);
    p.ready=true;assert(xl6ErrorScopeBegin(&scope));
    assert(policy->Destroy(renderer));outcome=xl6ErrorScopeEnd(&scope);
    assert(outcome.ok && !outcome.error && !xrtGetError() && invalidated==1 && !watch.ppPrevious);
    assert(p.paintDrops==p.shapes);
    assert(xuiDocumentSnapshotReleaseChecked(snapshot)==XUI_OK && xuiDocumentReleaseChecked(document)==XUI_OK);
    assert(xuiDestroyChecked(p.context)==XUI_OK && xuiFontReleaseChecked(font)==XUI_OK && p.fontDrops==1);
    assert(xuiProxyReleaseChecked(p.proxy)==XUI_OK && drop(&p.owner));
}
static void externalInvalidation(void)
{
    LayoutProvider p={0};
    xui_font font;
    layoutSetup(&p,&font);
    xui_document_renderer renderer=NULL;
    xui_doc_renderer_desc_t desc={.iSize=sizeof(desc),.tFonts={.normal=font}};
    assert(xuiDocumentRendererCreate(p.context,&desc,&renderer)==XUI_OK);
    assert(xuiDocumentRendererGetContext(renderer)==p.context);

    const XL6_ForeignViewDescriptorV1* policy=xl6Xui3DocumentRendererPolicyGet();
    unsigned invalidated=0;
    xui_lifetime_link_t watch={0};
    assert(xrtMemDebugFailAfter(0));
    assert(policy->Watch(renderer,&watch,gone,&invalidated));
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();

    XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(renderer,policy,&xl6Xui3OwnershipDomainV1,NULL);
    assert(view);
    assert(xuiDestroyChecked(p.context)==XUI_OK);
    assert(xuiDocumentRendererGetContext(renderer)==NULL && invalidated==0);

    XL6_ForeignViewCallV1 active={0};
    xui_call_scope_t scope={0};
    assert(xl6ForeignViewCallBeginV1(view,&active,&scope,sizeof(scope)));
    assert(xuiDocumentRendererReleaseChecked(renderer)==XUI_OK);
    assert(invalidated==1 && !watch.ppPrevious);
    assert(xl6ForeignViewCallEndV1(&active));
    assert(!xl6ForeignViewCallBeginV1(view,&active,&scope,sizeof(scope)));
    assert(xrtGetError());
    xrtClearError();
    assert(xl6ForeignViewCloseV1(view));
    assert(xl6ForeignViewReleaseV1(view) && xl6ForeignViewDrainV1());
    assert(xuiFontReleaseChecked(font)==XUI_OK && p.fontDrops==1);
    assert(xuiProxyReleaseChecked(p.proxy)==XUI_OK && drop(&p.owner));
}
int main(void)
{
    assert(xl6Xge3RuntimeBindV1()==0 && xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    pureWait(false);pureWait(true);externalInvalidation();
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    puts("renderer policy: deferred retry and external invalidation each consume once");
    return 0;
}

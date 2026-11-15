/* Test-only native readers and ownership baselines; no product adapters. */
#include "foreign_xui_state_outputs.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot stateBefore;
void fixtureStateBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&stateBefore);}
void fixtureStateEqual(void)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);assert(s.LiveCount==stateBefore.LiveCount&&s.LiveBytes==stateBefore.LiveBytes&&s.InvalidFreeCount==stateBefore.InvalidFreeCount&&s.DoubleFreeCount==stateBefore.DoubleFreeCount);}
void fixtureStateBudget(int n)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}
void fixtureStateReset(void){xrtMemDebugFailClear();xrtClearError();}
int fixtureStateBound(void)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);uint64_t n=s.AllocCount-stateBefore.AllocCount+s.ReallocCount-stateBefore.ReallocCount+8;assert(n<2048);return (int)n;}
void fixtureStateHotReport(int native)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);printf("Dock state heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",(unsigned long long)(s.AllocCount-stateBefore.AllocCount),(unsigned long long)(s.FreeCount-stateBefore.FreeCount),(unsigned long long)(s.ReallocCount-stateBefore.ReallocCount));}
void fixtureStateNativeHot(xui_widget dock)
{for(int i=0;i<100;++i){xvalue* state=NULL;assert(xuiDockPanelSaveState(dock,&state)==XUI_OK&&state);xrtValueRelease(state);}}
xui_doc_range_t fixtureStateRange(xui_document_snapshot snapshot,uint64_t paragraph)
{
 xui_doc_range_t r={0};r.tAnchor.iSize=r.tCaret.iSize=sizeof(r.tAnchor);
 r.tAnchor.iKind=r.tCaret.iKind=XUI_DOC_POSITION_GAP;
 r.tAnchor.iDocumentId=r.tCaret.iDocumentId=xuiDocumentSnapshotGetIdentity(snapshot);
 r.tAnchor.iRevision=r.tCaret.iRevision=xuiDocumentSnapshotGetRevision(snapshot);
 r.tAnchor.iNodeId=r.tCaret.iNodeId=paragraph;r.tCaret.iOffset=1;return r;
}
static int sameString(const char* a,const char* b){return a&&b?!strcmp(a,b):a==b;}
xui_doc_range_t fixtureStateDocRange(xui_document document,uint64_t paragraph)
{
 xui_doc_range_t r={0};r.tAnchor.iSize=r.tCaret.iSize=sizeof(r.tAnchor);
 r.tAnchor.iKind=r.tCaret.iKind=XUI_DOC_POSITION_GAP;
 r.tAnchor.iDocumentId=r.tCaret.iDocumentId=xuiDocumentGetIdentity(document);
 r.tAnchor.iRevision=r.tCaret.iRevision=xuiDocumentGetRevision(document);
 r.tAnchor.iNodeId=r.tCaret.iNodeId=paragraph;r.tCaret.iOffset=1;return r;
}
void fixtureStateNodeCheck(xui_document_snapshot snapshot,uint64_t node,const char* a,const char* b,const char* c,const char* d,const char* e)
{xui_doc_node_info_t info={.iSize=sizeof(info)};assert(xuiDocumentSnapshotGetNode(snapshot,node,&info)==XUI_OK);assert(sameString(a,info.sResource)&&sameString(b,info.sInfo)&&sameString(c,info.sTitle)&&sameString(d,info.sLinkTarget)&&sameString(e,info.sLinkTitle));}
void fixtureStateLinkCheck(xui_document_snapshot snapshot,const xui_doc_range_t* range,int linked,int mixed,const char* uri,const char* title)
{xui_doc_link_info_t info={.iSize=sizeof(info)};assert(xuiDocumentSnapshotQueryLink(snapshot,range,&info)==XUI_OK);assert(linked==info.bLinked&&mixed==info.bMixed&&sameString(uri,info.sUri)&&sameString(title,info.sTitle));}
void fixtureStateDockCheck(xui_widget dock,xvalue* state)
{
 int r=-1,w=-1,f=-1;assert(xuiDockPanelStateGetCounts(state,&r,&w,&f)==XUI_OK&&r==XUI_DOCK_PANEL_REGION_COUNT&&w==2&&f==1);
 assert(xuiDockPanelGetWindowCount(dock)==w);
}
void fixtureStateWindowCheck(xui_widget dock,int window,int expected)
{xui_dock_window_info_t info={.iSize=sizeof(info)};assert(xuiDockPanelGetWindowInfo(dock,window,&info)==XUI_OK&&info.iState==expected);}
void fixtureStateNativeOracle(void)
{
 xui_proxy proxy=NULL;xui_context ui=NULL;assert(fixtureRenderProxy(&proxy)==XUI_OK);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 assert(xuiCreateOwnedEx(proxy,&desc,&ui)==XUI_OK);
 xui_widget dock=NULL,a=NULL,b=NULL;assert(xuiDockPanelCreate(ui,&dock,NULL)==XUI_OK&&xuiPanelCreate(ui,&a,NULL)==XUI_OK&&xuiPanelCreate(ui,&b,NULL)==XUI_OK);
 int first=-1,second=-1,pane=-1;
 assert(xuiDockPanelAddWindow(dock,"One",a,&first)==XUI_OK&&xuiDockPanelAddWindow(dock,"Two",b,&second)==XUI_OK);
 assert(xuiDockPanelDockWindow(dock,first,XUI_DOCK_PANEL_REGION_DOCUMENT,XUI_DOCK_PANEL_SIDE_FILL,0,&pane)==XUI_OK);
 assert(xuiDockPanelFloatWindow(dock,second,(xui_rect_t){10,20,200,160})==XUI_OK);
 xvalue* state=NULL;fixtureStateBegin();assert(xuiDockPanelSaveState(dock,&state)==XUI_OK&&state);int bound=fixtureStateBound();xrtValueRelease(state);fixtureStateEqual();
 int failures=0;
 for(int i=0;i<bound;++i){fixtureStateBegin();fixtureStateBudget(i);state=NULL;int status=xuiDockPanelSaveState(dock,&state);fixtureStateReset();if(status!=XUI_OK){assert(!state);++failures;}xrtValueRelease(state);fixtureStateEqual();}
 assert(failures>0&&failures<bound);printf("Native Dock state OOM: prefixes=%d failures=%d\n",bound,failures);
 assert(xuiDockPanelSaveState(dock,&state)==XUI_OK);fixtureStateDockCheck(dock,state);
 assert(xuiDockPanelHideWindow(dock,first)==XUI_OK&&xuiDockPanelLoadState(dock,state)==XUI_OK);
 fixtureStateWindowCheck(dock,first,XUI_DOCK_PANEL_WINDOW_DOCKED);
 assert(xuiWidgetDestroyChecked(dock)==XUI_OK);int r,w,f;assert(xuiDockPanelStateGetCounts(state,&r,&w,&f)==XUI_OK&&w==2);xrtValueRelease(state);
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK&&fixtureRenderBalanced());
 puts("State outputs native: Dock owned value, OOM and independent state lifetime passed");
}

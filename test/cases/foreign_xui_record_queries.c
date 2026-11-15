/* Independent test readers and owned-string baseline, never product adapters. */
#include "foreign_xui_record_queries.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot queryBefore;
void fixtureQueryBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&queryBefore);}
void fixtureQueryEqual(void)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);assert(s.LiveCount==queryBefore.LiveCount&&s.LiveBytes==queryBefore.LiveBytes&&s.InvalidFreeCount==queryBefore.InvalidFreeCount&&s.DoubleFreeCount==queryBefore.DoubleFreeCount);}
void fixtureQueryBudget(int n){assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}
void fixtureQueryReset(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureQueryReport(int kind,int native)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);printf("Record query heap kind=%d %s: alloc=%llu free=%llu realloc=%llu\n",kind,native?"native":"language",(unsigned long long)(s.AllocCount-queryBefore.AllocCount),(unsigned long long)(s.FreeCount-queryBefore.FreeCount),(unsigned long long)(s.ReallocCount-queryBefore.ReallocCount));}
int fixtureQueryAccessible(xui_widget widget,QueryAccessibleScalars* out)
{
 xui_accessible_node_t n={.iSize=sizeof(n)};int status=xuiWidgetGetAccessibleNode(widget,0,&n);if(status!=XUI_OK)return status;
 *out=(QueryAccessibleScalars){n.iId,n.iParentId,n.iRole,n.iState,n.tBounds,n.iTextStart,n.iTextEnd,n.iLevel,n.iRow,n.iColumn,n.iRowCount,n.iColumnCount,n.iActions,n.tRange};return XUI_OK;
}
void fixtureQueryAccessibleText(xui_widget widget,int field,const char* text,bool absent)
{
 xui_accessible_node_t n={.iSize=sizeof(n)};assert(xuiWidgetGetAccessibleNode(widget,0,&n)==XUI_OK);
 const char* expected=field==0?n.sName:field==1?n.sValue:n.sDescription;
 assert(absent==(expected==NULL)&&!strcmp(text,expected?expected:""));
}
static void copyTexts(const char* const* texts,int count)
{
 char* owned[3]={0};assert(count<=3);
 for(int i=0;i<count;++i)if(texts[i]){owned[i]=xrtStrDup(texts[i]);assert(owned[i]);}
 for(int i=count;i>0;--i)xrtFree(owned[i-1]);
}
void fixtureQueryNativeHot(int kind,xui_context ui,xui_widget widget,xui_code_annotation_store store)
{
 for(int i=0;i<1000;++i){
  if(kind==0){xui_code_diagnostic_t d={.iSize=sizeof(d)};assert(xuiCodeAnnotationGetDiagnostic(store,0,&d)==XUI_OK);const char* p[]={d.sCode,d.sMessage,d.sSource};copyTexts(p,3);}
  else if(kind==1){xui_code_virtual_text_t v={.iSize=sizeof(v)};assert(xuiCodeEditGetVirtualText(widget,&v)==XUI_OK);const char* p[]={v.sText};copyTexts(p,1);}
  else if(kind==2){xui_accessible_node_t n={.iSize=sizeof(n)};assert(xuiWidgetGetAccessibleNode(widget,0,&n)==XUI_OK);const char* p[]={n.sName,n.sValue,n.sDescription};copyTexts(p,3);}
  else{xui_style_value_t v={.iSize=sizeof(v)};assert(xuiStyleGetToken(ui,"text",&v)==XUI_OK);const char* p[]={v.sText};copyTexts(p,1);}
 }
}
void fixtureQueryNativeOracle(void)
{
 xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=NULL;
 xui_widget editor=NULL,button=NULL;xui_code_annotation_store store=NULL;
 assert(fixtureRenderProxy(&proxy)==XUI_OK&&fixtureRenderFont(proxy,&font)==XUI_OK);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 assert(xuiCreateOwnedEx(proxy,&desc,&ui)==XUI_OK&&xuiSetDefaultFont(ui,font)==XUI_OK);
 assert(xuiCodeEditCreate(ui,&editor,NULL)==XUI_OK&&xuiButtonCreate(ui,&button,NULL)==XUI_OK);
 assert(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);
 xui_code_diagnostic_t d={.iSize=sizeof(d),.tRange={2,5},.iSeverity=2,.sCode="E42",.sMessage="\xe9\x94\x99\xe8\xaf\xaf\xe4\xbf\xa1\xe6\x81\xaf",.sSource="checker",.iUserData=UINT64_C(1234567890123456789)};
 assert(xuiCodeAnnotationSetDiagnostics(store,&d,1)==XUI_OK);
 xui_code_diagnostic_t got={.iSize=sizeof(got)};
 assert(xuiCodeAnnotationGetDiagnostic(store,0,&got)==XUI_OK&&got.tRange.iStart==2&&got.tRange.iEnd==5&&got.iSeverity==2&&got.iUserData==d.iUserData);
 assert(!strcmp(got.sCode,d.sCode)&&!strcmp(got.sMessage,d.sMessage)&&!strcmp(got.sSource,d.sSource));
 assert(xuiCodeAnnotationGetDiagnostic(store,-1,&got)==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiCodeEditSetText(editor,"abc")==XUI_OK&&xuiEditSetSelection(editor,1,1)==XUI_OK);
 xui_code_virtual_text_t v={.iSize=sizeof(v),.iKind=XUI_CODE_VIRTUAL_TEXT_INLINE_COMPLETION,.iOffset=1,.sText="completion",.iFlags=5,.iUserData=7};
 assert(xuiCodeEditSetVirtualText(editor,&v)==XUI_OK);
 memset(&v,0,sizeof(v));v.iSize=sizeof(v);
 assert(xuiCodeEditGetVirtualText(editor,&v)==XUI_OK&&v.iOffset==1&&!strcmp(v.sText,"completion")&&!v.iFlags&&!v.iUserData);
 assert(xuiButtonSetText(button,"Run")==XUI_OK&&xuiWidgetSetAccessibleName(button,"name")==XUI_OK&&xuiWidgetSetAccessibleDescription(button,"description")==XUI_OK);
 xui_accessible_node_t n={.iSize=sizeof(n)};assert(xuiWidgetGetAccessibleNode(button,0,&n)==XUI_OK&&n.iId&&!strcmp(n.sName,"name")&&!strcmp(n.sDescription,"description"));
 xui_style_value_t token={.iSize=sizeof(token),.iType=XUI_STYLE_VALUE_STRING,.sText="theme text"};
 assert(xuiStyleSetToken(ui,"text",&token)==XUI_OK);
 memset(&token,0,sizeof(token));token.iSize=sizeof(token);
 assert(xuiStyleGetToken(ui,"text",&token)==XUI_OK&&token.iType==XUI_STYLE_VALUE_STRING&&!strcmp(token.sText,"theme text"));
 for(int kind=0;kind<4;++kind){fixtureQueryBegin();fixtureQueryNativeHot(kind,ui,kind==1?editor:button,store);fixtureQueryReport(kind,1);fixtureQueryEqual();}
 xuiCodeAnnotationStoreDestroy(store);
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiFontReleaseChecked(font)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK&&fixtureRenderBalanced());
 puts("Record queries native: authoritative fields, status and owned-string baseline passed");
}

/* Test-only owning C baseline and CPU provider; no product forwarding layer. */
#include "foreign_xui_page_text.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot before;
void fixturePageTextBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixturePageTextEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}
void fixturePageTextBudget(int n)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}
void fixturePageTextReset(void){xrtMemDebugFailClear();xrtClearError();}
void fixturePageTextReport(int native)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("Page text heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
  (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),(unsigned long long)(after.ReallocCount-before.ReallocCount));
}
static void read_text(xui_widget page,const char** text)
{
 assert(xuiPageGetText(page,&text[0],&text[1],&text[2],&text[3])==XUI_OK);
 for(int i=0;i<4;++i)assert(text[i]);
}
void fixturePageTextNativeHot(xui_widget page)
{
 size_t observedBytes=0;
 for(int round=0;round<1000;++round){
  const char* text[4];char* copied[4];read_text(page,text);
  for(int i=0;i<4;++i){size_t size=strlen(text[i]);copied[i]=xrtStrDupN(text[i],size);assert(copied[i]);observedBytes+=size;}
  for(int i=3;i>=0;--i)xrtFree(copied[i]);
 }
 assert(observedBytes==21000);
}
void fixturePageTextNativeOracle(void)
{
 xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=NULL;xui_widget page=NULL,button=NULL;
 assert(fixtureRenderProxy(&proxy)==XUI_OK&&fixtureRenderFont(proxy,&font)==XUI_OK);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 assert(xuiCreateOwnedEx(proxy,&desc,&ui)==XUI_OK&&xuiSetDefaultFont(ui,font)==XUI_OK);
 assert(xuiPageCreate(ui,&page,NULL)==XUI_OK&&xuiButtonCreate(ui,&button,NULL)==XUI_OK);
 const char* text[4];read_text(page,text);
 assert(!strcmp(text[0],"First")&&!strcmp(text[1],"Last")&&!strcmp(text[2],"<")&&!strcmp(text[3],">"));
 assert(xuiPageGetText(button,&text[0],&text[1],&text[2],&text[3])==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiPageGetText(NULL,&text[0],&text[1],&text[2],&text[3])==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiPageSetText(page,"","Last","Previous","Next")==XUI_OK);read_text(page,text);assert(!strcmp(text[0],""));
 assert(xuiPageSetText(page,"First","Last","Previous","Next")==XUI_OK);
 fixturePageTextBegin();fixturePageTextNativeHot(page);fixturePageTextReport(1);fixturePageTextEqual();
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiFontReleaseChecked(font)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK&&fixtureRenderBalanced());
 puts("Page text native: four live buffers, defaults, empty, invalid widget and owning-copy baseline passed");
}

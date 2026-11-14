/* Test-only readers inspect the existing C API, not private widget layouts.
 * The language's output-record string contract remains a separate decision. */
#include "foreign_xui_menu_items.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot menuBound,menuHeap;
void fixtureMenuBoundBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&menuBound);}
int fixtureMenuBoundEnd(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 uint64 n=after.AllocCount-menuBound.AllocCount+after.ReallocCount-menuBound.ReallocCount+8;
 assert(n>=8&&n<=1024);return (int)n;
}
void fixtureMenuHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&menuHeap);}
void fixtureMenuHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==menuHeap.LiveCount&&after.LiveBytes==menuHeap.LiveBytes&&after.InvalidFreeCount==menuHeap.InvalidFreeCount&&after.DoubleFreeCount==menuHeap.DoubleFreeCount);
}
void fixtureMenuHotReport(int native)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("Menu heap %s: alloc=%llu free=%llu realloc=%llu\n",native==2?"array-mutation":native?"native":"language",
 (unsigned long long)(after.AllocCount-menuBound.AllocCount),
 (unsigned long long)(after.FreeCount-menuBound.FreeCount),
 (unsigned long long)(after.ReallocCount-menuBound.ReallocCount));
}
void fixtureMenuCheck(xui_widget menu,int kind,xui_widget child)
{
 int count=kind==0?1:kind==1?2:3;assert(xuiMenuGetItemCount(menu)==count);
 const xui_menu_item_t* first=xuiMenuGetItem(menu,0);assert(first);
 if(kind!=2){assert(!strcmp(first->sText,"kept")&&!strcmp(first->sShortcut,"Ctrl+K")&&first->iValue==1);}
 else {assert(!strcmp(first->sText,"Run")&&!strcmp(first->sShortcut,"Ctrl+R")&&first->iType==XUI_MENU_ITEM_CHECK&&first->iValue==11&&first->iIcon==7&&first->iState==(XUI_MENU_ITEM_ENABLED|XUI_MENU_ITEM_CHECKED|XUI_MENU_ITEM_DEFAULT));}
 if(kind){
  const xui_menu_item_t* more=xuiMenuGetItem(menu,1);assert(more&&!strcmp(more->sText,"More")&&more->sShortcut==NULL&&more->iType==XUI_MENU_ITEM_SUBMENU&&more->iValue==22&&more->iIcon==9&&more->pSubmenu==child&&more->pUser==NULL);
 }
 if(kind==2){const xui_menu_item_t* last=xuiMenuGetItem(menu,2);assert(last&&!strcmp(last->sText,"")&&last->sShortcut&&!strcmp(last->sShortcut,"")&&last->iType==XUI_MENU_ITEM_RADIO&&last->iValue==33&&last->pSubmenu==NULL);}
}
void fixtureMenuNullCheck(xui_widget menu)
{
 assert(xuiMenuGetItemCount(menu)==2);
 const xui_menu_item_t* a=xuiMenuGetItem(menu,0);const xui_menu_item_t* b=xuiMenuGetItem(menu,1);
 assert(a&&a->sText==NULL&&a->sShortcut==NULL&&a->iType==XUI_MENU_ITEM_SEPARATOR);
 assert(b&&b->sText&&!strcmp(b->sText,"")&&b->sShortcut&&!strcmp(b->sShortcut,""));
}
void fixtureMenuSelfCheck(xui_widget menu)
{
 const xui_menu_item_t* source=xuiMenuGetItem(menu,0);assert(source);
 xui_menu_item_t alias=*source;assert(xuiMenuSetItems(menu,&alias,1)==XUI_OK);
 assert(xuiMenuAddItem(menu,&alias)==XUI_OK&&xuiMenuGetItemCount(menu)==2);
}
void fixtureMenuNativeHot(xui_widget menu,xui_widget child,int changing)
{
 xui_menu_item_t values[3]={
  {.sText="Run",.sShortcut="Ctrl+R",.iType=XUI_MENU_ITEM_CHECK,.iState=XUI_MENU_ITEM_ENABLED|XUI_MENU_ITEM_CHECKED|XUI_MENU_ITEM_DEFAULT,.iValue=11,.iIcon=7},
  {.sText="More",.iType=XUI_MENU_ITEM_SUBMENU,.iState=XUI_MENU_ITEM_ENABLED,.iValue=22,.iIcon=9,.pSubmenu=child},
  {.sText="",.sShortcut="",.iType=XUI_MENU_ITEM_RADIO,.iState=XUI_MENU_ITEM_ENABLED|XUI_MENU_ITEM_DANGER,.iValue=33}};
 for(int i=0;i<1000;++i){if(changing)values[0].iValue=i&1;assert(xuiMenuSetItems(menu,values,3)==XUI_OK);}
}

/* Test-only native readers and stack-record baseline, never product adapters. */
#include "foreign_xui_control_items.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot controlBound,controlHeap;
void fixtureControlBoundBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&controlBound);}
int fixtureControlBoundEnd(void)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);uint64_t n=s.AllocCount-controlBound.AllocCount+s.ReallocCount-controlBound.ReallocCount+8;assert(n<=1024);return (int)n;}
void fixtureControlHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&controlHeap);}
void fixtureControlHeapEqual(void)
{xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);assert(s.LiveCount==controlHeap.LiveCount&&s.LiveBytes==controlHeap.LiveBytes&&s.InvalidFreeCount==controlHeap.InvalidFreeCount&&s.DoubleFreeCount==controlHeap.DoubleFreeCount);}
void fixtureControlReport(int kind,int native)
{
 xmemdebugsnapshot s;xrtMemDebugSnapshot(&s);
 printf("Control items heap kind=%d %s: alloc=%llu free=%llu realloc=%llu row=%zu\n",kind,native?"native":"language",
  (unsigned long long)(s.AllocCount-controlBound.AllocCount),(unsigned long long)(s.FreeCount-controlBound.FreeCount),(unsigned long long)(s.ReallocCount-controlBound.ReallocCount),kind==0?sizeof(xui_breadcrumb_item_t):kind==1?sizeof(xui_menubar_item_t):sizeof(xui_statusbar_item_t));
}
void fixtureControlSeed(int kind,xui_widget widget,xui_widget child)
{
 if(kind==0){xui_breadcrumb_item_t r={.sText="kept",.bClickable=1,.iValue=1};assert(xuiBreadcrumbSetItems(widget,&r,1)==XUI_OK);}
 else if(kind==1){xui_menubar_item_t r={.sText="kept",.iState=1,.iValue=1,.pMenu=child};assert(xuiMenuBarSetItems(widget,&r,1)==XUI_OK);}
 else{xui_statusbar_item_t r={.sText="kept",.iState=1,.iValue=1,.fWidth=100};assert(xuiStatusBarSetItems(widget,&r,1)==XUI_OK);}
}
int fixtureControlNativeSet(int kind,xui_widget widget,xui_widget child)
{
 if(kind==0){xui_breadcrumb_item_t r[3]={{"Home",1,11},{NULL,0,22},{"\xe9\xa1\xb9\xe7\x9b\xae",-1,33}};return xuiBreadcrumbSetItems(widget,r,3);}
 if(kind==1){xui_menubar_item_t r[3]={{.sText="&File",.iState=1,.iValue=11,.iMnemonic='f',.pMenu=child},{.iValue=22},{.sText="Help",.iState=1,.iValue=33,.iMnemonic='h',.pMenu=child}};return xuiMenuBarSetItems(widget,r,3);}
 xui_statusbar_item_t r[3]={{.sText="Ready",.iState=3,.iValue=11,.fWidth=100},{.iType=1,.iSection=2,.iState=1,.iValue=22,.fWidth=200,.fFlex=1,.fMin=10,.fMax=20,.fValue=99},{.sText="",.iType=2,.iSection=1,.iState=3,.iValue=33,.fFlex=2,.fMax=1}};
 return xuiStatusBarSetItems(widget,r,3);
}
void fixtureControlNativeHot(int kind,xui_widget widget,xui_widget child)
{for(int i=0;i<1000;++i)assert(fixtureControlNativeSet(kind,widget,child)==XUI_OK);}
void fixtureControlCheck(int kind,xui_widget widget,xui_widget child,int state)
{
 int count=state?3:1;
 if(kind==0){
  assert(xuiBreadcrumbGetItemCount(widget)==count);
  assert(!strcmp(xuiBreadcrumbGetItemText(widget,0),state?"Home":"kept")&&xuiBreadcrumbGetItemValue(widget,0)==(state?11:1));
  if(state){assert(!strcmp(xuiBreadcrumbGetItemText(widget,1),"")&&!strcmp(xuiBreadcrumbGetItemText(widget,2),"\xe9\xa1\xb9\xe7\x9b\xae"));assert(xuiBreadcrumbGetItemClickable(widget,0)==1&&xuiBreadcrumbGetItemClickable(widget,1)==0&&xuiBreadcrumbGetItemClickable(widget,2)==1);assert(xuiBreadcrumbGetItemValue(widget,1)==22&&xuiBreadcrumbGetItemValue(widget,2)==33);}
 }else if(kind==1){
  assert(xuiMenuBarGetItemCount(widget)==count);const xui_menubar_item_t* a=xuiMenuBarGetItem(widget,0);assert(a&&!strcmp(a->sText,state?"&File":"kept")&&a->iValue==(state?11:1)&&!a->pUser);
  if(state){const xui_menubar_item_t* b=xuiMenuBarGetItem(widget,1);const xui_menubar_item_t* c=xuiMenuBarGetItem(widget,2);assert(b&&c&&!strcmp(b->sText,"")&&!strcmp(c->sText,"Help")&&b->iValue==22&&c->iValue==33&&a->iMnemonic=='F'&&c->iMnemonic=='H');assert(b->pMenu==NULL&&a->pMenu==child&&c->pMenu==child);assert(a->iState==(state==2?0:1)&&c->iState==(state==2?0:1)&&!b->iState&&!b->pUser&&!c->pUser);}
 }else{
  assert(xuiStatusBarGetItemCount(widget)==count);const xui_statusbar_item_t* a=xuiStatusBarGetItem(widget,0);assert(a&&!strcmp(a->sText,state?"Ready":"kept")&&a->iValue==(state?11:1)&&!a->pUser);
  if(state){const xui_statusbar_item_t* b=xuiStatusBarGetItem(widget,1);const xui_statusbar_item_t* c=xuiStatusBarGetItem(widget,2);assert(a->iType==0&&a->iSection==0&&a->iState==3&&a->fWidth==100&&!a->fFlex);assert(b&&b->iType==1&&b->iSection==2&&b->iState==1&&b->iValue==22&&b->fWidth==200&&b->fFlex==1&&b->fMin==10&&b->fMax==20&&b->fValue==20&&!strcmp(b->sText,"")&&!b->pUser);assert(c&&c->iType==2&&c->iSection==1&&!c->iState&&c->iValue==33&&!c->fWidth&&c->fFlex==2&&c->fMax==1&&!c->fValue&&!strcmp(c->sText,"")&&!c->pUser);}
 }
}

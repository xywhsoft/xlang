#include "foreign_xui_code_annotations.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
static xmemdebugsnapshot prefix,hot;
static const xui_code_diagnostic_t diagnostics[2]={
 {0,{0,3},1,NULL,"one","test",UINT64_C(4294967296)},
 {0,{4,8},2,"W","",NULL,2}};
void fixtureAnnBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureAnnPrefixBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);}
void fixtureAnnPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureAnnHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=prefix.LiveCount||after.LiveBytes!=prefix.LiveBytes)
  fprintf(stderr,"Annotations live memory changed: %zu/%zu -> %zu/%zu\n",prefix.LiveCount,prefix.LiveBytes,after.LiveCount,after.LiveBytes);
 assert(after.LiveCount==prefix.LiveCount&&after.LiveBytes==prefix.LiveBytes&&
  after.InvalidFreeCount==prefix.InvalidFreeCount&&after.DoubleFreeCount==prefix.DoubleFreeCount);
}
void fixtureAnnHotBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&hot);}
void fixtureAnnHotReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("CodeAnnotations heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
}
void fixtureAnnCheck(xui_code_annotation_store store,int mode)
{
 xui_code_marker_t marker={0};xui_code_indicator_t indicator={0};int count=-1;
 if(mode==0||mode==4){
  xui_code_diagnostic_t first={0},second={0};assert(xuiCodeAnnotationGetDiagnosticCount(store)==2);
  assert(xuiCodeAnnotationGetDiagnostic(store,0,&first)==XUI_OK&&xuiCodeAnnotationGetDiagnostic(store,1,&second)==XUI_OK);
  assert(first.iSeverity==1&&first.sCode==NULL&&!strcmp(first.sMessage,"one")&&!strcmp(first.sSource,"test")&&first.iUserData==UINT64_C(4294967296));
  assert(second.iSeverity==2&&!strcmp(second.sCode,"W")&&second.sMessage&&!*second.sMessage&&second.sSource==NULL&&second.iUserData==2);
  assert(first.tRange.iStart==(mode==4 ? 1:0)&&first.tRange.iEnd==(mode==4 ? 4:3));
  assert(second.tRange.iStart==(mode==4 ? 5:4)&&second.tRange.iEnd==(mode==4 ? 9:8));
 }else if(mode==1||mode==2||mode==6){
  assert(xuiCodeAnnotationGetMarkers(store,1,&marker,1,&count)==XUI_OK&&count==1&&marker.iMarker==4&&marker.iFlags==7&&marker.iUserData==UINT64_C(4294967296));
  if(mode==1)assert(!marker.sTooltip);else if(mode==2)assert(marker.sTooltip&&!*marker.sTooltip);else assert(!strcmp(marker.sTooltip,"tip"));
 }else if(mode==3){
  assert(xuiCodeAnnotationGetIndicatorsAt(store,3,&indicator,1,&count)==XUI_OK&&count==1&&indicator.iIndicator==5&&indicator.tRange.iStart==2&&indicator.tRange.iEnd==4);
  assert(xuiCodeAnnotationGetIndicatorsAt(store,7,&indicator,1,&count)==XUI_OK&&count==1&&indicator.tRange.iStart==6&&indicator.tRange.iEnd==10);
  assert(xuiCodeAnnotationGetIndicatorsAt(store,5,NULL,0,&count)==XUI_OK&&count==0);
 }else if(mode==5){
  for(int i=0;i<12;++i)assert(xuiCodeAnnotationGetIndicatorsAt(store,i,NULL,0,&count)==XUI_OK&&count==0);
 }else assert(!"unknown check");
}
void fixtureAnnNativeHot(xui_code_annotation_store store,uint32_t version)
{for(int i=0;i<1000;++i){assert(xuiCodeAnnotationGetDiagnosticCount(store)==2);assert(xuiCodeAnnotationGetDiagnosticVersion(store)==version);}}
void fixtureAnnNativeCreate(void)
{xui_code_annotation_store store=NULL;assert(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);xuiCodeAnnotationStoreDestroy(store);}
void fixtureAnnNativeSet(xui_code_annotation_store store)
{assert(xuiCodeAnnotationSetDiagnostics(store,diagnostics,2)==XUI_OK);}
int fixtureAnnDuringCall(xui_code_annotation_store store,void (*callback)(void*),void* user)
{callback(user);return xuiCodeAnnotationGetDiagnosticCount(store)==2 ? XUI_OK:XUI_ERROR_INVALID_STATE;}

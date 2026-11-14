#include "foreign_xge_error_fields.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static xmemdebugsnapshot before;
int fixtureXgeErrorMutable(int mode)
{
 char subsystem[]="engine",operation[]="draw",message[]="mutable UTF-8 \xe4\xbd\xa0";
 xge_error_info_t info={sizeof(info),XGE_ERROR_INVALID_ARGUMENT,1,subsystem,operation,message};
 if(mode==1)info.sSubsystem=info.sOperation=info.sMessage=NULL;
 if(mode==2)info.sSubsystem=info.sOperation=info.sMessage="";
 if(mode==3)info.sSubsystem=info.sOperation=info.sMessage="prefix\0hidden";
 int result=xgeReportError(&info);
 memset(subsystem,'x',sizeof(subsystem)-1);memset(operation,'x',sizeof(operation)-1);memset(message,'x',sizeof(message)-1);
 return result;
}
void fixtureXgeErrorBudget(int allowance)
{
 assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));
 if(allowance<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter(allowance));
}
void fixtureXgeErrorHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureXgeErrorHeapReport(int native)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("Error fields heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
  (unsigned long long)(after.AllocCount-before.AllocCount),
  (unsigned long long)(after.FreeCount-before.FreeCount),
  (unsigned long long)(after.ReallocCount-before.ReallocCount));
 assert(after.LiveCount==before.LiveCount&&after.LiveBytes==before.LiveBytes&&after.InvalidFreeCount==before.InvalidFreeCount&&after.DoubleFreeCount==before.DoubleFreeCount);
}
static void nativeObserver(const xge_error_info_t* info,void* user)
{
 (void)user;char* a=info->sSubsystem?xrtStrDup(info->sSubsystem):NULL;
 char* b=info->sOperation?xrtStrDup(info->sOperation):NULL;
 char* c=info->sMessage?xrtStrDup(info->sMessage):NULL;
 assert(a&&b&&c);
 xrtFree(a);xrtFree(b);xrtFree(c);
}
void fixtureXgeErrorNativeHot(void)
{
 assert(xgeSetErrorCallback(nativeObserver,NULL)==XGE_OK);
 xge_error_info_t info={sizeof(info),XGE_ERROR_INVALID_ARGUMENT,1,"bench","draw","text"};
 fixtureXgeErrorHeapBegin();
 for(int i=0;i<1000;++i)assert(xgeReportError(&info)==XGE_OK);
 fixtureXgeErrorHeapReport(1);
 assert(xgeSetErrorCallback(NULL,NULL)==XGE_OK);
}

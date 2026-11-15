#include "foreign_xui_error_report.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int fixtureXuiErrorMutable(xui_context ui,int mode)
{
    char operation[32]="stack operation";char message[64]="mutable UTF-8 你";
    if(mode==2){operation[0]=0;message[0]=0;}
    if(mode==3){memcpy(operation,"prefix\0tail",11);memcpy(message,"prefix\0tail",11);}
    xui_error_info_t error={0};error.iSize=sizeof(error);error.iCode=XUI_ERROR_INVALID_ARGUMENT;
    error.iStage=XUI_ERROR_STAGE_USER;error.bRecoverable=1;
    error.sOperation=mode==1?NULL:operation;error.sMessage=mode==1?NULL:message;
    int status=xuiReportError(ui,&error);
    memset(operation,'x',sizeof(operation));memset(message,'y',sizeof(message));return status;
}
int fixtureXuiErrorAmbient(xui_context ui)
{
    xrtSetErrorInfo(XERR_IO,"error-report-test",42,"ambient business error");
    const xerror* ambient=xrtGetError();assert(ambient);
    xui_error_info_t error={0};error.iSize=sizeof(error);error.iCode=XUI_ERROR_INVALID_ARGUMENT;
    error.iStage=XUI_ERROR_STAGE_USER;error.bRecoverable=1;error.sOperation="ambient";error.sMessage="observed";
    int status=xuiReportError(ui,&error);assert(xrtGetError()==ambient);return status;
}
void fixtureXuiErrorBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
static xmemdebugsnapshot before;
void fixtureXuiErrorHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureXuiErrorHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("XuiError heap %s: alloc=%llu free=%llu realloc=%llu; payload=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount),sizeof(xui_error_info_t));
}
static void nativeObserver(xui_context ui,const xui_error_info_t* error,void* user)
{
    (void)ui;(void)user;char* a=xrtStrDup(error->sOperation);char* b=xrtStrDup(error->sMessage);
    assert(a&&b);xrtFree(a);xrtFree(b);
}
static size_t nativeReferences=1;
static bool nativeHold(const void* raw){assert(raw==&nativeReferences);++nativeReferences;return true;}
static bool nativeDrop(const void* raw){assert(raw==&nativeReferences&&nativeReferences>1);--nativeReferences;return true;}
static bool nativeCount(const void* raw,size_t* n){assert(raw==&nativeReferences);*n=nativeReferences;return true;}
static bool nativeTrace(const void* raw,xrtownershipvisitor visit,void* user){(void)raw;(void)visit;(void)user;return true;}
static const xrtownershipops nativeOps={nativeCount,nativeTrace};
void fixtureXuiErrorNativeHot(xui_context ui)
{
    const xui_callback_owner_t owner={{&nativeReferences,&nativeOps},nativeHold,nativeDrop};
    assert(xuiSetErrorCallbackOwned(ui,nativeObserver,NULL,&owner)==XUI_OK&&nativeReferences==2);
    xui_error_info_t error={0};error.iSize=sizeof(error);error.iCode=XUI_ERROR_INVALID_ARGUMENT;
    error.iStage=XUI_ERROR_STAGE_USER;error.bRecoverable=1;error.sOperation="bench";error.sMessage="text";
    fixtureXuiErrorHeapBegin();
    for(int i=0;i<1000;++i)assert(xuiReportError(ui,&error)==XUI_OK);
    fixtureXuiErrorHeapReport(1);assert(xuiSetErrorCallbackOwned(ui,NULL,NULL,NULL)==XUI_OK&&nativeReferences==1);
}

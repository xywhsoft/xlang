/* Native side of the same hot owned-callback workload. One host XRT only. */
#include "foreign_xui_code_document.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
static size_t refs=1;
static int calls;
static bool hold(const void* raw){assert(raw==&refs);++refs;return true;}
static bool drop(const void* raw){assert(raw==&refs && refs>1);--refs;return true;}
static bool count(const void* raw,size_t* output){assert(raw==&refs);*output=refs;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user){assert(raw==&refs);(void)visit;(void)user;return true;}
static const xrtownershipops ops={count,trace};
static void changed(xui_code_document document,xui_code_range_t range,uint32_t version,void* user)
{assert(document && user==&calls && refs==3 && range.iStart>=0 && version>0);++calls;}
void fixtureDocumentBudget(int allowance)
{if(allowance<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}}
void fixtureDocumentHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureDocumentHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("CodeDocument heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureDocumentNativeSetup(xui_code_document document)
{const xui_callback_owner_t owner={{&refs,&ops},hold,drop};calls=0;assert(refs==1 && xuiCodeDocumentSetChangeCallbackOwned(document,changed,&calls,&owner)==XUI_OK);}
void fixtureDocumentNativeHot(xui_code_document document)
{for(int i=0;i<1000;++i)assert(xuiCodeDocumentSetTextLength(document,"hot",3)==XUI_OK);assert(calls==1000 && refs==2);}
void fixtureDocumentNativeClear(xui_code_document document)
{assert(xuiCodeDocumentSetChangeCallback(document,NULL,NULL)==XUI_OK && refs==1);}
int fixtureDocumentUtf8(void){return XENCODING_UTF8;}

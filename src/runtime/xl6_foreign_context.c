#include "src/common/xl6_xrt.h"
#include "xl6_foreign_context.h"
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
static DWORD contextKeys[XL6_FOREIGN_CONTEXT_KEY_COUNT_V1];
#else
#include <pthread.h>
static pthread_key_t contextKeys[XL6_FOREIGN_CONTEXT_KEY_COUNT_V1];
#endif
static xonce contextOnce=XRT_ONCE_INIT;
static bool initialize(void* unused)
{
    (void)unused;unsigned count=0;
    for(;count<XL6_FOREIGN_CONTEXT_KEY_COUNT_V1;++count){
#if defined(_WIN32)
        contextKeys[count]=FlsAlloc(NULL);
        if(contextKeys[count]==FLS_OUT_OF_INDEXES)break;
#else
        if(pthread_key_create(&contextKeys[count],NULL))break;
#endif
    }
    if(count==XL6_FOREIGN_CONTEXT_KEY_COUNT_V1)return true;
    while(count){
        --count;
#if defined(_WIN32)
        if(!FlsFree(contextKeys[count]))abort();
#else
        if(pthread_key_delete(contextKeys[count]))abort();
#endif
    }
    return false;
}
bool xl6ForeignContextReadyV1(void)
{
    if(xrtOnce(&contextOnce,initialize,NULL))return true;
    xrtSetErrorKind(XERR_MEMORY);return false;
}
void* xl6ForeignContextGetV1(XL6_ForeignContextKeyV1 key)
{
    if((unsigned)key>=XL6_FOREIGN_CONTEXT_KEY_COUNT_V1)abort();
#if defined(_WIN32)
    return FlsGetValue(contextKeys[key]);
#else
    return pthread_getspecific(contextKeys[key]);
#endif
}
bool xl6ForeignContextSetV1(XL6_ForeignContextKeyV1 key,void* value)
{
    if((unsigned)key>=XL6_FOREIGN_CONTEXT_KEY_COUNT_V1)abort();
#if defined(_WIN32)
    if(FlsSetValue(contextKeys[key],value))return true;
#else
    if(!pthread_setspecific(contextKeys[key],value))return true;
#endif
    xrtSetErrorKind(XERR_MEMORY);return false;
}
bool xl6ForeignContextCallEnterV1(XL6_ForeignCallContextV1* scope,const void* owner)
{
    if(!scope || scope->self || scope->older || scope->newer || scope->owner){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ForeignContextReadyV1())return false;
    XL6_ForeignCallContextV1* older=xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1);
    if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1,scope))return false;
    scope->self=scope;scope->older=older;scope->owner=owner;if(older)older->newer=scope;return true;
}
bool xl6ForeignContextCallContainsV1(const XL6_ForeignCallContextV1* scope)
{
    if(!scope || scope->self!=scope)return false;
    for(const XL6_ForeignCallContextV1* item=xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1);item;item=item->older)
        if(item==scope)return true;
    return false;
}
bool xl6ForeignContextCallOwnsV1(const void* owner)
{
    if(!owner)return false;
    for(const XL6_ForeignCallContextV1* item=xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1);item;item=item->older)
        if(item->owner==owner)return true;
    return false;
}
void xl6ForeignContextCallLeaveV1(XL6_ForeignCallContextV1* scope)
{
    if(!xl6ForeignContextCallContainsV1(scope))abort();
    if(scope->newer)scope->newer->older=scope->older;
    else if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1,scope->older))abort();
    if(scope->older)scope->older->newer=scope->newer;
    *scope=(XL6_ForeignCallContextV1){0};
}

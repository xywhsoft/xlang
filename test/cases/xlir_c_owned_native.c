#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <__xl_runtime/xruntime_decl.h>
static xatomic64 count;
static const char* originalText;
static const void* originalBinary;
void* c_owned_text(void)
{
    char* data=malloc(6); assert(data); memcpy(data,"hello",6); originalText=data; return data;
}
bool c_owned_text_is_original(const char* data) { return data==originalText; }
void* c_owned_text_with_nul(void)
{
    const char text[]={'a',0,'b',0};
    char* data=malloc(sizeof(text));assert(data);memcpy(data,text,sizeof(text));originalText=data;return data;
}
void* c_owned_number(void)
{
    char* data=malloc(3); assert(data); memcpy(data,"42",3); return data;
}
void* c_owned_binary(void)
{
    const unsigned char bytes[]={0,255,17,42};
    void* data=malloc(sizeof(bytes)); assert(data); memcpy(data,bytes,sizeof(bytes)); originalBinary=data; return data;
}
void c_owned_release(void* data) { assert(data); xrtAtomic64FetchAdd(&count,1,XMEMORY_RELAXED); free(data); }
int64_t c_owned_released(void) { return (int64_t)xrtAtomic64Load(&count,XMEMORY_ACQUIRE); }
void* c_owned_unicode(void)
{
    const char text[]="A\xe4\xbd\xa0\xe5\xa5\xbd" "B";
    void* data=malloc(sizeof(text));assert(data);memcpy(data,text,sizeof(text));return data;
}
bool c_owned_debug(void)
{
    if(xrtMemDebugEnabled())return true;
    bool enabled=xrtMemDebugEnable(true);
    if(!enabled)xrtClearError(); /* optional in source shipping profiles */
    return enabled;
}
void c_owned_fail_next(void) { assert(xrtMemDebugFailAfter(0)); }
void c_owned_fail_clear(void) { xrtMemDebugFailClear();xrtClearError(); }
int64_t c_owned_allocations(void)
{
    xmemdebugsnapshot snapshot={0};xrtMemDebugSnapshot(&snapshot);return (int64_t)snapshot.AllocCount;
}

bool c_owned_text_span_original(xstrview text) { return text.Data==originalText && text.Size==5; }
bool c_owned_binary_span_original(xbytesview data) { return data.Data==originalBinary && data.Size==4; }
bool c_owned_binary_span_valid(xbytesview data) { const unsigned char expected[]={0,255,17,42}; return data.Size==4 && !memcmp(data.Data,expected,4); }

bool c_owned_debug_enabled(void) { return xrtMemDebugEnabled(); }

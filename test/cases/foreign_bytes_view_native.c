#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include <assert.h>
#include "foreign_bytes_view.h"
static xmemdebugsnapshot before;
static xmemdebugsnapshot balance;
static unsigned char source[]={0xff,0,0x80,1};
size_t foreign_bytes_size(xbytesview value){assert(value.Data);return value.Size;}
size_t foreign_bytes_pair(xbytesview a,xbytesview b){assert(a.Data && b.Data);return a.Size+b.Size;}
xbytesview foreign_bytes_echo(xbytesview value){assert(value.Data);return value;}
xbytesview foreign_bytes_static(int mode)
{
    switch(mode){
    case 0:return (xbytesview){source,sizeof(source)};
    case 1:return (xbytesview){source,0};
    case 2:return (xbytesview){0};
    case 3:return (xbytesview){NULL,4};
    case 5:xrtSetErrorKind(XERR_TYPE);return (xbytesview){(const void*)(uintptr_t)1,3};
    case 6:assert(xrtMemDebugFailAfter(0));return (xbytesview){source,sizeof(source)};
    default:return (xbytesview){source,SIZE_MAX};
    }
}
xbytesview foreign_bytes_optional(int mode){return foreign_bytes_static(mode);}
void foreign_bytes_begin(void){assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
int foreign_bytes_unchanged(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount;}
int foreign_bytes_allocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return (int)(after.AllocCount-before.AllocCount);}
void foreign_bytes_expect(int actual,int expected){assert(actual==expected);}
void foreign_bytes_reset(void){assert(xrtMemDebugFailAfter(-1));}
void foreign_bytes_mutate(void){source[0]^=1;}
void foreign_bytes_mark_balance(void){xrtMemDebugSnapshot(&balance);}
int foreign_bytes_balanced(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return after.LiveCount==balance.LiveCount && after.LiveBytes==balance.LiveBytes && after.InvalidFreeCount==balance.InvalidFreeCount && after.DoubleFreeCount==balance.DoubleFreeCount;}
foreign_bytes_widget* foreign_bytes_widget_get(void)
{static foreign_bytes_widget widget={{0xfe,0,0x81,2}};return &widget;}
xbytesview foreign_bytes_widget_echo(foreign_bytes_widget* widget,xbytesview value)
{assert(widget && value.Data);return value;}
xbytesview foreign_bytes_widget_data(foreign_bytes_widget* widget)
{assert(widget);return (xbytesview){widget->data,sizeof(widget->data)};}
void foreign_bytes_widget_set(foreign_bytes_widget* widget,xbytesview value)
{assert(widget && value.Data && value.Size==sizeof(widget->data));memcpy(widget->data,value.Data,value.Size);}
void* foreign_bytes_native_open(void){return foreign_bytes_widget_get();}
xbytesview foreign_bytes_native_echo(void* widget,xbytesview value)
{assert(widget && value.Data);return value;}
xbytesview foreign_bytes_native_result(void* widget,int mode)
{assert(widget);return foreign_bytes_static(mode);}
size_t foreign_bytes_native_text(void* widget,const char* value)
{assert(widget && value);return strlen(value);}
size_t foreign_bytes_native_text_view(void* widget,xstrview value)
{assert(widget && value.Data);return value.Size;}

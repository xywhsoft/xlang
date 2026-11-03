#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include <assert.h>
#include "foreign_text_view.h"
static xmemdebugsnapshot before;
size_t foreign_text_size(xstrview value){assert(value.Data);return value.Size;}
xstrview foreign_text_echo(xstrview value){assert(value.Data);return value;}
xstrview foreign_text_static(int mode)
{
    static const char data[]={'a',0,'b'};
    switch(mode){
    case 0:return (xstrview){data,sizeof(data)}; /* deliberately not terminated */
    case 1:return (xstrview){"",0};
    case 2:return (xstrview){0};
    case 3:return (xstrview){NULL,4};
    case 5:xrtSetErrorKind(XERR_TYPE);return (xstrview){(const char*)(uintptr_t)1,3};
    case 6:assert(xrtMemDebugFailAfter(0));return (xstrview){data,sizeof(data)};
    default:return (xstrview){data,SIZE_MAX};
    }
}
void foreign_text_begin(void){assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
xstrview foreign_text_optional(int mode){return foreign_text_static(mode);}
int foreign_text_unchanged(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return before.AllocCount==after.AllocCount && before.ReallocCount==after.ReallocCount;}
int foreign_text_allocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return (int)(after.AllocCount-before.AllocCount);}
void foreign_text_expect(int actual,int expected){assert(actual==expected);}
void foreign_text_reset(void){assert(xrtMemDebugFailAfter(-1));}
foreign_text_widget* foreign_text_widget_get(void)
{static foreign_text_widget widget={{'x',0,'y'}};return &widget;}
xstrview foreign_text_widget_echo(foreign_text_widget* widget,xstrview value)
{assert(widget && value.Data);return value;}
xstrview foreign_text_widget_text(foreign_text_widget* widget)
{assert(widget);return (xstrview){widget->data,sizeof(widget->data)};}
void foreign_text_widget_set(foreign_text_widget* widget,xstrview value)
{assert(widget && value.Data && value.Size==sizeof(widget->data));memcpy(widget->data,value.Data,value.Size);}

#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "foreign_cstring_result.h"
#include <assert.h>
static xmemdebugsnapshot before;
static int calls;
const char* foreign_cstring_result(int mode)
{
    ++calls;
    switch(mode){
    case 0:return "borrowed \xE4\xBD\xA0";
    case 1:return "";
    case 2:return NULL;
    case 3:xrtSetErrorKind(XERR_TYPE);return (const char*)(uintptr_t)1;
    case 4:assert(xrtMemDebugFailAfter(0));return "copy must fail";
    default:return "a\0b";
    }
}
const char* foreign_cstring_echo(const char* input){++calls;assert(input);return input;}
const char* foreign_cstring_optional(int mode){return foreign_cstring_result(mode);}
foreign_cstring_widget* foreign_cstring_widget_get(void)
{static foreign_cstring_widget widget={"original"};return &widget;}
const char* foreign_cstring_widget_text(foreign_cstring_widget* widget)
{++calls;assert(widget);return widget->text;}
const char* foreign_cstring_widget_echo(foreign_cstring_widget* widget,const char* input)
{assert(widget);return foreign_cstring_echo(input);}
void foreign_cstring_widget_set(foreign_cstring_widget* widget,const char* input)
{assert(widget && input && strlen(input)<sizeof(widget->text));strcpy(widget->text,input);}
void foreign_cstring_begin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);calls=0;}
int foreign_cstring_allocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.ReallocCount==before.ReallocCount);return (int)(after.AllocCount-before.AllocCount);}
int foreign_cstring_calls(void){return calls;}
void foreign_cstring_reset(void){assert(xrtMemDebugFailAfter(-1));}

#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "foreign_integer_min.h"
#include <assert.h>
static int calls;
static xmemdebugsnapshot before;
int integer_min_count(int value){++calls;return value;}
int integer_min_find(int value){++calls;return value;}
int integer_min_positive(int value){++calls;return value;}
long long integer_min_wide(long long value){++calls;return value;}
int integer_min_failure(int mode)
{
    ++calls;
    if(mode==1)xrtSetErrorInfo(XERR_TYPE,"integer_min.fixture",321,"producer diagnostic");
    if(mode==2)assert(xrtMemDebugFailAfter(0));
    if(mode==3){xrtSetErrorInfo(XERR_TYPE,"integer_min.fixture",321,"diagnostic with a legal result");return 23;}
    return -17;
}
integer_min_box* integer_min_box_get(void){static integer_min_box box={7};return &box;}
int integer_min_box_lookup(integer_min_box* box,int value){assert(box);++calls;return value;}
int integer_min_box_count(integer_min_box* box){assert(box);++calls;return box->value;}
void integer_min_begin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));calls=0;xrtMemDebugSnapshot(&before);}
int integer_min_calls(void){return calls;}
int integer_min_allocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.ReallocCount==before.ReallocCount);return (int)(after.AllocCount-before.AllocCount);}
void integer_min_reset(void){assert(xrtMemDebugFailAfter(-1));}

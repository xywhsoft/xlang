/* The returned address is the XRT allocation itself. No library adapter or
 * C-string owner participates. Counters measure actual runtime allocations. */
#include "xruntime_decl.h"
void xl6ForeignCallbackFail(void);
#include <stdint.h>
#include <string.h>
#include <stdio.h>
bool owned_verify(bool value,int64_t line)
{if(!value)fprintf(stderr,"owned string assertion at line %lld\n",(long long)line);return value;}
static char* last;
char* owned_text(void)
{
    char* text=(char*)xrtMalloc(20);
    if(text)memcpy(text,"owned text",11);
    last=text;return text;
}
char* owned_empty(void){last=(char*)xrtMalloc(1);if(last)*last=0;return last;}
char* owned_null(void){return NULL;}
char* owned_optional(void){return NULL;}
char* owned_failed(void)
{
    /* Failure storage deliberately has no terminator. Admission must observe
     * the outcome and release it BEFORE attempting strlen. */
    last=(char*)xrtMalloc(20);if(last)memset(last,0xff,20);
    xrtSetErrorKind(XERR_RANGE);return last;
}
char* owned_failed_without_diagnostic(void)
{
    last=(char*)xrtMalloc(20);if(last)memset(last,0xff,20);
    xl6ForeignCallbackFail();xrtClearError();return last;
}
bool owned_original(const char* text){return text==last;}
bool owned_original_value(xvalue* value)
{xstrview text={0};bool ok=xrtValueGetString(value,&text)&&text.Data==last;
 if(!ok)fprintf(stderr,"owned original=%p observed=%p\n",(void*)last,(void*)text.Data);return ok;}
void owned_reset(void){xrtMemStatsEnable(true);xrtMemStatsReset();}
bool owned_one_allocation(const char* text)
{
    xmemstats stats={0};xrtMemStatsGet(&stats);
    return text==last&&stats.MallocCalls==1&&stats.CallocCalls==0&&
        stats.ReallocCalls==0&&stats.MemDupCalls==0&&stats.FreeCalls==0;
}
int64_t owned_frees(void){xmemstats s={0};xrtMemStatsGet(&s);return s.FreeCalls;}
int64_t owned_allocations(void){xmemstats s={0};xrtMemStatsGet(&s);return s.MallocCalls+s.CallocCalls+s.ReallocCalls+s.MemDupCalls;}
/* Borrowed immortal receiver: this test isolates string-return ownership. */
void* owned_open(void){static int receiver;return &receiver;}
char* owned_member(void* self){return self?owned_text():NULL;}
char* owned_member_get(void* self){return self?owned_text():NULL;}
char* owned_member_null(void* self){(void)self;return NULL;}
char* owned_member_failed(void* self){(void)self;return owned_failed();}

/* The old unannotated return remains borrowed and must still be copied. */
const char* owned_borrowed(void){return "borrowed";}

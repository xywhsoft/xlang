#include <__xl_runtime/xruntime_decl.h>
#include <stdint.h>
#include <string.h>
char* owned_task_text(int mode);
char* owned_task_default_i64(int64_t mode) { return owned_task_text((int)mode); }
char* owned_task_default_label(const char* label) { return xrtStrDup(label); }
char* owned_task_default_pair(int a,unsigned int b)
{
    if(a!=7 || b!=9) { xrtSetErrorKind(XERR_VALUE);return NULL; }
    return xrtStrDup("pair");
}
/* Only synchronous callers touch these counters. The task test reads them
 * after await; the producer has completed that write before publication. */
static int defaults;
static int default_calls;
static bool fail_default;
char* owned_task_default_text(int mode) { ++default_calls;return owned_task_text(mode); }
int owned_task_default_next(void)
{
    ++defaults;
    if (fail_default) xrtSetErrorKind(XERR_RANGE);
    return 7;
}
int owned_task_default_count(void) { return defaults; }
int owned_task_default_calls(void) { return default_calls; }
void owned_task_default_fail(bool fail) { fail_default=fail; }
static uint32_t float_bits(int kind)
{ return kind==0 ? UINT32_C(0x80000000) : kind==1 ? UINT32_C(0x7fc12345) : UINT32_C(0x7f800000); }
static uint64_t double_bits(int kind)
{ return kind==0 ? UINT64_C(0x8000000000000000) : kind==1 ? UINT64_C(0x7ff8123456789abc) : UINT64_C(0x7ff0000000000000); }
float owned_task_float(int kind) { uint32_t bits=float_bits(kind);float v;memcpy(&v,&bits,sizeof(v));return v; }
double owned_task_double(int kind) { uint64_t bits=double_bits(kind);double v;memcpy(&v,&bits,sizeof(v));return v; }
bool owned_task_float_check(float v,int kind) { uint32_t bits;memcpy(&bits,&v,sizeof(v));return bits==float_bits(kind); }
bool owned_task_double_check(double v,int kind) { uint64_t bits;memcpy(&bits,&v,sizeof(v));return bits==double_bits(kind); }

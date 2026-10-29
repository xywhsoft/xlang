/* 独立组帧，不调用产品 build；只限制标签，不改变数值文本。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_BUFFER
#define XRT_MODULE_NUMBER_FLOAT
#include "xrt.h"
#include "../fix_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned cases, failures, oom_paths;
static bool emit;

static bool balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount;
}

static bool frame(char* output, size_t capacity, unsigned position, const char* tag, bool padded_length)
{
    const char* tags[] = {"8","9","35","10"};
    char body[256];
    tags[position] = tag;
    int size = snprintf(body,sizeof(body),"%s=%s\x01",tags[2],padded_length ? "abc=xyz" : "0007");
    if (size < 0 || (size_t)size >= sizeof(body)) return false;
    int prefix = snprintf(output,capacity,"%s=FIX.4.4\x01%s=%s%d\x01%s",tags[0],tags[1],
        padded_length ? "00" : "",size,body);
    if (prefix < 0 || (size_t)prefix >= capacity) return false;
    unsigned sum = 0;
    for (int i = 0; i < prefix; ++i) sum += (unsigned char)output[i];
    int tail = snprintf(output+prefix,capacity-(size_t)prefix,"%s=%03u\x01",tags[3],sum&255u);
    return tail > 0 && (size_t)tail < capacity-(size_t)prefix;
}

static bool decoded(xvalue* result, int64 tag, const char* value)
{
    xvalue* fields = xrtValueObjectGet(result,XRT_STR_LITERAL("fields"));
    xvalue* field = xrtValueArrayGet(fields,2);
    int64 number;
    xstrview text;
    return xrtValueCount(fields) == 4 &&
        xrtValueGetInt(xrtValueObjectGet(field,XRT_STR_LITERAL("tag")),&number) && number == tag &&
        xrtValueGetString(xrtValueObjectGet(field,XRT_STR_LITERAL("value")),&text) &&
        text.Size == strlen(value) && memcmp(text.Data,value,text.Size) == 0;
}

static void check(unsigned position, const char* tag, bool padded_length, bool valid)
{
    char message[512];
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    bool ok = frame(message,sizeof(message),position,tag,padded_length);
    if (!ok) { ++cases; ++failures; return; }
    xvalue* result = xfix_parse(message);
    bool parsed = xrtValueType(result) == XVALUE_OBJECT;
    xerrkind kind = xrtErrorKind(xrtGetError());
    ok = parsed == valid && kind == (valid ? 0 : XERR_VALUE);
    if (valid) ok = ok && decoded(result,position == 2 ? (int64)strtoll(tag,NULL,10) : 35,
        padded_length ? "abc=xyz" : "0007");
    xrtValueRelease(result);
    bool validated = xfix_validate(message);
    xerrkind last = xrtErrorKind(xrtGetError());
    ok = balanced(before) && ok && validated == valid && last == 0;
    if (emit) {
        printf("%u\t%d\t%s\t%d\t%s\t",cases,(int)parsed,
            kind == 0 ? "none" : kind == XERR_VALUE ? "value" : "other",(int)validated,
            last == 0 ? "none" : "other");
        for (size_t i = 0; message[i] != 0; ++i) printf("%02x",(unsigned char)message[i]);
        putchar('\n');
    }
    if (!ok && failures < 8) fprintf(stderr,"FIX tag role=%u tag=%s paddedLength=%d expected=%d parsed=%d validate=%d\n",
        position,tag,(int)padded_length,(int)valid,(int)parsed,(int)validated);
    ++cases; if (!ok) ++failures;
}

/* 有效与畸形标签都扫描实际分配，解除故障后立即解析同一有效帧。 */
static bool allocation_path(bool predicate, bool valid)
{
    char message[512], good[512];
    if (!frame(message,sizeof(message),2,valid ? "35" : "035",true) ||
        !frame(good,sizeof(good),2,"35",false)) return false;
    bool completed = false, injected = false;
    for (unsigned point = 0; point < 128; ++point) {
        xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        if (!xrtMemDebugFailAfter(point)) return false;
        xvalue* result = predicate ? NULL : xfix_parse(message);
        bool success = predicate ? xfix_validate(message) : xrtValueType(result) == XVALUE_OBJECT;
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        xerrkind kind = xrtErrorKind(xrtGetError());
        bool ok = triggered ? (!success && kind == XERR_MEMORY) :
            (success == valid && kind == (!valid && !predicate ? XERR_VALUE : 0));
        injected = injected || triggered;
        xrtValueRelease(result);
        result = xfix_parse(good);
        ok = ok && xrtValueType(result) == XVALUE_OBJECT && xrtGetError() == NULL && decoded(result,35,"0007");
        xrtValueRelease(result);
        if (!balanced(before) || !ok) return false;
        if (!triggered) { completed = true; break; }
    }
    return completed && injected;
}

int main(int argc, char** argv)
{
    static const char* canonical[] = {"8","9","35","10"};
    static const char* invalid[] = {"","0","00","-1","+8"," 8","8 ","8a","8\x01",
        "9223372036854775808","18446744073709551615","\xff"};
    static const char* custom[] = {"1","11","35","58","9999","2147483648","9223372036854775807"};
    bool grammar = argc == 2 && strcmp(argv[1],"--grammar") == 0;
    emit = argc == 2 && strcmp(argv[1],"--records") == 0;
    if (argc != 1 && !grammar && !emit) return 2;
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE,"test.fix",0,"warm XRT error slot"); xrtClearError();
    for (unsigned padding = 0; padding <= 32; ++padding) for (unsigned role = 0; role < 4; ++role) {
        char tag[64];
        memset(tag,'0',padding); strcpy(tag+padding,canonical[role]);
        for (unsigned style = 0; style < 2; ++style) check(role,tag,style != 0,padding == 0);
    }
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
        for (unsigned role = 0; role < 4; ++role)
            for (unsigned style = 0; style < 2; ++style) check(role,invalid[i],style != 0,false);
    for (size_t i = 0; i < sizeof(custom)/sizeof(custom[0]); ++i)
        for (unsigned style = 0; style < 2; ++style) check(2,custom[i],style != 0,true);
    if (!grammar && !emit) for (unsigned predicate = 0; predicate < 2; ++predicate)
        for (unsigned valid = 0; valid < 2; ++valid) {
            ++oom_paths; if (!allocation_path(predicate != 0,valid != 0)) ++failures;
        }
    if (!emit) printf("fix tag grammar: %u cases, %u OOM paths, %u failures\n",cases,oom_paths,failures);
    return failures != 0 || ferror(stdout);
}

#include "../fix_native.c"

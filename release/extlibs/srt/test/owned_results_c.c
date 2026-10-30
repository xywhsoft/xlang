#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/srt_native.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { if (failures<8) { \
    fprintf(stderr,"srt owned line %d: %s\n",__LINE__,#c); } ++failures; } } while (0)
static const char source[]="1\n00:00:00,000 --> 00:00:01,000\nHi\n\n";
static const char web_source[]="WEBVTT\n\nintro\n00:00:00.500 --> 00:00:01.000 align:start\nHello\n\n";
static const char* expected[]={"00:00:00,000","2562047788015:12:55,807","",source,"WEBVTT\n\n",web_source};
static const unsigned native_budgets[]={1,1,1,3,2,8};

static char* invoke(unsigned operation, xvalue* empty, xvalue* plain, xvalue* web)
{
    switch (operation) {
    case 0: return xsrt_format_time(0);
    case 1: return xsrt_format_time(INT64_MAX);
    case 2: return xsrt_build(empty);
    case 3: return xsrt_build(plain);
    case 4: return xsrt_build_vtt(empty);
    default: return xsrt_build_vtt(web);
    }
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    CHECK(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
    CHECK(after.OverflowCount==before->OverflowCount && after.UnderflowCount==before->UnderflowCount);
}

int main(int argc, char** argv)
{
    bool costs=argc==2 && strcmp(argv[1],"--costs")==0;
    if (argc!=1 && !costs) return 2;
    CHECK(xrtMemDebugEnabled());
    xrtSetErrorInfo(XERR_STATE,"test.srt",0,"warm error slot"); xrtClearError();
    xmemdebugsnapshot initial;
    xrtMemDebugSnapshot(&initial);
    xvalue *empty=xsrt_parse(""), *plain=xsrt_parse(source), *web=xsrt_parse_vtt(web_source);
    CHECK(empty && plain && web && !xrtGetError());
    char* held[6]={0};
    for (unsigned operation=0; operation<6; ++operation) {
        unsigned needed=32;
        for (unsigned budget=0; budget<32; ++budget) {
            xmemdebugsnapshot before;
            xrtClearError(); xrtMemDebugSnapshot(&before);
            CHECK(xrtMemDebugFailAfter(budget));
            char* result=invoke(operation,empty,plain,web);
            bool triggered=xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) CHECK(!result && xrtErrorKind(xrtGetError())==XERR_MEMORY);
            else CHECK(result && !triggered && !xrtGetError() && strcmp(result,expected[operation])==0);
            xrtFree(result); balanced(&before);
            result=invoke(operation,empty,plain,web);
            CHECK(result && !xrtGetError() && strcmp(result,expected[operation])==0);
            xrtFree(result); balanced(&before);
            if (!triggered) {needed=budget; break;}
        }
        CHECK(needed==native_budgets[operation]);
        if (costs) printf("%u\t%u\n",operation,needed);
        held[operation]=invoke(operation,empty,plain,web);
        CHECK(held[operation] && !xrtGetError());
    }
    // 所有文本都独立拥有；释放输入与连续调用不能让旧输出失效。
    xrtValueRelease(web); xrtValueRelease(plain); xrtValueRelease(empty);
    for (unsigned i=0; i<6; ++i) {
        CHECK(held[i] && strcmp(held[i],expected[i])==0);
        for (unsigned j=0; j<i; ++j) CHECK(held[i]!=held[j]);
    }
    for (unsigned i=0; i<6; ++i) xrtFree(held[i]);
    balanced(&initial);
    if (failures) return 1;
    if (!costs) puts("srt C owned results: 22 budgets, PASS");
    return 0;
}

#include "../srt_native.c"

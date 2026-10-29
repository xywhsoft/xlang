/* 同时验证 checked 与装箱入口；记录交由 Python 任意精度整数独立核对。 */
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/money_native.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned failures, cases;
static bool records;
#define CHECK(expr) do { if (!(expr)) { if (failures < 8) { \
    fprintf(stderr,"money boxed line %d: %s\n",__LINE__,#expr); } ++failures; } } while (0)

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    CHECK(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount);
    CHECK(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static int checked(unsigned op, int64_t a, int64_t b, int64_t d, double rate, int64_t* out)
{
    switch (op) {
    case 0: return xm_add_checked(a,b,out);
    case 1: return xm_sub_checked(a,b,out);
    case 2: return xm_mul_checked(a,b,out);
    case 3: return xm_scale_checked(a,b,d,out);
    default: return xm_rate_checked(a,rate,out);
    }
}

static xvalue* boxed(unsigned op, int64_t a, int64_t b, int64_t d, double rate)
{
    switch (op) {
    case 0: return xm_add(a,b);
    case 1: return xm_sub(a,b);
    case 2: return xm_mul(a,b);
    case 3: return xm_scale(a,b,d);
    default: return xm_rate(a,rate);
    }
}

static void verify(unsigned op, int64_t a, int64_t b, int64_t d, double rate)
{
    const int64_t sentinel = INT64_C(0x123456789abcdef);
    int64_t expected = sentinel, actual = sentinel;
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    int ok = checked(op,a,b,d,rate,&expected);
    xerrkind error = xrtErrorKind(xrtGetError());
    CHECK(ok ? error == 0 : (expected == sentinel && (error == XERR_RANGE || error == XERR_VALUE)));
    /* 成功调用必须主动清除旧错误，不能只在空错误槽下成立。 */
    xrtSetErrorInfo(XERR_STATE,"test.money",0,"stale error");
    xvalue* value = boxed(op,a,b,d,rate);
    CHECK(xrtErrorKind(xrtGetError()) == error);
    if (ok) {
        CHECK(value && xrtValueGetInt(value,&actual) && actual == expected);
    } else CHECK(value && xrtValueType(value) == XVALUE_NULL);
    if (records) {
        printf("%u\t%u\t%s\t",cases,op,ok ? "ok" : error == XERR_RANGE ? "range" : "value");
        if (ok) printf("%lld\n",(long long)actual); else puts("-");
    }
    xrtValueRelease(value); balanced(&before); ++cases;
}

static int64_t random_int(uint64_t* state)
{
    *state = *state * UINT64_C(6364136223846793005) + 1;
    int64_t value = (int64_t)(*state >> 1);
    return *state & 4 ? -value : value;
}

static void corpus(void)
{
    static const int64_t triples[][3] = {
        {7,8,2}, {INT64_MIN,0,1}, {INT64_MAX,1,1}, {INT64_MIN,-1,-1},
        {INT64_MAX,2,2}, {INT64_MIN,1,-1}, {INT64_MIN,INT64_MIN,INT64_MIN},
        {INT64_MAX,INT64_MAX,INT64_MAX}, {1,1,0}, {1,1,2}, {-1,1,2},
        {1,-1,2}, {3,1,-2}, {0,0,INT64_MIN}, {INT64_MIN,INT64_MIN,0}, {1,INT64_MAX,INT64_MIN}
    };
    uint64_t state = UINT64_C(0x9e3779b97f4a7c15);
    for (unsigned i=0; i<144; ++i) {
        int64_t a,b,d;
        if (i<16) {a=triples[i][0]; b=triples[i][1]; d=triples[i][2];}
        else {a=random_int(&state); b=random_int(&state); d=random_int(&state); if (i%17==0) d=0;}
        for (unsigned op=0; op<4; ++op) verify(op,a,b,d,0);
    }
    static const int64_t amounts[] = {-125,-1,0,1,125};
    static const double rates[] = {-2,-1.5,-1,-0.5,0,0.5,1,1.5,2,NAN,INFINITY,-INFINITY,DBL_MAX};
    for (unsigned i=0; i<5; ++i) for (unsigned r=0; r<13; ++r) verify(4,amounts[i],0,0,rates[r]);
    static const double edge_rates[] = {-1,0,1,2};
    static const int64_t edge_amounts[] = {INT64_MIN,INT64_MIN+1,INT64_MAX-1,INT64_MAX};
    for (unsigned i=0; i<4; ++i) for (unsigned r=0; r<4; ++r)
        verify(4,edge_amounts[i],0,0,edge_rates[r]);
}

static void allocation_paths(void)
{
    const int64_t expected[] = {15,-1,56,28,11};
    for (unsigned op=0; op<5; ++op) {
        unsigned faults=0;
        int64_t actual;
        CHECK(!checked(op,7,8,2,1.5,NULL) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        for (unsigned budget=0; budget<8; ++budget) {
            xmemdebugsnapshot before;
            xrtClearError(); xrtMemDebugSnapshot(&before);
            CHECK(xrtMemDebugFailAfter(budget));
            xvalue* value = boxed(op,7,8,2,1.5);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {++faults; CHECK(!value && xrtErrorKind(xrtGetError()) == XERR_MEMORY);}
            else CHECK(value && xrtValueGetInt(value,&actual) && actual == expected[op] && !xrtGetError());
            xrtValueRelease(value); balanced(&before);
            value = boxed(op,7,8,2,1.5);
            CHECK(value && xrtValueGetInt(value,&actual) && actual == expected[op] && !xrtGetError());
            xrtValueRelease(value); balanced(&before);
            if (!triggered) break;
        }
        CHECK(faults > 0);
    }
}

int main(int argc, char** argv)
{
    records = argc==2 && strcmp(argv[1],"--records")==0;
    if (argc!=1 && !records) return 2;
    CHECK(xrtMemDebugEnabled());
    xrtSetErrorInfo(XERR_STATE,"test.money",0,"warm error slot"); xrtClearError();
    corpus();
    if (!records) allocation_paths();
    CHECK(cases == 657);
    if (failures) {fprintf(stderr,"money boxed: %u failed checks\n",failures); return 1;}
    if (!records) puts("money boxed: 657 cases, 5 allocation paths, PASS");
    return failures || ferror(stdout) ? 1 : 0;
}

#ifndef MONEY_TEST_EXTERNAL
#include "../money_native.c"
#endif

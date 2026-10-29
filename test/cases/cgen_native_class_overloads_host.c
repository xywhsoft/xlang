#include <stdlib.h>
#include <string.h>

typedef struct NativeScore {
    long long value;
} NativeScore;

static long long gClosed = 0;
static int gExitCheckRegistered = 0;

static void score_verify_exit(void)
{
    if ( gClosed != 1 ) {
        abort();
    }
}

void* score_open(long long seed)
{
    NativeScore* p = (NativeScore*)malloc(sizeof(NativeScore));
    if ( p == 0 ) {
        exit(2);
    }
    if ( !gExitCheckRegistered ) {
        if ( atexit(score_verify_exit) != 0 ) {
            free(p);
            exit(3);
        }
        gExitCheckRegistered = 1;
    }
    p->value = seed;
    return p;
}

long long score_close(void* self)
{
    free(self);
    gClosed += 1;
    return 0;
}

long long score_add_i(void* self, long long value)
{
    NativeScore* p = (NativeScore*)self;
    p->value += value;
    return p->value;
}

long long score_add_s(void* self, const char* text)
{
    NativeScore* p = (NativeScore*)self;
    p->value += (long long)strlen(text);
    return p->value;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}

long long score_closed(void)
{
    return gClosed;
}

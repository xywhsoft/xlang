#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
/* The generated module owns its declaration closure. Only declare the real
 * allocator before interception; preincluding all XRT types conflicts with
 * otherwise self-contained generated modules. */
extern void* xrtMalloc(size_t size);
extern void xrtFree(void* pointer);

static void* tracked_malloc(size_t size);
static void tracked_free(void* pointer);
#if defined(XLIR_ALLOCATION_MEMBERSHIP) || defined(XLIR_ALLOCATION_BINARY)
struct xstrview;
extern char* xrtStrDup(const char* text);
extern char* xrtStrDupN(const char* text, size_t size);
extern char* xrtStrConcat(struct xstrview left, struct xstrview right);
static char* tracked_string_dup(const char* text);
static char* tracked_string_dupn(const char* text, size_t size);
static char* tracked_string_concat(struct xstrview left, struct xstrview right);
#define xrtStrDup tracked_string_dup
#define xrtStrDupN tracked_string_dupn
#define xrtStrConcat tracked_string_concat
#endif
/* Instrument generated allocations without changing production code or XRT.
 * Foreign runtime allocations may be freed here but are not counted. */
#define xrtMalloc tracked_malloc
#define xrtFree tracked_free
#if defined(XLIR_ALLOCATION_NULL_COMPARE)
    #define XLIR_GENERATED_MODULE "build/test/cgen_function_null_compare.c"
#elif defined(XLIR_ALLOCATION_MEMBERSHIP)
    #define XLIR_GENERATED_MODULE "build/test/xlir_membership_operand_lifetime.c"
#elif defined(XLIR_ALLOCATION_BINARY)
    #define XLIR_GENERATED_MODULE "build/test/xlir_binary_operand_lifetime.c"
#elif !defined(XLIR_GENERATED_MODULE)
    #define XLIR_GENERATED_MODULE "build/test/xlir_named_callable_owner_lifetime.c"
#endif
#include XLIR_GENERATED_MODULE
#undef xrtMalloc
#undef xrtFree
#if defined(XLIR_ALLOCATION_MEMBERSHIP) || defined(XLIR_ALLOCATION_BINARY)
#undef xrtStrDup
#undef xrtStrDupN
#undef xrtStrConcat
#endif

static void* tracked[32768];
static const char* tracked_source[32768];
static size_t allocations, releases, live;

static void* tracked_record(void* result, const char* source)
{
    size_t i;
    if (result == NULL) return NULL;
    for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]); ++i) {
        if (tracked[i] == NULL) {
            tracked[i] = result; ++allocations; ++live;
            tracked_source[i] = source;
            return result;
        }
    }
    fputs("generated allocation tracking capacity exceeded\n", stderr);
    abort();
}

static void* tracked_malloc(size_t size)
{
    return tracked_record(xrtMalloc(size), "malloc");
}

#if defined(XLIR_ALLOCATION_MEMBERSHIP) || defined(XLIR_ALLOCATION_BINARY)
/* String producers allocate inside XRT, outside the generated malloc hook.
 * Track their owned results too; otherwise a leaked needle would be invisible. */
static char* tracked_string_dup(const char* text)
{
    return (char*)tracked_record(xrtStrDup(text), "string-dup");
}
static char* tracked_string_dupn(const char* text, size_t size)
{
    return (char*)tracked_record(xrtStrDupN(text, size), "string-dupn");
}
static char* tracked_string_concat(struct xstrview left, struct xstrview right)
{
    return (char*)tracked_record(xrtStrConcat(left, right), "string-concat");
}
#endif

static void tracked_free(void* pointer)
{
    size_t i;
    if (pointer != NULL) {
        for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]); ++i) {
            if (tracked[i] == pointer) {
                tracked[i] = NULL; ++releases; --live;
                break;
            }
        }
    }
    xrtFree(pointer);
}

int main(void)
{
#if defined(XLIR_GENERATED_MODULE_USER_ENTRY)
    if (xlang_main(NULL) != 0) {
        const xerror* error = xrtGetError();
        fprintf(stderr, "allocation probe entry failed: %s\n",
            error != NULL ? xrtErrorMessage(error) : "no runtime error");
        return 1;
    }
#else
    if (!xlir_module_init()) return 1;
#endif
#ifndef XLIR_MIN_ALLOCATIONS
    #define XLIR_MIN_ALLOCATIONS 4800
#endif
    if (allocations < XLIR_MIN_ALLOCATIONS || live != 0 || releases != allocations) {
        size_t i, shown = 0;
        fprintf(stderr, "generated allocation accounting: created=%llu freed=%llu live=%llu\n",
            (unsigned long long)allocations, (unsigned long long)releases,
            (unsigned long long)live);
        for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]) && shown < 8; ++i) {
            if (tracked[i] == NULL) continue;
            fprintf(stderr, "unreleased %s allocation\n", tracked_source[i]);
            ++shown;
        }
        return 1;
    }
    printf("generated allocations balanced: %llu\n", (unsigned long long)allocations);
    return 0;
}

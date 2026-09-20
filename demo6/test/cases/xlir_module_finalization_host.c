#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void* xrtMalloc(size_t size);
extern void xrtFree(void* pointer);
struct xstrview;
extern char* xrtStrDup(const char* text);
extern char* xrtStrConcat(struct xstrview left, struct xstrview right);
static void* tracked_malloc(size_t size);
static void tracked_free(void* pointer);
static char* tracked_dup(const char* text);
static char* tracked_concat(struct xstrview left, struct xstrview right);
#define xrtMalloc tracked_malloc
#define xrtFree tracked_free
#define xrtStrDup tracked_dup
#define xrtStrConcat tracked_concat
#include "../../build/test/xlir_module_finalization.c"
#undef xrtMalloc
#undef xrtFree
#undef xrtStrDup
#undef xrtStrConcat

static void* tracked[128];
static size_t allocations, releases, live;
static void* track(void* pointer)
{
    size_t i;
    if (pointer == NULL) return NULL;
    for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]); ++i) {
        if (tracked[i] == NULL) {
            tracked[i] = pointer; ++allocations; ++live;
            return pointer;
        }
    }
    abort();
}
static void* tracked_malloc(size_t size) { return track(xrtMalloc(size)); }
static char* tracked_dup(const char* text) { return (char*)track(xrtStrDup(text)); }
static char* tracked_concat(struct xstrview left, struct xstrview right)
{ return (char*)track(xrtStrConcat(left, right)); }
static void tracked_free(void* pointer)
{
    size_t i;
    if (pointer != NULL) {
        for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]); ++i) {
            if (tracked[i] == pointer) {
                tracked[i] = NULL; --live; ++releases;
                break;
            }
        }
    }
    xrtFree(pointer);
}

static int created_count, destroyed_count, destroy_order;
static bool fail_initialization, reentry_checked, init_reentry_checked;
static int host_failure;

static int run_main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if (arguments == NULL) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
}

void module_created(int64_t id)
{
    (void)id;
    ++created_count;
    if (!init_reentry_checked) {
        int before = created_count;
        init_reentry_checked = true;
        /* A host callback cannot unload or restart a half-built module.
         * Internal initializer failure uses its own cleanup entry. */
        xl_script_module_unit();
        if (xl_script_module_init(NULL) != 0 || created_count != before || destroyed_count != 0)
            host_failure = 1;
    }
}

bool module_should_fail(void) { return fail_initialization; }

void module_destroyed(int64_t id)
{
    xerror* diagnostic;
    ++destroyed_count;
    destroy_order = destroy_order * 10 + (int)id;
    if (id == 5 && !reentry_checked) {
        int before = destroyed_count;
        reentry_checked = true;
        xl_script_module_unit();
        if (destroyed_count != before || xl_script_module_init(NULL) != 0)
            host_failure = 1;
    }
    /* A native destructor may leave a diagnostic. It cannot replace the
     * exception that caused module finalization. */
    diagnostic = xrtErrorCreate(15, "module-test", (int)id, "destructor diagnostic");
    if (diagnostic == NULL) host_failure = 1;
    xrtSetErrorTake(diagnostic);
}

static int finish_cycle(int expected_created, bool with_error)
{
    xerror* pending = with_error ? xrtErrorCreate(15, "module-test", 1, "pending host failure") : NULL;
    const xerror* observed;
    if (with_error && pending == NULL) return 0;
    xrtSetErrorTake(pending);
    xl_script_module_unit();
    if (host_failure || !reentry_checked || !init_reentry_checked || destroy_order != 564213 ||
        created_count != expected_created || destroyed_count != created_count ||
        (with_error ? xrtGetError() != pending : xrtGetError() == NULL || xrtErrorCode(xrtGetError()) != 5) ||
        live != 0 || allocations != releases) return 0;
    observed = xrtGetError();
    xl_script_module_unit();
    if (destroyed_count != created_count || xrtGetError() != observed) return 0;
    xrtClearError();
    return 1;
}

int main(void)
{
    int i;
    /* Unload before initialization is a no-op. */
    xl_script_module_unit();
    if (!xl_script_module_init(NULL) || created_count != 6 || destroyed_count != 0 ||
        !xl_script_module_init(NULL) || run_main() != 0 || run_main() != 0 ||
        created_count != 6 || destroyed_count != 0 || !finish_cycle(6, true)) {
        fprintf(stderr, "module normal/reentrant finalization failed: created=%d destroyed=%d order=%d reentry=%d host=%d error=%s\n",
            created_count, destroyed_count, destroy_order, reentry_checked, host_failure,
            xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
        return 1;
    }
    /* A completed unload permits a fresh initialization of module storage. */
    destroy_order = 0;
    if (!xl_script_module_init(NULL) || run_main() != 0 || !finish_cycle(12, false)) {
        fputs("module reinitialization failed\n", stderr);
        return 2;
    }
    fail_initialization = true;
    destroy_order = 0;
    if (xl_script_module_init(NULL) != 0 || created_count != 15 ||
        destroyed_count != 15 || destroy_order != 213 || live != 0 || xrtGetError() == NULL ||
        strcmp(xrtErrorMessage(xrtGetError()), "expected module initialization failure") != 0) {
        fprintf(stderr, "module partial initialization failed: created=%d destroyed=%d order=%d\n",
            created_count, destroyed_count, destroy_order);
        return 3;
    }
    xl_script_module_unit();
    if (destroyed_count != 15 || destroy_order != 213) return 4;
    xrtClearError();
    fail_initialization = false;
    destroy_order = 0;
    if (!xl_script_module_init(NULL) || run_main() != 0 || !finish_cycle(21, true)) return 5;
    for (i = 0; i < 200; ++i) {
        destroy_order = 0;
        if (!xl_script_module_init(NULL) || run_main() != 0 ||
            !finish_cycle(27 + i * 6, (i & 1) != 0)) return 6;
    }
    printf("module allocations balanced: %llu; language objects balanced: %d\n",
        (unsigned long long)allocations, destroyed_count);
    puts("module finalization: reverse global/static/collection/closure ownership, partial failure, original error, initialization/finalization reentry and retry ok");
    return 0;
}

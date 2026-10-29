/* The generated C and canonical target binding are preincluded. Only the
 * public foreign-entry boundary is fault injected; language scopes stay real. */
#include <assert.h>
#include <stdio.h>
#undef xl6ForeignCallBegin
#undef xrtStrDup
#undef xrtFree
extern bool xl6ForeignCallBegin(xl6_foreign_error_scope*);
extern char* xrtStrDup(const char*);
extern void xrtFree(void*);
extern bool xrtMemDebugEnable(bool);
extern uint64_t prepare_live_count(void), prepare_live_bytes(void);
static int fault;
static unsigned invocations;
/* One caller-local copy backs the borrowed argument; two independent copies
 * are transferred. The local owner must outlive both transferred inputs. */
static char* copies[3];
static unsigned copy_count, release_count;
static int cleanup_fault;
static int tracking;
static xerror* injected;
char* prepare_dup(const char* text)
{ char* copy = xrtStrDup(text); if (tracking) { assert(copy_count < 3); copies[copy_count++] = copy; } return copy; }
void prepare_free(void* value)
{
    if (tracking && value != NULL && (value == copies[0] || value == copies[1] || value == copies[2])) {
        assert(release_count < 3 && value == copies[2 - release_count]);
        ++release_count; xrtFree(value);
        if (cleanup_fault && release_count < 3) xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "cleanup", 502, "later failure"));
    } else xrtFree(value);
}
bool prepare_begin(xl6_foreign_error_scope* scope)
{
    if (!fault) return xl6ForeignCallBegin(scope);
    if (fault == 1) {
        injected = xrtErrorCreate(XERR_MEMORY, "preparation", 501, "entry failed");
        assert(injected); xrtSetErrorTake(injected);
    }
    return false;
}
int64_t prepare_consume(char* left, const char* borrowed, char* right)
{
    int64_t length = (int64_t)(strlen(left) + strlen(borrowed) + strlen(right));
    assert(left != right && left != borrowed && right != borrowed);
    ++invocations; prepare_free(right); prepare_free(left); return length;
}
int main(void)
{
    unsigned failures = 0, mode, iteration, late;
    assert(xrtMemDebugEnable(true)); assert(xlir_module_init());
    tracking = 1;
    for (iteration = 0; iteration < 100; ++iteration)
    for (late = 0; late < 2; ++late)
    for (mode = 0; mode < 3; ++mode) {
        uint64_t beforeCount, beforeBytes, afterCount, afterBytes; xlir_result_i64 result;
        xl6_error_scope scope = {0}; xl6_error_result outcome;
        unsigned calls = invocations;
        beforeCount = prepare_live_count(); beforeBytes = prepare_live_bytes();
        copy_count = release_count = 0; copies[0] = copies[1] = copies[2] = NULL; injected = NULL;
        fault = (int)mode; cleanup_fault = mode != 0 && late;
        assert(xl6ErrorScopeBegin(&scope));
        result = XLIR_PREPARE_CASE("abc");
        if (!result.ok) xl6ErrorScopeCaptureFailure(&scope);
        outcome = xl6ErrorScopeEnd(&scope);
        assert(result.ok == (mode == 0));
        assert(invocations - calls == (mode == 0 ? 1u : 0u));
        assert(outcome.ok == result.ok);
        assert(copy_count == 3 && release_count == 3);
        if (!mode) assert(result.value == 9);
        assert(outcome.error == (mode == 1 ? injected : NULL));
        xrtErrorFree(outcome.error); xrtClearError();
        afterCount = prepare_live_count(); afterBytes = prepare_live_bytes();
        if (beforeCount != afterCount || beforeBytes != afterBytes) {
            ++failures;
            printf("prepare mode=%u leaked owners=%llu bytes=%llu\n", mode,
                (unsigned long long)(afterCount - beforeCount),
                (unsigned long long)(afterBytes - beforeBytes));
        }
    }
    fault = cleanup_fault = tracking = 0; xlir_module_fini();
    printf("foreign preparation: 600 outcomes, reverse-owned/borrowed/first-error/false-NULL, leaks=%u\n", failures);
    return failures ? 1 : 0;
}

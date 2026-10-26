#include <__xl_runtime/xruntime_decl.h>
#include <stdint.h>

static int observations;
const char* borrowed_checked_text(void) { return "borrowed"; }
const char* borrowed_checked_variadic(int mode, ...) { (void)mode; return "borrowed"; }
const char* borrowed_checked_missing(void) { return NULL; }
void xl6ForeignCallbackFail(void);
const char* borrowed_checked_failed(bool silent)
{
    xrtSetErrorKind(XERR_RANGE);
    if (silent) { xl6ForeignCallbackFail(); xrtClearError(); }
    /* A failed producer does not promise a readable payload. Admission must
     * observe its outcome before either strlen or the compatibility copy. */
    return (const char*)(uintptr_t)1;
}
void* borrowed_checked_open(void) { static int receiver; return &receiver; }
const char* borrowed_checked_member(void* self) { (void)self; return "borrowed"; }
const char* borrowed_checked_get(void* self) { (void)self; return "borrowed"; }
void borrowed_checked_observe(const char* value) { (void)value; ++observations; }
int borrowed_checked_observations(void) { return observations; }
bool borrowed_checked_debug(void) { return xrtMemDebugEnabled() || xrtMemDebugEnable(true); }
void borrowed_checked_fail(int64_t prefix) { xrtMemDebugFailAfter((uint64_t)prefix); }
void borrowed_checked_clear(void) { xrtMemDebugFailClear(); xrtClearError(); }
int64_t borrowed_checked_live(void) { xmemdebugsnapshot snapshot={0}; xrtMemDebugSnapshot(&snapshot); return snapshot.LiveCount; }

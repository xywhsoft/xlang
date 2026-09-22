#include "../../build/test/__xl_runtime/xruntime_decl.h"
#include <assert.h>
#include <string.h>

static int64_t continued;
static int32_t (*saved)(void);

static void consume_diagnostic(void)
{
    const xerror* error = xrtGetError();
    assert(error != NULL);
    assert(strcmp(xrtErrorOperation(error), "callback") == 0);
    assert(xrtErrorLine(error) > 0);
    ++continued;
    /* Both replacement and clearing must leave the original pending error. */
    xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "native", 3, "replacement diagnostic"));
    xrtClearError();
}

void foreign_raw_int(int32_t (*callback)(void))
{
    assert(callback() == 0);
    consume_diagnostic();
}
void foreign_raw_bool(bool (*callback)(void))
{
    assert(!callback());
    consume_diagnostic();
}
void foreign_raw_double(double (*callback)(void))
{
    assert(callback() == 0.0);
    consume_diagnostic();
}
void foreign_raw_pointer(void* (*callback)(void))
{
    assert(callback() == NULL);
    consume_diagnostic();
}
void foreign_raw_void(void (*callback)(void))
{
    callback();
    consume_diagnostic();
}
void foreign_raw_save(int32_t (*callback)(void)) { saved = callback; }
void foreign_raw_saved(void) { foreign_raw_int(saved); }
void foreign_raw_success(int32_t (*callback)(void))
{
    assert(callback() == 42);
    assert(xrtGetError() == NULL);
}
void foreign_raw_pair(int32_t (*first)(void), int32_t (*second)(void))
{
    foreign_raw_int(first);
    foreign_raw_int(second);
}
int64_t foreign_raw_count(void) { return continued; }

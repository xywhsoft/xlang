#include <assert.h>
#ifndef XLIR_STORAGE_EXCHANGE_SOURCE
#define XLIR_STORAGE_EXCHANGE_SOURCE "../../build/test/xlir_storage_exchange.c"
#endif
#include XLIR_STORAGE_EXCHANGE_SOURCE
static unsigned dropped[11], observed, caught;
static int failureMode;
void exchange_drop(int64_t id)
{
    assert(id > 0 && id <= 10 && ++dropped[id] == 1);
    if (id == 10 && failureMode) {
        xl6_error_result failed = {false, NULL};
        if (failureMode == 1) {
            failed.error = xrtErrorCreate(XERR_STATE, "storage-exchange", 77, "old payload cleanup failed");
            assert(failed.error);
        }
        xl6ErrorScopePropagateFailure(failed);
    }
}
void exchange_observe(int64_t id) { assert(id == 1); ++observed; }
void exchange_caught(int64_t code) { assert(code == (failureMode == 1 ? 77 : 0)); ++caught; }
int storage_exchange_init(void) { return xlir_module_init(); }
void storage_exchange_fini(void) { xlir_module_fini(); }
void storage_exchange_cycle(int kind, int mode)
{
    memset(dropped, 0, sizeof(dropped)); observed = caught = 0; failureMode = mode;
    assert(exchange_case(kind) == (mode ? 23 : 0));
    assert(observed == 1 && caught == (unsigned)(mode != 0));
    assert(dropped[10] == 1 && dropped[1] == 1 && dropped[3] == 1);
    assert(!xrtGetError());
}

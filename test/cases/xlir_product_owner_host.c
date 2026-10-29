#include <assert.h>
#ifndef XLIR_PRODUCT_OWNER_SOURCE
#define XLIR_PRODUCT_OWNER_SOURCE "../../build/test/xlir_product_owner.c"
#endif
#include XLIR_PRODUCT_OWNER_SOURCE
extern void product_owner_arm(void);
extern int product_owner_fault_status(void);
static unsigned dropped[21], caught, finalized, indices;
static int firstDropFailure, secondaryFailures, withoutDiagnostic;
static int64_t observed;
static void failure(int64_t code)
{
    xl6_error_result failed = {false, NULL};
    if (!withoutDiagnostic || code == 99) {
        failed.error = xrtErrorCreate(XERR_STATE, "product-owner", code, "injected product owner failure");
        assert(failed.error);
    }
    xl6ErrorScopePropagateFailure(failed);
}
int64_t product_owner_index(int64_t index, int64_t stage)
{
    assert(index == (int64_t)indices++);
    if (stage == 3 && index == 0) product_owner_arm();
    if (stage == index + 1) failure(77);
    return index;
}
void product_owner_drop(int64_t id)
{
    assert(id > 0 && id <= 20 && ++dropped[id] == 1);
    if (id == firstDropFailure) failure(77);
    else if (secondaryFailures && (id == 1 || id == 2)) failure(99);
}
void product_owner_caught(int64_t code) { ++caught; observed = code; }
void product_owner_finally(void) { ++finalized; }
int product_owner_init(void) { return xlir_module_init(); }
void product_owner_fini(void) { xlir_module_fini(); }
void product_owner_cycle(int kind, int stage, int failDrop, int mode)
{
    int64_t result;
    memset(dropped, 0, sizeof(dropped)); caught = finalized = indices = 0; observed = -1;
    firstDropFailure = failDrop; secondaryFailures = kind != 1 && (stage != 0 || failDrop != 0);
    withoutDiagnostic = mode;
    result = xl6_export_product_owner_case(kind, stage);
    if (result != ((stage == 3 ? product_owner_fault_status() : stage || failDrop) ? 23 : 0))
        fprintf(stderr, "product outcome kind=%d stage=%d failDrop=%d mode=%d result=%lld caught=%u code=%lld finally=%u\n",
            kind, stage, failDrop, mode, (long long)result, caught, (long long)observed, finalized);
    assert(result == ((stage == 3 ? product_owner_fault_status() : stage || failDrop) ? 23 : 0));
    assert(finalized == 1 && caught == (unsigned)(result != 0));
    if (caught && stage != 3) assert(observed == (mode ? 0 : 77));
    if (stage == 3) assert(indices == 1 || indices == 2);
    else if (kind <= 1) assert(indices == (unsigned)(stage == 1 ? 1 : 2));
    else assert(indices == 0);
    if (kind != 1) {
        if (dropped[1] != 1 || dropped[2] != 1)
            fprintf(stderr, "product field destruction kind=%d stage=%d mode=%d first=%u second=%u\n",
                kind, stage, mode, dropped[1], dropped[2]);
        assert(dropped[1] == 1 && dropped[2] == 1);
        assert(dropped[10] == (unsigned)(kind != 2) && dropped[20] == (unsigned)(kind != 2));
    }
    assert(!xrtGetError());
}

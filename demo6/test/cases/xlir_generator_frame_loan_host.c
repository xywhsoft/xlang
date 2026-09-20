#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_LOAN_SOURCE
#define XLIR_GENERATOR_LOAN_SOURCE "../../build/test/xlir_generator_frame_loan.c"
#endif
#include XLIR_GENERATOR_LOAN_SOURCE
static int failureMode;
static unsigned drops, catches, finals;
void generator_loan_drop(int64_t id)
{
    assert(id == 7 && ++drops == 1);
    if (failureMode) {
        xl6_error_result failed = {false, NULL};
        if (failureMode == 1) {
            failed.error = xrtErrorCreate(XERR_STATE, "generator-loan", 77, "parameter destructor failed");
            assert(failed.error);
        }
        xl6ErrorScopePropagateFailure(failed);
    }
}
void generator_loan_caught(int64_t code)
{
    assert(failureMode && code == (failureMode == 1 ? 77 : 0)); ++catches;
}
void generator_loan_finally(void) { ++finals; }
int generator_loan_init(void) { return xlir_module_init(); }
void generator_loan_fini(void) { xlir_module_fini(); }
void generator_loan_cycle(int kind, int stop, int failure)
{
    int count = stop ? stop : 3, result;
    drops = catches = finals = 0; failureMode = failure;
    result = generator_loan_case(kind, stop);
    if (result != count + ((kind == 1 && failure) ? 100 : 0))
        fprintf(stderr, "generator loan kind=%d stop=%d failure=%d result=%d expected=%d drops=%u catches=%u finally=%u error=%p\n",
            kind, stop, failure, result, count + ((kind == 1 && failure) ? 100 : 0), drops, catches, finals, (void*)xrtGetError());
    assert(result == count + ((kind == 1 && failure) ? 100 : 0));
    assert(drops == (unsigned)(kind == 1));
    assert(catches == (unsigned)(kind == 1 && failure != 0) && finals == 1);
    assert(!xrtGetError());
}

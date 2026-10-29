#include "xlir_closure_allocation_outcome.c"
#include "xlir_closure_allocation_outcome.h.bind.h"
int closure_outcome_init(void) { return xlir_module_init(); }
void closure_outcome_finish(void) { xlir_module_fini(); }
int closure_outcome_run(int shape, int* nonnull, int* invoked, long long* value)
{
    xlir_result_closure result = {0};
    *nonnull = 0; *invoked = -1; *value = 0;
    if (shape == 9) { *value = (long long)CLOSURE_OUTCOME_9(); return 1; }
    switch (shape) {
    case 0: result = CLOSURE_OUTCOME_0(); break;
    case 1: result = CLOSURE_OUTCOME_1(); break;
    case 2: result = CLOSURE_OUTCOME_2(); break;
    case 3: result = CLOSURE_OUTCOME_3(); break;
    case 4: result = CLOSURE_OUTCOME_4(); break;
    case 5: result = CLOSURE_OUTCOME_5(); break;
    case 6: result = CLOSURE_OUTCOME_6(false); break;
    case 7: result = CLOSURE_OUTCOME_7(); break;
    case 8: result = CLOSURE_OUTCOME_8(); break;
    case 10: result = CLOSURE_OUTCOME_6(true); break;
    case 11: result = CLOSURE_OUTCOME_10(false); break;
    case 12: result = CLOSURE_OUTCOME_10(true); break;
    }
    *nonnull = result.value != NULL;
    if (result.ok && result.value != NULL) {
        int64_t answer = 0;
        *invoked = ((bool (*)(void*, int64_t*))result.value->invoke)(result.value->environment, &answer);
        *value = (long long)answer;
    }
    xlir_closure_release(result.value);
    return result.ok;
}

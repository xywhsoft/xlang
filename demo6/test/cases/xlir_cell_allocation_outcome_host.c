#include "xlir_cell_allocation_outcome.c"
#include "xlir_cell_allocation_outcome.h.bind.h"
static xvalue* cellInput;
static unsigned cellFinalies;
void cell_allocation_finally(void) { ++cellFinalies; }
int cell_outcome_init(void)
{
    if (!xlir_module_init()) return 0;
    cellInput = xrtValueInt(7); return cellInput != NULL;
}
void cell_outcome_finish(void) { xrtValueRelease(cellInput); cellInput = NULL; xlir_module_fini(); }
int cell_outcome_run(int shape, int* nonnull, int* invoked, long long* value, unsigned* finalies)
{
    xlir_result_closure result = {0}; unsigned before = cellFinalies;
    *nonnull = 0; *invoked = -1; *value = 0; *finalies = 0;
    if (shape == 12) {
        xlir_result_i64 caught = CELL_OUTCOME_10();
        *value = (long long)caught.value; *finalies = cellFinalies - before; return caught.ok;
    }
    switch (shape) {
    case 0: result = CELL_OUTCOME_0(); break;
    case 1: result = CELL_OUTCOME_1(); break;
    case 2: result = CELL_OUTCOME_2(); break;
    case 3: result = CELL_OUTCOME_3(false); break;
    case 4: result = CELL_OUTCOME_3(true); break;
    case 5: result = CELL_OUTCOME_4(7); break;
    case 6: result = CELL_OUTCOME_5((xlir_string){"value",5}); break;
    case 7: result = CELL_OUTCOME_6(cellInput); break;
    case 8: result = CELL_OUTCOME_7(7, (xlir_string){"value",5}, cellInput); break;
    case 9: result = CELL_OUTCOME_8((xlir_string){0}); break;
    case 10: result = CELL_OUTCOME_8((xlir_string){"value",5}); break;
    case 11: result = CELL_OUTCOME_9(); break;
    }
    *nonnull = result.value != NULL;
    if (result.ok && result.value) {
        int64_t answer = 0;
        *invoked = ((bool (*)(void*, int64_t*))result.value->invoke)(result.value->environment, &answer);
        *value = (long long)answer;
    }
    xlir_closure_release(result.value); return result.ok;
}

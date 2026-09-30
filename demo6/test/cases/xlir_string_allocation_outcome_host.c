#include <assert.h>
#include "../../build/test/xlir_string_allocation_outcome.c"
#include "../../build/test/xlir_string_allocation_outcome.h.bind.h"
extern xerror* string_outcome_diagnostic(void);
static int defaultMode;
static xvalue* dynamicInput;
const char* string_outcome_default(void)
{
    if (defaultMode) {
        xl6_error_result failure = {false, defaultMode == 1 ? string_outcome_diagnostic() : NULL};
        xl6ErrorScopePropagateFailure(failure); return NULL;
    }
    return "suffix";
}
void string_outcome_prepare(void) { assert(xlir_module_init()); dynamicInput = xrtValueInt(41); assert(dynamicInput); }
void string_outcome_finish(void) { xrtValueRelease(dynamicInput); dynamicInput = NULL; xlir_module_fini(); }
int string_outcome_invoke(int shape, int mode, char** left, char* right, char** output)
{
    xlir_result_string result;
    defaultMode = mode;
    if (shape == 5) {
        /* The unqualified string parameter is borrowed at the C boundary;
         * assignment commits the returned owner only after success. */
        result = STRING_OUTCOME_5(*left, right);
        if (result.ok) {
            xrtFree(*left);
            *left = result.value;
        }
        return result.ok;
    }
    switch (shape) {
    case 0: result = STRING_OUTCOME_0(*left, right); break;
    case 1: result = STRING_OUTCOME_1(*left, right); break;
    case 2: result = STRING_OUTCOME_2(*left, right); break;
    case 3: result = STRING_OUTCOME_3(*left, right); break;
    case 4: assert(!right); result = STRING_OUTCOME_ADAPTER(NULL, *left); break;
    case 6: result = STRING_OUTCOME_6(*left, right); break;
    case 7: result = STRING_OUTCOME_7(*left, right); break;
    case 8: result = STRING_OUTCOME_8(*left, right); break;
    case 9: result = STRING_OUTCOME_9(*left, right); break;
    case 10: result = STRING_OUTCOME_10(*left, right); break;
    case 11: result = STRING_OUTCOME_11(*left, right); break;
    case 12: result = STRING_OUTCOME_12(*left, right); break;
    case 13: result = STRING_OUTCOME_13(*left, right); break;
    case 14: result = STRING_OUTCOME_14(*left, right); break;
    case 15: result = STRING_OUTCOME_15(*left, right, dynamicInput); break;
    case 16: result = STRING_OUTCOME_16(*left, right); break;
    case 17: case 18: result = STRING_OUTCOME_17(*left, right); break;
    case 19: result = STRING_OUTCOME_18(*left, right); break;
    default: assert(0); return 0;
    }
    *left = NULL; *output = result.value; return result.ok;
}

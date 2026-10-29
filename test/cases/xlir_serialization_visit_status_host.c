/* Preinclude the actual visit fixture's generated C. This checks native error
 * ownership independently of the full language/closure/parse execution gate. */
#include <assert.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"

static xerror* diagnostic(xerrkind kind)
{
    xerror* error = xrtErrorCreate(kind, "visit-test", 17, "owned diagnostic");
    assert(error != NULL);
    return error;
}

int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    for (int status = -1; status <= 1; ++status) {
        for (int memoryPrior = 0; memoryPrior <= 1; ++memoryPrior) {
            xl6_serialization_visitor_context context = {0};
            xerror* prior = diagnostic(memoryPrior ? XERR_MEMORY : XERR_VALUE);
            context.PriorError = prior;
            xrtSetErrorTake(diagnostic(XERR_IO));
            XL6_Serialization_Visit_Result result = xl6_serialization_visit_finish(&context, status);
            assert(result.ok && result.value == status && xrtGetError() == prior);
            assert(context.PriorError == NULL && context.CallbackError == NULL);
            xrtClearError();
        }
    }
    {
        xl6_serialization_visitor_context context = {0};
        context.PriorError = diagnostic(XERR_VALUE);
        xerror* failure = diagnostic(XERR_MEMORY);
        xrtSetErrorTake(failure);
        XL6_Serialization_Visit_Result result = xl6_serialization_visit_finish(&context, -1);
        assert(!result.ok && xrtGetError() == failure && context.PriorError == NULL);
        xrtClearError();
    }
    {
        xl6_serialization_visitor_context context = {0};
        context.PriorError = diagnostic(XERR_IO);
        xerror* failure = diagnostic(XERR_VALUE);
        xrtSetErrorTake(failure);
        xl6_serialization_callback_fail(&context);
        xrtSetErrorTake(diagnostic(XERR_MEMORY)); /* Native unwinding must not replace the throw. */
        XL6_Serialization_Visit_Result result = xl6_serialization_visit_finish(&context, -1);
        assert(!result.ok && xrtGetError() == failure);
        assert(context.PriorError == NULL && context.CallbackError == NULL);
        xrtClearError();
    }
    assert(!xl6ProductMemoryEnd());
    xl6ProductMemoryVerify();
    puts("visit status, prior diagnostic, OOM and original callback error ownership balanced");
    return 0;
}

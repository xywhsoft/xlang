#include "xlir_future_aggregate_code_owner_box.h"
#include <assert.h>
__declspec(dllexport) struct xvalue* aggregate_owner_box(const void* input)
{
    const AggregateBoxPayload* payload = input; AggregateBoxHooks* hooks = payload->hooks;
    ++hooks->calls; hooks->during(hooks->context, 0);
    struct xvalue* result = hooks->box(payload->value); assert(result);
    ++hooks->returned; return result;
}
__declspec(dllexport) struct xvalue* aggregate_owner_child_box(const void* input)
{
    const AggregateBoxPayload* payload = input; AggregateBoxHooks* hooks = payload->hooks;
    ++hooks->children; hooks->during(hooks->context, 1);
    struct xvalue* result = hooks->box(payload->value); assert(result);
    ++hooks->returned; return result;
}

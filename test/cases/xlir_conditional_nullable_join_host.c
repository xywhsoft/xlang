#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_conditional_nullable_join.c"
static int live, order, picks, selection;
void nullable_join_open(int64_t id)
{
    int bit = 1 << ((int)id - 1);
    assert(id >= 1 && id <= 2 && !(live & bit)); live |= bit;
}
void nullable_join_drop(int64_t id)
{
    int bit = 1 << ((int)id - 1);
    assert(id >= 1 && id <= 2 && (live & bit)); live &= ~bit; order = order * 10 + (int)id;
}
int64_t nullable_join_pick(int64_t id)
{
    assert(id == (selection ? 1 : 2) && ++picks == 1); return id;
}
void nullable_join_case(int choose)
{
    xl6_error_scope boundary = {0};
    xl6_error_result result;
    int64_t value;
    selection = choose; live = order = picks = 0;
    assert(xrtGetError() == NULL);
    assert(xrtValueType(xlir_xvalue_array_at_i64_or_null(NULL, 0)) == 0);
    assert(xrtValueType(xlir_xvalue_object_get_literal_or_null(xrtValueNull(), xrtStrView("value"))) == 0);
    assert(xrtValueType(xlir_xvalue_object_get_string_or_null(NULL, xrtStrView("value"))) == 0);
    assert(xrtGetError() == NULL && xlir_module_init() && xl6ErrorScopeBegin(&boundary));
    value = nullable_join_probe(choose != 0);
    result = xl6ErrorScopeEnd(&boundary);
    if (!result.ok) fprintf(stderr, "nullable join failure choose=%d code=%d line=%d message=%s\n",
        choose, xrtErrorCode(result.error), xrtErrorLine(result.error), xrtErrorMessage(result.error));
    assert(result.ok && result.error == NULL && value == 41);
    if (live != 0 || order != (choose ? 21 : 1) || picks != 1)
        fprintf(stderr, "nullable join ownership choose=%d live=%d order=%d picks=%d\n", choose, live, order, picks);
    assert(live == 0 && order == (choose ? 21 : 1) && picks == 1);
    xlir_module_fini(); assert(xrtGetError() == NULL);
}

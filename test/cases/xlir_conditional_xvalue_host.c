/* The suite generates this file immediately before compiling this host. */
#include <stdint.h>
#include "../../build/test/xlir_conditional_xvalue_generated.c"

extern bool xrtValueGetInt(const xvalue* value, int64_t* result);

static int has_value(const xvalue* object, int64_t expected)
{
    xvalue* item = xrtValueObjectGet(object, xrtStrView("value"));
    int64_t actual = 0;
    return item != NULL && xrtValueGetInt(item, &actual) && actual == expected;
}

int main(void)
{
    xvalue* yes;
    xvalue* no;
    int ok;
    if ( xlir_module_init() != 1 ) return 1;
    yes = xlir_fn_1(true);
    no = xlir_fn_1(false);
    ok = yes != NULL && no != NULL && has_value(yes, 41) && has_value(no, 42);
    xrtValueRelease(yes);
    xrtValueRelease(no);
    return ok ? 0 : 1;
}

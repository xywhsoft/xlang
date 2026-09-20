#include <assert.h>
#ifndef XLIR_GENERATOR_ITEM_SOURCE
#error XLIR_GENERATOR_ITEM_SOURCE must name the generated module
#endif
#include XLIR_GENERATOR_ITEM_SOURCE

void generator_item_basic_prepare(void)
{
    assert(xlir_module_init() && !xrtGetError());
}

void generator_item_basic_case(void)
{
    xl6_error_scope scope = {0};
    xl6_error_result outcome;
    int64_t value;
    assert(xl6ErrorScopeBegin(&scope));
    value = XLIR_GENERATOR_ITEM_ENTRY();
    outcome = xl6ErrorScopeEnd(&scope);
    assert(outcome.ok && !outcome.error && value == XLIR_GENERATOR_ITEM_EXPECTED);
}

void generator_item_basic_finish(void)
{
    xlir_module_fini();
    assert(!xrtGetError());
}

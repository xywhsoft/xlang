#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "xlir_generator_storage.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
#ifndef XLIR_GENERATOR_STORAGE_BINDING
#define XLIR_GENERATOR_STORAGE_BINDING "xlir_generator_storage.bind.h"
#endif
#include XLIR_GENERATOR_STORAGE_BINDING
#include <assert.h>
void* generator_storage_create(void) { return GENERATOR_STORAGE_START(); }
int generator_storage_next(void* data)
{
    xlir_generator_step step=xlir_generator_next((xlir_generator*)data);
    return step.ok ? (step.value ? 1 : 0) : -1;
}
int64_t generator_storage_get(void* data)
{
    int64_t item=0;
    assert(xlir_generator_item_get((xlir_generator*)data,&item,sizeof(item),false)); return item;
}
xrtownershipref generator_storage_ownership(const void* data)
{ return xlir_generator_ownership((const xlir_generator*)data); }
void generator_storage_destroy(void* data) { xlir_generator_destroy((xlir_generator*)data); }

#include <stdlib.h>
#include <stdint.h>
void* compiled_demo_counter_create(int64_t value)
{
    int64_t* result = (int64_t*)malloc(sizeof(*result));
    if (result) *result = value;
    return result;
}
void compiled_demo_counter_destroy(void* value) { free(value); }
int64_t compiled_demo_counter_get(void* value) { return *(int64_t*)value; }
int64_t compiled_demo_counter_add(void* value, int64_t delta)
{
    return (*(int64_t*)value += delta);
}
int64_t compiled_demo_scale(int64_t value) { return value * 2; }
#define XRT_MODULE_MEMORY
#define XRT_MODULE_STRING
#include "lib/xrt/xrt.h"
char* compiled_demo_owned_text(void) { return xrtStrDup("owned native text"); }
int64_t compiled_demo_apply_raw(int64_t (*operation)(int64_t),int64_t value) { return operation(value); }

#include <assert.h>
#include "xlir_cell_thread_lifetime.c"
#include "xlir_cell_thread_lifetime.h.bind.h"
void cell_thread_prepare(void) { assert(xlir_module_init() && cell_thread_semantics() == 8 && !xrtGetError()); }
void cell_thread_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
void* cell_thread_raw_new(void) { return (xl6CellRuntimeV1Get()->Create)(XL6_CELL_I64,sizeof(int64_t)); }
void cell_thread_raw_retain(void* value) { assert(xlir_cell_retain((xlir_cell*)value) == value); }
void cell_thread_raw_release(void* value) { xlir_cell_release((xlir_cell*)value); }
int32_t cell_thread_raw_count(void* value) { size_t count=0; xrtownershipref ref=xlir_cell_ownership(value); assert(ref.Ops->Count(ref.Data,&count)); return (int32_t)count; }
void* cell_thread_factory(void)
{
    xlir_result_closure result = CELL_THREAD_FACTORY();
    assert(result.ok && result.value && !xrtGetError()); return result.value;
}
void* cell_thread_reader(void* value)
{
    xlir_closure* factory = (xlir_closure*)value; xlir_closure* reader = NULL;
    assert(((bool (*)(void*, xlir_closure**))factory->invoke)(factory->environment, &reader));
    assert(reader && !xrtGetError()); return reader;
}
void cell_thread_read(void* value)
{
    xlir_closure* reader = (xlir_closure*)value; int64_t answer = 0;
    assert(((bool (*)(void*, int64_t*))reader->invoke)(reader->environment, &answer));
    assert(answer == 73 && !xrtGetError());
}
void cell_thread_release(void* value) { xlir_closure_release((xlir_closure*)value); assert(!xrtGetError()); }

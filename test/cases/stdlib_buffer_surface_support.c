/* A genuine provider call: transfer one newly allocated native buffer to the
 * language constructor. Never adopt a borrowed language Buffer handle. */
#include "__xl_runtime/xl6_text_value.h"
void* stdlib_surface_fresh_buffer(void)
{
    return xl6BufferCreateV1();
}

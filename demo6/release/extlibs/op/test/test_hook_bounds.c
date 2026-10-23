#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "../hooks/op_hook_bounds.h"

int main(void)
{
    const size_t header = 20u;
    assert(op_hook_frame_fits(1u, 1u, OP_HOOK_MAP_SIZE, header));
    assert(op_hook_frame_fits(1024u, 1023u, OP_HOOK_MAP_SIZE, header));
    assert(op_hook_frame_fits(1024u, 1024u, OP_HOOK_MAP_SIZE, header));
    assert(op_hook_frame_fits(3840u, 2160u, OP_HOOK_MAP_SIZE, header));
    assert(op_hook_frame_fits(4096u, 4096u, OP_HOOK_MAP_SIZE, header));
    assert(!op_hook_frame_fits(4097u, 4096u, OP_HOOK_MAP_SIZE, header));
    assert(!op_hook_frame_fits(0u, 1u, OP_HOOK_MAP_SIZE, header));
    assert(!op_hook_frame_fits(1u, 0u, OP_HOOK_MAP_SIZE, header));
    assert(!op_hook_frame_fits(1u, 1u, 19u, header));
    assert(!op_hook_frame_fits(~0u, 2u, OP_HOOK_MAP_SIZE, header));
    puts("op hook bounds: PASS");
    return 0;
}

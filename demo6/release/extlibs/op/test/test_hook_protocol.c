#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../hooks/op_hook_shared.h"

#define TEST_WIDTH 64u
#define TEST_HEIGHT 64u
#define TEST_BYTES (TEST_WIDTH * TEST_HEIGHT * 4u)

typedef struct test_state {
    unsigned char *map;
    unsigned char first[TEST_BYTES];
    unsigned char second[TEST_BYTES];
} test_state;

static DWORD WINAPI write_frames(void *opaque)
{
    test_state *state = (test_state *)opaque;
    unsigned int i;
    for (i = 0u; i < 10000u; ++i) {
        op_hook_write_frame(state->map, TEST_WIDTH, TEST_HEIGHT,
            (i & 1u) != 0u ? state->first : state->second, TEST_WIDTH * 4u);
    }
    return 0u;
}

int main(void)
{
    test_state *state = (test_state *)calloc(1u, sizeof(*state));
    unsigned char *copy = (unsigned char *)malloc(TEST_BYTES);
    HANDLE writer;
    unsigned int w = 0u;
    unsigned int h = 0u;
    unsigned int i;
    unsigned int successful = 0u;
    assert(state != NULL && copy != NULL);
    state->map = (unsigned char *)calloc(1u, sizeof(op_hook_header) + TEST_BYTES);
    assert(state->map != NULL);
    memset(state->first, 0x33, TEST_BYTES);
    memset(state->second, 0xcc, TEST_BYTES);

    /* 写入中和目标缓冲过短都不应暴露帧。 */
    op_hook_begin_frame((op_hook_header *)state->map, TEST_WIDTH, TEST_HEIGHT);
    assert(!op_hook_snapshot(state->map, copy, TEST_BYTES, &w, &h));
    memcpy(op_hook_pixels(state->map), state->first, TEST_BYTES);
    op_hook_end_frame((op_hook_header *)state->map);
    assert(!op_hook_snapshot(state->map, copy, TEST_BYTES - 1u, &w, &h));
    assert(op_hook_snapshot(state->map, copy, TEST_BYTES, &w, &h));
    assert(w == TEST_WIDTH && h == TEST_HEIGHT && copy[0] == 0x33);

    writer = CreateThread(NULL, 0u, write_frames, state, 0u, NULL);
    assert(writer != NULL);
    for (i = 0u; i < 10000u; ++i) {
        size_t j;
        unsigned char value;
        if (!op_hook_snapshot(state->map, copy, TEST_BYTES, &w, &h)) continue;
        assert(w == TEST_WIDTH && h == TEST_HEIGHT);
        value = copy[0];
        assert(value == 0x33 || value == 0xcc);
        for (j = 1u; j < TEST_BYTES; ++j) assert(copy[j] == value);
        ++successful;
    }
    assert(WaitForSingleObject(writer, 10000u) == WAIT_OBJECT_0);
    CloseHandle(writer);
    assert(successful != 0u);

    ((op_hook_header *)state->map)->width = 4097u;
    assert(!op_hook_snapshot(state->map, copy, TEST_BYTES, &w, &h));
    ((op_hook_header *)state->map)->width = TEST_WIDTH;
    ((op_hook_header *)state->map)->format = 1u;
    assert(!op_hook_snapshot(state->map, copy, TEST_BYTES, &w, &h));
    free(state->map);
    free(copy);
    free(state);
    puts("op hook protocol: PASS");
    return 0;
}

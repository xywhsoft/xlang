#include "../third_party/siphash.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "reference/vectors.h"
#include "reference/siphash.c"

int main(void)
{
    const uint8_t key[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    const uint8_t expectedEmpty[16] = {
        0xa3,0x81,0x7f,0x04,0xba,0x25,0xa8,0xe6,
        0x6d,0xf6,0x72,0x14,0xc7,0x55,0x02,0x93
    };
    const uint8_t expectedOne[16] = {
        0xda,0x87,0xc1,0xd8,0x6b,0x99,0xaf,0x44,
        0x34,0x76,0x59,0x11,0x9b,0x22,0xfc,0x45
    };
    siphash128_state state, copy;
    uint8_t digest[16];
    const uint8_t zero = 0;
    uint8_t input[256];
    uint8_t reference[16];
    size_t length, split, i;

    siphash128_init(&state, key);
    siphash128_final(&state, digest);
    assert(memcmp(digest, expectedEmpty, 16) == 0);
    siphash128_update(&state, &zero, 1);
    copy = state;
    siphash128_final(&state, digest);
    assert(memcmp(digest, expectedOne, 16) == 0);
    siphash128_final(&copy, digest);
    assert(memcmp(digest, expectedOne, 16) == 0);

    for (i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)i;
    for (length = 0; length < 64; ++length) {
        siphash128_init(&state, key);
        siphash128_update(&state, input, length);
        siphash128_final(&state, digest);
        assert(memcmp(digest, vectors_sip128[length], 16) == 0);
        for (split = 0; split <= length; ++split) {
            siphash128_init(&state, key);
            siphash128_update(&state, input, split);
            siphash128_update(&state, input + split, length - split);
            siphash128_final(&state, digest);
            assert(memcmp(digest, vectors_sip128[length], 16) == 0);
        }
    }
    for (length = 64; length <= sizeof(input); ++length) {
        assert(siphash(input, length, key, reference, 16) == 0);
        siphash128_init(&state, key);
        siphash128_update(&state, input, length);
        siphash128_final(&state, digest);
        assert(memcmp(digest, reference, 16) == 0);
    }
    puts("siphash128 core: PASS");
    return 0;
}

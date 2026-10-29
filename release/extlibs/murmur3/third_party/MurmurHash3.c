/* C port of Austin Appleby's public-domain MurmurHash3.
 * See README.md for provenance and local streaming changes. */
#include "MurmurHash3.h"
#include <string.h>

static uint32_t rotl32(uint32_t x, unsigned r) { return (x << r) | (x >> (32 - r)); }
static uint64_t rotl64(uint64_t x, unsigned r) { return (x << r) | (x >> (64 - r)); }

/* 明确小端装载，不要求调用方地址对齐，也不违反别名规则。 */
static uint32_t load32(const uint8_t* p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t load64(const uint8_t* p)
{ return (uint64_t)load32(p) | ((uint64_t)load32(p + 4) << 32); }
static void store64(uint8_t* p, uint64_t value)
{
    unsigned i;
    for (i = 0; i < 8; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16; h *= 0x85ebca6b;
    h ^= h >> 13; h *= 0xc2b2ae35;
    return h ^ (h >> 16);
}
static uint64_t fmix64(uint64_t h)
{
    h ^= h >> 33; h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33; h *= UINT64_C(0xc4ceb9fe1a85ec53);
    return h ^ (h >> 33);
}

// ----------------------------- x86_32 -----------------------------

static uint32_t mix32(uint32_t k)
{ return rotl32(k * 0xcc9e2d51u, 15) * 0x1b873593u; }
static void block32(murmur3_32_state* s, const uint8_t* p)
{ s->h1 = rotl32(s->h1 ^ mix32(load32(p)), 13) * 5 + 0xe6546b64u; }

void murmur3_32_init(murmur3_32_state* s, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->h1 = seed;
    s->seed = seed;
}
void murmur3_32_update(murmur3_32_state* s, const void* data, size_t size)
{
    const uint8_t* p = data;
    size_t take;
    s->total += size;
    if (size == 0) return;
    if (s->buflen != 0) {
        take = 4 - s->buflen;
        if (take > size) take = size;
        memcpy(s->buf + s->buflen, p, take);
        s->buflen += take; p += take; size -= take;
        if (s->buflen != 4) return;
        block32(s, s->buf);
        s->buflen = 0;
    }
    /* 完整块直接处理，只有尾部跨 update 保存。 */
    while (size >= 4) { block32(s, p); p += 4; size -= 4; }
    if (size != 0) memcpy(s->buf, p, size);
    s->buflen = size;
}
uint32_t murmur3_32_final(const murmur3_32_state* s)
{
    uint32_t tail = 0;
    size_t i;
    for (i = 0; i < s->buflen; ++i) tail |= (uint32_t)s->buf[i] << (i * 8);
    return fmix32(s->h1 ^ mix32(tail) ^ (uint32_t)s->total);
}
uint32_t murmur3_32(const void* data, size_t size, uint32_t seed)
{
    murmur3_32_state state;
    murmur3_32_init(&state, seed);
    murmur3_32_update(&state, data, size);
    return murmur3_32_final(&state);
}

// ----------------------------- x64_128 -----------------------------

static uint64_t mix128_1(uint64_t k)
{ return rotl64(k * UINT64_C(0x87c37b91114253d5), 31) * UINT64_C(0x4cf5ad432745937f); }
static uint64_t mix128_2(uint64_t k)
{ return rotl64(k * UINT64_C(0x4cf5ad432745937f), 33) * UINT64_C(0x87c37b91114253d5); }
static void block128(murmur3_128_state* s, const uint8_t* p)
{
    s->h1 ^= mix128_1(load64(p));
    s->h1 = (rotl64(s->h1, 27) + s->h2) * 5 + 0x52dce729u;
    s->h2 ^= mix128_2(load64(p + 8));
    s->h2 = (rotl64(s->h2, 31) + s->h1) * 5 + 0x38495ab5u;
}
void murmur3_128_init(murmur3_128_state* s, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->h1 = s->h2 = seed;
    s->seed = seed;
}
void murmur3_128_update(murmur3_128_state* s, const void* data, size_t size)
{
    const uint8_t* p = data;
    size_t take;
    s->total += size;
    if (size == 0) return;
    if (s->buflen != 0) {
        take = 16 - s->buflen;
        if (take > size) take = size;
        memcpy(s->buf + s->buflen, p, take);
        s->buflen += take; p += take; size -= take;
        if (s->buflen != 16) return;
        block128(s, s->buf);
        s->buflen = 0;
    }
    while (size >= 16) { block128(s, p); p += 16; size -= 16; }
    if (size != 0) memcpy(s->buf, p, size);
    s->buflen = size;
}
void murmur3_128_final(const murmur3_128_state* s, void* out)
{
    uint64_t h1 = s->h1, h2 = s->h2, k1 = 0, k2 = 0;
    size_t i;
    for (i = 0; i < s->buflen && i < 8; ++i) k1 |= (uint64_t)s->buf[i] << (i * 8);
    for (i = 8; i < s->buflen; ++i) k2 |= (uint64_t)s->buf[i] << ((i - 8) * 8);
    h1 ^= mix128_1(k1);
    h2 ^= mix128_2(k2);
    /* 流累计长度始终保持 64 位，不能经 size_t 在 32 位平台截断。 */
    h1 ^= s->total; h2 ^= s->total;
    h1 += h2; h2 += h1;
    h1 = fmix64(h1); h2 = fmix64(h2);
    h1 += h2; h2 += h1;
    store64(out, h1);
    store64((uint8_t*)out + 8, h2);
}
void murmur3_128(const void* data, size_t size, uint32_t seed, void* out)
{
    murmur3_128_state state;
    murmur3_128_init(&state, seed);
    murmur3_128_update(&state, data, size);
    murmur3_128_final(&state, out);
}

#include "siphash.h"
#include <string.h>

/* SipHash-2-4-128；来源与差分验证见 PROVENANCE.md。 */
static uint64_t load64(const uint8_t* p)
{
    uint64_t value = 0;
    unsigned i;
    for (i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (8 * i);
    return value;
}
static void store64(uint8_t* p, uint64_t value)
{
    unsigned i;
    for (i = 0; i < 8; ++i) p[i] = (uint8_t)(value >> (8 * i));
}
static uint64_t rotate(uint64_t x, unsigned n) { return (x << n) | (x >> (64 - n)); }
static void rounds(uint64_t v[4], unsigned count)
{
    while (count--) {
        v[0] += v[1]; v[1] = rotate(v[1], 13); v[1] ^= v[0]; v[0] = rotate(v[0], 32);
        v[2] += v[3]; v[3] = rotate(v[3], 16); v[3] ^= v[2];
        v[0] += v[3]; v[3] = rotate(v[3], 21); v[3] ^= v[0];
        v[2] += v[1]; v[1] = rotate(v[1], 17); v[1] ^= v[2]; v[2] = rotate(v[2], 32);
    }
}
static void block(uint64_t v[4], uint64_t word)
{
    v[3] ^= word; rounds(v, 2); v[0] ^= word;
}
void siphash128_init(siphash128_state* s, const uint8_t key[16])
{
    uint64_t a = load64(key), b = load64(key + 8);
    memset(s, 0, sizeof(*s));
    s->v[0] = a ^ UINT64_C(0x736f6d6570736575);
    s->v[1] = b ^ UINT64_C(0x646f72616e646f6d) ^ 0xee;
    s->v[2] = a ^ UINT64_C(0x6c7967656e657261);
    s->v[3] = b ^ UINT64_C(0x7465646279746573);
}
/* 只复制跨块尾部；完整八字节块直接读取，允许未对齐地址。 */
void siphash128_update(siphash128_state* s, const void* data, size_t size)
{
    const uint8_t* p = data;
    size_t count;
    s->total += size;
    if (s->used && size) {
        count = 8 - s->used;
        if (count > size) count = size;
        memcpy(s->tail + s->used, p, count);
        s->used += count; p += count; size -= count;
        if (s->used == 8) { block(s->v, load64(s->tail)); s->used = 0; }
    }
    while (size >= 8) { block(s->v, load64(p)); p += 8; size -= 8; }
    if (size) { memcpy(s->tail, p, size); s->used = size; }
}
/* 摘要是快照，调用后仍能追加数据；128 位域分离不能由两个 64 位摘要拼接。 */
void siphash128_final(const siphash128_state* s, uint8_t out[16])
{
    uint64_t v[4], tail = s->total << 56;
    size_t i;
    memcpy(v, s->v, sizeof(v));
    for (i = 0; i < s->used; ++i) tail |= (uint64_t)s->tail[i] << (8 * i);
    block(v, tail);
    v[2] ^= 0xee; rounds(v, 4);
    store64(out, v[0] ^ v[1] ^ v[2] ^ v[3]);
    v[1] ^= 0xdd; rounds(v, 4);
    store64(out + 8, v[0] ^ v[1] ^ v[2] ^ v[3]);
}

#ifndef XL_SIPHASH128_H
#define XL_SIPHASH128_H
#include <stddef.h>
#include <stdint.h>

/* 本地维护的 128 位增量引擎；64 位实现由 xlang 标准库使用 XRT。 */
typedef struct siphash128_state {
    uint64_t v[4], total;
    uint8_t tail[8];
    size_t used;
} siphash128_state;

void siphash128_init(siphash128_state* s, const uint8_t key[16]);
void siphash128_update(siphash128_state* s, const void* data, size_t size);
void siphash128_final(const siphash128_state* s, uint8_t out[16]);
#endif

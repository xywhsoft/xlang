/* 固定 64 位 XRT 哈希的 HyperLogLog；寄存器按精度独立分配。 */
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#include "xrt.h"
#include "include/hyperloglog_native.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#define XHLL_DOMAIN "xlang.extlib.hyperloglog"

typedef struct xhll_state {
    unsigned char* registers;
    unsigned int precision;
    unsigned int nregisters;
    uint64 added;
} xhll_state;

static void xhll_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XHLL_DOMAIN, 0, message);
}

/* 最高 p 位是寄存器索引；rho 只扫描剩余的 64-p 位。 */
static int xhll_rank(uint64 hash, unsigned int precision)
{
    uint64 remainder = hash << precision;
    int rank = 1;
    int limit = 64 - (int)precision;
    while (rank <= limit && (remainder & (UINT64_C(1) << 63)) == 0u) {
        remainder <<= 1;
        ++rank;
    }
    return rank;
}

void* xhll_create(int precision)
{
    xhll_state* state;
    unsigned int count;
    xrtClearError();
    if (precision < 4 || precision > 18) {
        xhll_error(XERR_RANGE, "precision must be between 4 and 18");
        return NULL;
    }
    count = 1u << precision;
    state = (xhll_state*)xrtCalloc(1u, sizeof(*state));
    if (state == NULL) return NULL;
    state->registers = (unsigned char*)xrtCalloc(count, 1u);
    if (state->registers == NULL) { xrtFree(state); return NULL; }
    state->precision = (unsigned int)precision;
    state->nregisters = count;
    return state;
}

void xhll_destroy(void* handle)
{
    xhll_state* state = (xhll_state*)handle;
    if (state == NULL) return;
    xrtFree(state->registers);
    xrtFree(state);
}

int xhll_add(void* handle, const char* data, long long length)
{
    xhll_state* state = (xhll_state*)handle;
    uint64 hash;
    unsigned int index;
    int rank;
    xrtClearError();
    if (state == NULL || data == NULL || length < -1) {
        xhll_error(XERR_ARGUMENT, "state, data or length is invalid");
        return 0;
    }
    if (length == -1) length = (long long)strlen(data);
    if ((uint64)length > SIZE_MAX) {
        xhll_error(XERR_RANGE, "item length exceeds address space");
        return 0;
    }
    hash = xrtHash64(data, (size_t)length);
    index = (unsigned int)(hash >> (64 - state->precision));
    rank = xhll_rank(hash, state->precision);
    if ((unsigned char)rank > state->registers[index])
        state->registers[index] = (unsigned char)rank;
    if (state->added < (uint64)LLONG_MAX) ++state->added;
    return 1;
}

int64_t xhll_count(void* handle)
{
    xhll_state* state = (xhll_state*)handle;
    double alpha, sum = 0.0, estimate;
    unsigned int count, i, zeros = 0u;
    xrtClearError();
    if (state == NULL) {
        xhll_error(XERR_ARGUMENT, "state is null");
        return -1;
    }
    count = state->nregisters;
    if (count == 16u) alpha = 0.673;
    else if (count == 32u) alpha = 0.697;
    else if (count == 64u) alpha = 0.709;
    else alpha = 0.7213 / (1.0 + 1.079 / (double)count);
    for (i = 0u; i < count; ++i) {
        sum += 1.0 / (double)(UINT64_C(1) << state->registers[i]);
        if (state->registers[i] == 0u) ++zeros;
    }
    estimate = alpha * (double)count * (double)count / sum;
    if (estimate <= 2.5 * (double)count && zeros != 0u)
        estimate = (double)count * log((double)count / (double)zeros);
    if (estimate >= (double)LLONG_MAX) return LLONG_MAX;
    return (int64_t)(estimate + 0.5);
}

int xhll_clear(void* handle)
{
    xhll_state* state = (xhll_state*)handle;
    xrtClearError();
    if (state == NULL) {
        xhll_error(XERR_ARGUMENT, "state is null");
        return 0;
    }
    memset(state->registers, 0, state->nregisters);
    state->added = 0u;
    return 1;
}

int xhll_merge(void* destination, void* source)
{
    xhll_state* dst = (xhll_state*)destination;
    xhll_state* src = (xhll_state*)source;
    unsigned int i;
    xrtClearError();
    if (dst == NULL || src == NULL) {
        xhll_error(XERR_ARGUMENT, "state is null");
        return 0;
    }
    if (dst->precision != src->precision) {
        xhll_error(XERR_VALUE, "cannot merge different precisions");
        return 0;
    }
    if (dst == src) return 1;
    for (i = 0u; i < dst->nregisters; ++i)
        if (src->registers[i] > dst->registers[i])
            dst->registers[i] = src->registers[i];
    dst->added = src->added > (uint64)LLONG_MAX - dst->added ?
        (uint64)LLONG_MAX : dst->added + src->added;
    return 1;
}

int xhll_precision(void* handle)
{
    xhll_state* state = (xhll_state*)handle;
    xrtClearError();
    if (state == NULL) { xhll_error(XERR_ARGUMENT, "state is null"); return -1; }
    return (int)state->precision;
}

int64_t xhll_added(void* handle)
{
    xhll_state* state = (xhll_state*)handle;
    xrtClearError();
    if (state == NULL) { xhll_error(XERR_ARGUMENT, "state is null"); return -1; }
    return (int64_t)state->added;
}

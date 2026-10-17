#ifndef XL6_ATOMIC_INTERNAL_H
#define XL6_ATOMIC_INTERNAL_H
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <string.h>

/* Validate before touching the object. XRT's C callers are trusted to pass a
 * legal order; the language facade accepts integers and enforces that contract. */
static inline bool xl6AtomicOrder(int64_t order, bool load, bool store)
{
    bool valid = order >= XMEMORY_RELAXED && order <= XMEMORY_SEQ_CST;
    if (load) valid = order == XMEMORY_RELAXED || order == XMEMORY_ACQUIRE || order == XMEMORY_SEQ_CST;
    if (store) valid = order == XMEMORY_RELAXED || order == XMEMORY_RELEASE || order == XMEMORY_SEQ_CST;
    if (!valid) xrtSetErrorKind(XERR_ARGUMENT);
    return valid;
}

static inline bool xl6AtomicCompareOrders(int64_t success, int64_t failure)
{
    if (!xl6AtomicOrder(success, false, false) || !xl6AtomicOrder(failure, true, false)) return false;
    bool valid = failure == XMEMORY_RELAXED ||
        (failure == XMEMORY_ACQUIRE && (success == XMEMORY_ACQUIRE ||
            success == XMEMORY_ACQ_REL || success == XMEMORY_SEQ_CST)) ||
        (failure == XMEMORY_SEQ_CST && success == XMEMORY_SEQ_CST);
    if (!valid) xrtSetErrorKind(XERR_ARGUMENT);
    return valid;
}

static inline bool xl6AtomicSigned32(int64_t value)
{
    if (value >= INT32_MIN && value <= INT32_MAX) return true;
    xrtSetErrorKind(XERR_RANGE);
    return false;
}

static inline bool xl6AtomicUnsigned32(int64_t value)
{
    if (value >= 0 && (uint64_t)value <= UINT32_MAX) return true;
    xrtSetErrorKind(XERR_RANGE);
    return false;
}

static inline int64_t xl6AtomicDecodeSigned32(uint32_t bits)
{ int32_t value; memcpy(&value, &bits, sizeof(value)); return value; }
static inline int64_t xl6AtomicDecodeSigned64(uint64_t bits)
{ int64_t value; memcpy(&value, &bits, sizeof(value)); return value; }
static inline bool xl6AtomicAllI64(int64_t value) { (void)value; return true; }
static inline bool xl6AtomicAllU64(uint64_t value) { (void)value; return true; }
static inline bool xl6AtomicAllBool(bool value) { (void)value; return true; }
#endif

#ifndef XL6_SLOT_H
#define XL6_SLOT_H
#include <stddef.h>
#include "xl6_code_lease.h"

/* Ordered physical writes from a verified IR storage/frame transfer plan.
 * Ownership, payload type, empty representation and activation bits are fixed
 * by the producer. This runtime never discovers owners or invokes callbacks.
 * Inputs observe preceding writes, exactly like the IR's ordered transfer.
 * Plan metadata must remain immutable and disjoint from every target. */
typedef struct XL6_SlotWrite {
    void* target;
    const void* input;
    size_t size;
} XL6_SlotWrite;
typedef struct XL6_SlotRuntimeV1 {
    /* Infallible, allocation-free, ordered commit under one mutation scope.
     * Payload + activation, source detach and destination publication can be
     * one plan. Use actual typed empty objects, never guessed null bit patterns.
     * Ordinary concurrent access still needs the language/host serialization
     * contract. This does not certify arbitrary graph children or code lifetime. */
    void (*Apply)(const XL6_SlotWrite*, size_t);
} XL6_SlotRuntimeV1;
XL6_CODE_LEASE_API const XL6_SlotRuntimeV1* xl6SlotRuntimeV1Get(void);
#endif

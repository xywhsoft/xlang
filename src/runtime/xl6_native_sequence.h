#ifndef XL6_NATIVE_SEQUENCE_H
#define XL6_NATIVE_SEQUENCE_H
#ifdef __cplusplus
extern "C" {
#endif
#include "xl6_foreign_view_abi.h"

/* Closed compiler metadata, copied into each owning range. Element storage
 * belongs to an independently retained native owner. The trusted producer
 * promises that data/count and every element remain stable, without storage
 * migration, for that owner's complete lifetime. Merely retaining a mutable
 * vector is insufficient. No element pointer or
 * mutable reference is exposed to the language. */
typedef struct XL6_NativeSequenceSchemaV1 {
    size_t size;
    uint64_t abiTag, sequenceIdentity, elementIdentity;
    size_t elementSize, elementAlignment, stride;
} XL6_NativeSequenceSchemaV1;
#define XL6_NATIVE_SEQUENCE_SCHEMA_V1_TAG UINT64_C(0x584C364E53513131)

/* A producer reserves all wrappers and the code lease BEFORE Native Retain.
 * Span validates the complete byte extent without reading any element.
 * Adopt is called whenever the typed retainer actually acquired one owner,
 * even if a separate checked callback failure has already escaped. */
typedef struct XL6_NativeSequenceResultV1 {
    struct XL6_NativeSequenceResultV1* self;
    uint64_t thread;
    XL6_ForeignResultProbeV1 owner;
    void* backing;
    void* range;
    struct xvalue* value;
    bool spanReady;
} XL6_NativeSequenceResultV1;
XL6_CODE_LEASE_API bool xl6NativeSequenceResultReserveV1(
    XL6_NativeSequenceResultV1*,const XL6_NativeSequenceSchemaV1*,
    const XL6_ForeignViewDescriptorV1*,const XL6_OwnershipDomainV1*,const XL6_CodeOwner*);
XL6_CODE_LEASE_API bool xl6NativeSequenceResultSpanV1(
    XL6_NativeSequenceResultV1*,const void* data,uint64_t count);
XL6_CODE_LEASE_API bool xl6NativeSequenceResultAdoptV1(XL6_NativeSequenceResultV1*,const void* nativeOwner);
XL6_CODE_LEASE_API struct xvalue* xl6NativeSequenceResultTakeV1(XL6_NativeSequenceResultV1*);
XL6_CODE_LEASE_API bool xl6NativeSequenceResultEndV1(XL6_NativeSequenceResultV1*);

XL6_CODE_LEASE_API struct xvalue* xl6NativeSequenceEmptyV1(const XL6_NativeSequenceSchemaV1*);
XL6_CODE_LEASE_API bool xl6NativeSequenceAdmitsV1(const struct xvalue*,uint64_t sequence,uint64_t element);
XL6_CODE_LEASE_API bool xl6NativeSequenceLengthV1(const struct xvalue*,uint64_t sequence,uint64_t element,int64_t*);
XL6_CODE_LEASE_API struct xvalue* xl6NativeSequenceSliceV1(
    const struct xvalue*,uint64_t sequence,uint64_t element,int64_t start,int64_t count);

/* One internal read scope protects the WHOLE typed traversal. The generated
 * loop uses const T* / stride directly, without per-element boxing/dispatch.
 * The span cannot escape, suspend, or be copied while live. Source and closed
 * IR must reject await/yield while this read scope is active. A single indexed
 * read uses the same guard and copies one independent trivial C value. */
typedef struct XL6_NativeSequenceReadV1 {
    struct XL6_NativeSequenceReadV1* self;
    XL6_ForeignViewCallV1 call;
    const void* data;
    size_t count, stride;
    uint64_t thread;
} XL6_NativeSequenceReadV1;
XL6_CODE_LEASE_API bool xl6NativeSequenceReadBeginV1(
    const struct xvalue*,const XL6_NativeSequenceSchemaV1*,XL6_NativeSequenceReadV1*);
XL6_CODE_LEASE_API bool xl6NativeSequenceReadEndV1(XL6_NativeSequenceReadV1*);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainNativeSequenceV1Get(void);
#ifdef __cplusplus
}
#endif
#endif

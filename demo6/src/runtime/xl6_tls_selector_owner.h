#ifndef XL6_TLS_SELECTOR_OWNER_H
#define XL6_TLS_SELECTOR_OWNER_H
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
/* Only the trusted typed-selector producer installs these permanent resident
 * operations. data is one real counted closure owner, never borrowed module
 * userdata. Base role/session units need not link selector implementations. */
typedef struct XL6_TlsSelectorOpsV1 {
    void* (*Ref)(void*);
    void (*Drop)(void*);
    xrtownershipref (*Ownership)(const void*);
    xtlsserverselectproc Select;
} XL6_TlsSelectorOpsV1;
typedef struct XL6_TlsSelectorV1 {void* data;const XL6_TlsSelectorOpsV1* ops;} XL6_TlsSelectorV1;
static inline XL6_TlsSelectorV1 xl6TlsSelectorRefV1(XL6_TlsSelectorV1 source)
{return source.data?(XL6_TlsSelectorV1){source.ops->Ref(source.data),source.ops}:(XL6_TlsSelectorV1){0};}
static inline void xl6TlsSelectorDropV1(XL6_TlsSelectorV1* slot)
{XL6_TlsSelectorV1 owned=*slot;*slot=(XL6_TlsSelectorV1){0};if(owned.data)owned.ops->Drop(owned.data);}
#endif

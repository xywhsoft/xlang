#ifndef XL6_TLS_RESUME_LOOKUP_OWNER_H
#define XL6_TLS_RESUME_LOOKUP_OWNER_H
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#endif
/* Only the typed producer installs these permanent operations. data is an
 * actual counted closure owner and an explicit graph edge, not module userdata.
 * Base Context/Session/Listener providers need not link the resume overlay. */
typedef struct XL6_TlsResumeLookupOpsV1 {
    void* (*Ref)(void*);
    void (*Drop)(void*);
    xrtownershipref (*Ownership)(const void*);
    xtlsserverresumeownedprocV1 Lookup;
} XL6_TlsResumeLookupOpsV1;
typedef struct XL6_TlsResumeLookupV1 {void* data;const XL6_TlsResumeLookupOpsV1* ops;} XL6_TlsResumeLookupV1;
static inline XL6_TlsResumeLookupV1 xl6TlsResumeLookupRefV1(XL6_TlsResumeLookupV1 source)
{return source.data?(XL6_TlsResumeLookupV1){source.ops->Ref(source.data),source.ops}:(XL6_TlsResumeLookupV1){0};}
static inline void xl6TlsResumeLookupDropV1(XL6_TlsResumeLookupV1* slot)
{XL6_TlsResumeLookupV1 owned=*slot;*slot=(XL6_TlsResumeLookupV1){0};if(owned.data)owned.ops->Drop(owned.data);}
#endif

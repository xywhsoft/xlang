#ifndef XL6_FOREIGN_SCOPE_VALUE_H
#define XL6_FOREIGN_SCOPE_VALUE_H
#include "xl6_foreign_scope_view.h"
#include "xl6_capture_v2.h"

typedef struct XL6_NullableForeignScopeViewV1 {
    bool hasValue;
    XL6_ForeignScopeViewV1 value;
} XL6_NullableForeignScopeViewV1;
typedef struct xlir_result_foreign_scope_view_v1 {bool ok;XL6_ForeignScopeViewV1 value;} xlir_result_foreign_scope_view_v1;
typedef struct xlir_result_nullable_foreign_scope_view_v1 {bool ok;XL6_NullableForeignScopeViewV1 value;} xlir_result_nullable_foreign_scope_view_v1;
struct xvalue;
struct xvalueconverter;
struct XL6_SequenceAdmissionOpsV2;

#define XL6_SCOPE_VALUE_API(Name,Type) \
    XL6_CODE_LEASE_API bool xl6##Name##CopyV1(Type,Type*); \
    XL6_CODE_LEASE_API void xl6##Name##DropV1(Type); \
    XL6_CODE_LEASE_API bool xl6##Name##EqualV1(Type,Type); \
    XL6_CODE_LEASE_API struct xvalue* xl6##Name##BoxV1(Type,uint64_t); \
    XL6_CODE_LEASE_API bool xl6##Name##UnboxV1(const struct xvalue*,uint64_t,bool,Type*); \
    XL6_CODE_LEASE_API bool xl6##Name##TryUnboxV1(const struct xvalue*,uint64_t,Type*); \
    XL6_CODE_LEASE_API const xrttype* xl6##Name##TypeV1(void); \
    XL6_CODE_LEASE_API const XL6_NativeCaptureProfileV1* xl6##Name##CaptureV1(void); \
    XL6_CODE_LEASE_API const struct xvalueconverter* xl6##Name##ConverterV1(void);
XL6_SCOPE_VALUE_API(ScopeView,XL6_ForeignScopeViewV1)
XL6_SCOPE_VALUE_API(NullableScopeView,XL6_NullableForeignScopeViewV1)
#undef XL6_SCOPE_VALUE_API
/* These copies preserve expired values. Only native ACCESS resolves their
 * callback ID; saving/boxing a token never resurrects or pins its payload. */
XL6_CODE_LEASE_API struct xvalue* xl6ScopeViewSequenceCreateV1(unsigned int,uint64_t,uint64_t,uint64_t,bool);
XL6_CODE_LEASE_API bool xl6ArrayExtractScopeViewV1(const struct xvalue*,uint64_t,uint64_t,int64_t,bool,uint64_t,bool,void*);
XL6_CODE_LEASE_API const struct XL6_SequenceAdmissionOpsV2* xl6ScopeViewSequenceAdmissionV1(void);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainSequenceScopeViewV1Get(void);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainScopeViewValueV1Get(void);
#endif

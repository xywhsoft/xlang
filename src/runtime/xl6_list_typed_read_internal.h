#ifndef XL6_LIST_TYPED_READ_INTERNAL_H
#define XL6_LIST_TYPED_READ_INTERNAL_H
#include "xl6_list.h"
#include "src/api/xl_foreign_error.h"
/* -1 failure, 0 borrowed wire item, 1 copied native slot. No foreign HANDLE
 * dereference, guessed tuple, whole-list projection or metadata allocation. */
static inline int xl6ListTypedReadSlot(const xvalue* source,uint64_t identity,
    uint64_t schema,uint64_t element,int64_t key,unsigned transfer,size_t size,
    const xrttype* type,void* output,const xvalue** wire)
{
    if (!source || !identity || !schema || !element) {
        xrtSetErrorKind(XERR_ARGUMENT); return -1;
    }
    int native=xl6ListErasedProbeV1(source);
    if (native<0) return -1;
    if (native) {
        const XL6_ListLayoutV1 layout={identity,{schema,element,transfer,size,type}};
        bool found=false;
        if (!xl6ListGetV1(source,&layout,key,output,false,&found)) return -1;
        if (!found) { xrtSetErrorKind(XERR_RANGE); return -1; }
        return 1;
    }
    if (xrtValueType(source)!=XVALUE_INT_MAP) { xrtSetErrorKind(XERR_TYPE); return -1; }
    *wire=xrtValueIntMapGet(source,key);
    if (!*wire) { if (!xrtGetError()) xrtSetErrorKind(XERR_RANGE); return -1; }
    return 0;
}
static inline bool xl6ListTypedReadFinish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
#endif
